#include "clickedit.h"
#include "window.h"
#include <QDateTime>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProgressBar>
#include <QSaveFile>
#include <QSet>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <cmath>
#include <functional>
#include <memory>

bool Window::saveProfile(const QString &path) {
  if (applying || readsPending)
    return false;
  QJsonArray values;
  int known = 0;
  for (const auto &p : parameters) {
    if (p.known)
      ++known;
    if (!p.known && !p.dirty)
      continue;
    const qint64 raw =
        p.dirty && p.editor ? qRound64(p.editor->value() * p.scale) : p.raw;
    values.append(QJsonObject{{"query", "~" + p.suffix},
                              {"raw", double(raw)},
                              {"unit", p.unit},
                              {"scale", p.scale},
                              {"draft", p.dirty},
                              {"readable", p.known}});
  }
  if (values.isEmpty())
    return false;
  const QString model = session.connected()
                            ? Protocol::supportedModel(session.model())
                        : profileModel.isEmpty() ? offlineModel->currentText()
                                                 : profileModel;
  if (model.isEmpty())
    return false;
  QJsonObject obj{
      {"application", "MotorLab Linux"},
      {"version", "0.6.0"},
      {"schema", 1},
      {"scope", "catalog_configuration_profile"},
      {"source", session.connected() ? (session.isDemo() ? "demo" : "serial")
                                     : profileSource},
      {"model", model},
      {"firmware", session.connected() ? session.firmware() : profileFirmware},
      {"session_connected", session.connected()},
      {"timestamp_utc",
       QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
      {"catalog_fields", parameters.size()},
      {"known_fields", known},
      {"complete_catalog_read", known == parameters.size()},
      {"parameters", values}};
  // Completeness is relative to the documented catalog, not a proprietary
  // controller dump.
  const auto bytes = QJsonDocument(obj).toJson();
  QSaveFile f(path);
  return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size() &&
         f.commit();
}
bool Window::loadProfile(const QString &path, QString *error) {
  auto bad = [&](const QString &message) {
    if (error)
      *error = message;
    return false;
  };
  if (applying || readsPending || session.streaming())
    return bad("Encerre a operação atual antes de importar.");
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
    return bad("Perfil inacessível ou maior que 1 MiB.");
  QJsonParseError parse;
  const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
  if (parse.error != QJsonParseError::NoError || !doc.isObject())
    return bad("JSON inválido.");
  const auto obj = doc.object();
  const auto scope = obj["scope"].toString();
  if (obj["application"].toString() != "MotorLab Linux" ||
      (scope != "catalog_configuration_profile" &&
       scope != "partial_parameter_snapshot"))
    return bad("Formato de perfil não reconhecido.");
  if (scope == "catalog_configuration_profile" && obj["schema"].toInt() != 1)
    return bad("Versão de esquema não suportada.");
  const auto model = Protocol::supportedModel(obj["model"].toString());
  if (model.isEmpty())
    return bad("Modelo do perfil não suportado.");
  const QString expected = session.connected()
                               ? Protocol::supportedModel(session.model())
                               : offlineModel->currentText();
  if (expected != model)
    return bad("O perfil pertence a outro modelo. Selecione o modelo correto "
               "offline ou conecte a unidade correspondente.");
  if (session.connected() && !session.isDemo() &&
      obj["source"].toString() == "demo")
    return bad("Perfil simulado não pode ser aplicado a um controlador real.");
  if (session.connected() && !session.isDemo() &&
      !obj["firmware"].toString().isEmpty() &&
      obj["firmware"].toString() != session.firmware())
    return bad("Firmware do perfil diferente do firmware conectado.");
  if (!obj["parameters"].isArray() || obj["parameters"].toArray().isEmpty())
    return bad("Perfil sem parâmetros.");
  QVector<QPair<int, qint64>> staged;
  QSet<QString> seen;
  for (const auto &v : obj["parameters"].toArray()) {
    if (!v.isObject())
      return bad("Entrada de parâmetro inválida.");
    const auto entry = v.toObject();
    const auto query = entry["query"].toString();
    const auto cmd = Protocol::command(query);
    if (!cmd.valid || !query.startsWith('~') || seen.contains(query) ||
        !entry["raw"].isDouble())
      return bad("Consulta inválida ou duplicada no perfil.");
    seen.insert(query);
    const double d = entry["raw"].toDouble();
    if (!std::isfinite(d) || std::floor(d) != d || d < -2147483648.0 ||
        d > 4294967295.0)
      return bad("Valor bruto deve ser um inteiro de 32 bits.");
    int row = -1;
    for (int i = 0; i < parameters.size(); ++i)
      if ("~" + parameters[i].suffix == query) {
        row = i;
        break;
      }
    if (row < 0)
      return bad("Parâmetro desconhecido: " + query);
    const auto &p = parameters[row];
    const qint64 raw = qint64(d);
    if (raw < p.spec->minimum || raw > p.spec->maximum)
      return bad("Valor fora do limite para " + query);
    if (p.spec->editable && !Catalog::validValue(*p.spec, raw, p.channel))
      return bad("Combinação de índice e valor inválida.");
    staged.append({row, raw});
  }
  // Commit locally only after every entry passes. Import never sends a command.
  for (const auto &value : staged) {
    auto &p = parameters[value.first];
    if (!p.editor)
      continue;
    const QSignalBlocker blocker(p.editor);
    p.editor->setValue(value.second / p.scale);
    p.dirty = !p.known || p.raw != value.second;
    parameterItems[value.first]->setText(0, p.name + " · " + p.suffix +
                                                (p.dirty ? " *" : ""));
  }
  profileModel = model;
  profileFirmware = obj["firmware"].toString();
  profileSource = obj["source"].toString();
  refreshState();
  return true;
}

void Window::maintenance(const QString &cmd, const QString &title,
                         const QString &description) {
  if (!session.writesEnabled() || applying || readsPending ||
      session.uploading()) {
    notify("Habilite a edição e aguarde a operação atual.");
    return;
  }
  if (QMessageBox::question(this, title,
                            description +
                                "\n\nO script será pausado e a parada !EX será "
                                "aplicada antes da operação. Confirmar?",
                            QMessageBox::Yes | QMessageBox::Cancel,
                            QMessageBox::Cancel) != QMessageBox::Yes)
    return;
  session.stopStream();
  session.cancelMotion();
  applying = true;
  operationTarget = cmd;
  operationPhase = 0;
  operationExpected = "!R 0";
  scanActive = false;
  refreshState();
  if (!session.enqueue(operationExpected))
    completeApply(false, "Não foi possível iniciar a operação.");
}
void Window::maintenanceReply(const QString &cmd, const QString &payload,
                              bool ok) {
  if (cmd != operationExpected)
    return;
  // A rejected pause is tolerated only for RAM application / EEPROM save,
  // followed by EX acknowledgement and fresh FF/FS checks before writing.
  const bool guardedConfiguration = operationTarget == "APPLY_RAM" ||
                                    operationTarget == "%EESAV";
  const bool pauseAttempt = guardedConfiguration &&
                            operationPhase == 0 && cmd == "!R 0";
  if (!ok && !pauseAttempt) {
    operationExpected.clear();
    operationTarget.clear();
    completeApply(false,
                  "Controlador rejeitou a operação. Releia a configuração.");
    return;
  }
  if (operationPhase == 0) {
    operationPhase = 1;
    operationExpected = "!EX";
  } else if (operationPhase == 1) {
    operationPhase = 2;
    operationExpected = "?FF";
  } else if (operationPhase == 2) {
    const auto reply = Protocol::reply("FF=" + payload);
    if (!reply.numeric || reply.numbers.size() != 1 || reply.numbers[0] < 0 ||
        reply.numbers[0] > 65535 || !(reply.numbers[0] & 16)) {
      operationExpected.clear();
      operationTarget.clear();
      completeApply(
          false,
          "Parada de emergência não confirmada por ?FF. Operação cancelada.");
      return;
    }
    operationPhase = 3;
    operationExpected = operationTarget;
    if (guardedConfiguration) {
      operationPhase = 4;
      operationExpected = "?FS";
    }
    if (operationTarget == "UPLOAD_HEX") {
      operationExpected.clear();
      operationTarget.clear();
      if (session.busy()) {
        QTimer::singleShot(30, this, [this] {
          if (!session.uploadScript(pendingHex))
            completeApply(false,
                          "Carregador ocupado. Transferência não iniciada.");
        });
      } else if (!session.uploadScript(pendingHex))
        completeApply(false, "Não foi possível iniciar a transferência HEX.");
      return;
    }
  } else if (operationPhase == 4 && guardedConfiguration) {
    const auto reply = Protocol::reply("FS=" + payload);
    if (!reply.numeric || reply.numbers.size() != 1 || reply.numbers[0] < 0 ||
        reply.numbers[0] > 65535 || (reply.numbers[0] & (128 | 256))) {
      completeApply(false,
                    "Script/setup não confirmado inativo por ?FS. "
                    "Configuração não enviada.");
      return;
    }
    if (operationTarget == "APPLY_RAM") {
      operationExpected.clear();
      operationTarget.clear();
      applyNext();
      return;
    }
    operationPhase = 5;
    operationExpected = operationTarget;
  } else {
    const QString done = operationTarget;
    operationExpected.clear();
    operationTarget.clear();
    applying = false;
    refreshState();
    notify("Operação confirmada. A parada permanece aplicada.");
    if (done.startsWith("%EELD") || done.startsWith("%EERST")) {
      for (auto &p : parameters)
        p.dirty = false;
      readParameters();
    }
    if (done.startsWith("%LK") || done.startsWith("%UK"))
      session.enqueue("?LK");
    return;
  }
  if (!session.enqueue(operationExpected)) {
    operationExpected.clear();
    operationTarget.clear();
    completeApply(false, "Não foi possível enviar a próxima etapa.");
  }
}
void Window::runtime(const QString &cmd, const QString &description) {
  const auto c = Protocol::command(cmd);
  if (!c.valid)
    return;
  if (c.motion && !session.motionEnabled()) {
    notify("Libere os comandos de motor / script na aba Monitoramento.");
    return;
  }
  if (c.motion && QMessageBox::question(
                      this, "Confirmar comando",
                      description + "\n\nControlador: " + session.model() +
                          "\nComando: " + cmd,
                      QMessageBox::Yes | QMessageBox::Cancel,
                      QMessageBox::Cancel) != QMessageBox::Yes)
    return;
  if (!session.enqueue(cmd))
    notify("Comando indisponível neste estado.");
}
void Window::scriptUpload() {
  if (!session.writesEnabled() || applying || readsPending) {
    notify("Habilite a edição e aguarde a leitura.");
    return;
  }
  const auto path = QFileDialog::getOpenFileName(
      this, "Enviar bytecode MicroBasic já compilado", QString(),
      "Script compilado (*.hex)");
  if (path.isEmpty())
    return;
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly) || f.size() > 256 * 1024) {
    notify("Arquivo HEX inacessível ou muito grande.");
    return;
  }
  pendingHex = HexFile::parse(f.readAll());
  if (!pendingHex.valid()) {
    notify(pendingHex.error);
    return;
  }
  const QString description =
      QString("Substituir o script em Flash pelo HEX selecionado (%1 "
              "bytes).\nO HEX deve ter sido compilado para este controlador; "
              "checksum não prova compatibilidade do bytecode.")
          .arg(pendingHex.bytes.size());
  maintenance("UPLOAD_HEX", "Transferir MicroBasic", description);
}
void Window::toggleStreaming() {
  if (session.streaming()) {
    session.stopStream();
    notify("Captura contínua encerrada.");
    return;
  }
  if (!session.connected() || readsPending || applying)
    return;
  pollTimer.stop();
  startStreamingWhenIdle(30);
}
void Window::startStreamingWhenIdle(int remaining) {
  if (!session.connected()) {
    pollTimer.start();
    return;
  }
  if (session.busy() && remaining > 0) {
    QTimer::singleShot(
        50, this, [this, remaining] { startStreamingWhenIdle(remaining - 1); });
    return;
  }
  const bool ok = session.startStream(streamRate->currentData().toInt());
  pollTimer.start();
  notify(ok ? "Captura contínua ativa. Taxa efetiva depende do controlador e "
              "transporte."
            : "Captura indisponível: aguarde a operação; serial física exige "
              "20 ms ou mais.");
}

void Window::onStream(const QString &payload) {
  const auto r = Protocol::reply("ML=" + payload);
  if (!r.numeric || r.numbers.size() != 12) {
    session.stopStream();
    notify("Quadro de captura inválido; captura encerrada.");
    return;
  }
  readings.clear(); // Inputs absent from this frame must not appear live.
  const auto p = payload.split(':');
  readings["?A"] = p[0] + ":" + p[1];
  readings["?S"] = p[2] + ":" + p[3];
  readings["?V 2"] = p[4];
  readings["?T 2"] = p[5];
  readings["?C"] = p[6] + ":" + p[7];
  readings["?DI"] = p[8];
  readings["?DO"] = p[9];
  readings["?FF"] = p[10];
  readings["?FS"] = p[11];
  updateSamples();
}
void Window::showSensorSetup() {
  QDialog dialog(this);
  dialog.setWindowTitle("Motor / sensores BLDC");
  auto layout = new QVBoxLayout(&dialog);
  auto info = new QLabel(
      "Configuração de sensores pelo procedimento do controlador (%CLMOD).\nA "
      "rotina movimenta o motor e publica o resultado no log.\nRevise BMOD, "
      "BPOL, BFBK, BZPW e os ganhos antes de iniciar.");
  info->setWordWrap(true);
  layout->addWidget(info);
  auto channel = new ClickEdit<QComboBox>;
  channel->addItems({"Motor 1", "Motor 2"});
  layout->addWidget(channel);
  auto start = new QPushButton("Iniciar ajuste motor / sensor");
  auto stop = new QPushButton("Cancelar ajuste");
  auto close = new QPushButton("Fechar");
  layout->addWidget(start);
  layout->addWidget(stop);
  layout->addWidget(close);
  connect(start, &QPushButton::clicked, &dialog, [this, channel] {
    runtime(QString("%CLMOD %1").arg(channel->currentIndex() + 2),
            "Executar o ajuste motor/sensor. O eixo irá girar; o controlador "
            "pode modificar calibrações.");
  });
  connect(stop, &QPushButton::clicked, &dialog, [this] {
    session.enqueue("%CLMOD 0");
    notify("Cancelamento do ajuste solicitado.");
  });
  connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
  dialog.exec();
}
void Window::saveLog() {
  const auto path = QFileDialog::getSaveFileName(
      this, "Salvar log", "motorlab-log.txt", "Texto (*.txt)");
  if (path.isEmpty())
    return;
  const auto bytes = console->toPlainText().toUtf8();
  QSaveFile f(path);
  notify(f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size() &&
                 f.commit()
             ? "Log salvo."
             : "Falha ao salvar log.");
}

void Window::executeDocumentedCommand(const QString &text) {
  const auto c = Protocol::command(text);
  if (!c.valid || c.hex || c.key == "SLD" || c.key == "LK" || c.key == "UK") {
    notify("Comando inválido ou reservado ao carregador / proteção de "
           "configuração.");
    return;
  }
  if (!c.write) {
    if (!session.enqueue(c.text))
      notify("Conexão indisponível.");
    return;
  }
  if (c.text.startsWith('^')) {
    if (!session.writesEnabled() || applying || readsPending) {
      notify("Habilite a edição e leia o parâmetro antes de escrever.");
      return;
    }
    for (int row = 0; row < parameters.size(); ++row) {
      auto &p = parameters[row];
      if (p.key != c.key || p.channel != c.channel)
        continue;
      if (!p.known || !p.editor) {
        notify("O parâmetro precisa ser lido e estar disponível para edição.");
        return;
      }
      p.editor->setValue(c.value / p.scale);
      selectPage(0);
      applyParameters();
      return;
    }
    return;
  }
  if (c.emergency) {
    if (applying)
      completeApply(
          false,
          "Operação cancelada pelo comando de parada; releia a configuração.");
    session.enqueue(c.text);
    return;
  }
  if (c.maintenance || c.key == "C" || c.key == "CB") {
    maintenance(c.text, "Comando documentado",
                "Executar no controlador identificado: " + c.text);
    return;
  }
  runtime(c.text,
          "Executar o comando documentado no controlador identificado.");
}
