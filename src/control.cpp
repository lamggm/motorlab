#include "control.h"
#include "cansdo.h"
#include "cpr.h"
#include "firmware.h"
#include "robocan.h"
#include "window.h"
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QProcess>
#include <QSaveFile>
#include <QSet>
#include <QSignalBlocker>
#include <QTemporaryFile>
#include <QUuid>
#include <cmath>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace {
void need(bool condition, const QString &message) {
  if (!condition)
    throw std::runtime_error(message.toStdString());
}
QString text(const QJsonObject &a, const char *key) {
  return a[key].toString();
}
QString now() {
  return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}
QByteArray readFile(const QString &path, int max = 1024 * 1024) {
  QFile f(path);
  need(QFileInfo(path).isFile() && f.open(QIODevice::ReadOnly) &&
           f.size() <= max,
       "Arquivo inacessível ou grande demais: " + path);
  return f.readAll();
}
void outputPath(const QString &path, bool overwrite = false) {
  need(QFileInfo(path).isAbsolute() && !QFileInfo(path).isSymLink(),
       "Use um caminho absoluto sem link simbólico no destino.");
  need(overwrite || !QFileInfo::exists(path),
       "Destino já existe; informe overwrite=true para substituir.");
}
bool writeFile(const QString &path, const QByteArray &data) {
  QSaveFile f(path);
  return f.open(QIODevice::WriteOnly) && f.write(data) == data.size() &&
         f.commit();
}
// Validate on the application side too: the Unix socket is a separate trust
// boundary.
void validate(const QJsonValue &v, const QJsonObject &s,
              const QString &path = "arguments") {
  const auto type = s["type"].toString();
  if (type == "object") {
    need(v.isObject(), path + " deve ser objeto.");
    const auto a = v.toObject(), props = s["properties"].toObject();
    for (auto r : s["required"].toArray())
      need(a.contains(r.toString()),
           path + "." + r.toString() + " obrigatório.");
    for (auto it = a.begin(); it != a.end(); ++it) {
      need(props.contains(it.key()), path + "." + it.key() + " desconhecido.");
      validate(it.value(), props[it.key()].toObject(), path + "." + it.key());
    }
  } else if (type == "array") {
    need(v.isArray() && v.toArray().size() <= s["maxItems"].toInt(1000),
         path + " lista inválida.");
    for (auto item : v.toArray())
      validate(item, s["items"].toObject(), path + "[]");
  } else if (type == "string") {
    need(v.isString() && v.toString().size() <= s["maxLength"].toInt(4096),
         path + " texto inválido.");
  } else if (type == "boolean")
    need(v.isBool(), path + " deve ser booleano.");
  else if (type == "integer") {
    const double n = v.toDouble();
    need(v.isDouble() && std::isfinite(n) && std::floor(n) == n &&
             n >= s["minimum"].toDouble() && n <= s["maximum"].toDouble(),
         path + " inteiro fora do limite.");
  }
  if (s.contains("enum"))
    need(s["enum"].toArray().contains(v), path + " opção desconhecida.");
}
} // namespace

ControlServer::ControlServer(Window &window) : QObject(&window), w(window) {
  QFile f(":/mcp-tools.json");
  if (!f.open(QIODevice::ReadOnly))
    qFatal("MCP schema resource missing");
  schemas = QJsonDocument::fromJson(f.readAll()).array();
  connect(&server, &QLocalServer::newConnection, this, &ControlServer::serve);
  tick.setInterval(15);
  connect(&tick, &QTimer::timeout, this, &ControlServer::next);
  supervision.setInterval(20);
  connect(&supervision, &QTimer::timeout, this, [this] {
    if (!continuous.enabled || continuous.stopping)
      return;
    if (continuous.lease.elapsed() >= continuous.leaseMs)
      continuousStop("Supervisão expirou; renovação não recebida.");
    else if (continuous.running && !w.session.motionEnabled())
      continuousStop("Permissão de movimento revogada.");
    else if (continuous.running && continuous.motorAck.isValid() &&
             continuous.motorAck.elapsed() >= w.session.watchdogMs() / 2)
      continuousStop("Comunicação atrasada; margem do watchdog atingida.");
    else if (continuous.running && continuous.encoderSpeed && continuous.encoderProgress.elapsed() >= continuous.noMotionMs)
      continuousStop("Sem progresso do encoder A/B; realimentação não confirmada.");
    else if (continuous.running && continuous.progress.elapsed() >= continuous.noMotionMs)
      continuousStop("Sem progresso do sensor interno; movimento não confirmado.");
  });
  deadline.setSingleShot(true);
  connect(&deadline, &QTimer::timeout, this, [this] {
    finish(false, "Tempo limite. Operação pode ter sido parcialmente aplicada; "
                  "releia o controlador.");
    w.session.close();
  });
  connect(&w.session, &Session::received, this,
          [this](QString cmd, QString payload, bool ok) {
            if (active.isEmpty() || waiting.isEmpty())
              return;
            if (cmd != waiting) {
              if (Protocol::command(cmd).emergency)
                finish(false,
                       "Operação interrompida por comando externo de parada.");
              return;
            }
            const auto step = steps[stepIndex];
            auto &j = jobs[active];
            auto replies = j["replies"].toArray();
            replies.append(QJsonObject{{"command", Protocol::redacted(cmd)},
                                       {"payload", payload},
                                       {"ok", ok}});
            if (continuous.enabled && replies.size() > 128)
              replies.removeAt(0);
            j["replies"] = replies;
            if (continuous.enabled && !continuous.stoppingSequence) {
              continuousObserve(cmd, payload, ok);
              if (continuous.stopping) {
                waiting.clear();
                tick.start();
                return;
              }
            }
            if (!ok && !tolerateReject && !step.pauseAttempt) {
              finish(false, "Controlador rejeitou " + Protocol::redacted(cmd));
              return;
            }
            if (!step.expected.isEmpty() && payload != step.expected) {
              finish(false, "Releitura diferente do valor solicitado.");
              return;
            }
            if (step.stop) {
              const auto r = Protocol::reply("FF=" + payload);
              if (!ok || !r.numeric || r.numbers.size() != 1 ||
                  r.numbers[0] < 0 || r.numbers[0] > 65535 ||
                  !(r.numbers[0] & 16)) {
                finish(false, "Parada !EX não confirmada por ?FF.");
                return;
              }
            }
            if (step.scriptIdle) {
              const auto r = Protocol::reply("FS=" + payload);
              if (!ok || !r.numeric || r.numbers.size() != 1 ||
                  r.numbers[0] < 0 || r.numbers[0] > 65535 ||
                  (r.numbers[0] & (128 | 256))) {
                finish(false, "Script/setup não confirmado inativo por ?FS; configuração não enviada.");
                return;
              }
            }
            waiting.clear();
            ++stepIndex;
            j["completed_steps"] = stepIndex;
          });
  connect(&w.session, &Session::failed, this, [this](const QString &message) {
    if (!active.isEmpty() && jobs[active]["kind"] != "firmware")
      finish(false, message);
  });
  connect(&w.session, &Session::uploadProgress, this,
          [this](int done, int total) {
            if (!active.isEmpty() && jobs[active]["kind"] == "upload_script") {
              jobs[active]["records_sent"] = done;
              jobs[active]["records_total"] = total;
            }
          });
  connect(&w.session, &Session::uploadFinished, this,
          [this](bool ok, const QString &msg) {
            if (!active.isEmpty() && jobs[active]["kind"] == "upload_script")
              finish(ok, msg);
          });
  connect(&w, &Window::firmwareUpdateFinished, this,
          [this](bool ok, const QString &msg) {
            if (!active.isEmpty() && jobs[active]["kind"] == "firmware")
              finish(ok, msg);
          });
}
bool ControlServer::listen(const QString &path) {
  // Never unlink another instance's endpoint. Launcher removes stale sockets
  // only under its startup lock.
  const auto parent = QFileInfo(path).absolutePath();
  struct stat st{};
  if (!QFileInfo(path).isAbsolute() ||
      ::lstat(QFile::encodeName(parent).constData(), &st) ||
      !S_ISDIR(st.st_mode) || st.st_uid != getuid() || (st.st_mode & 0077))
    return false;
  server.setSocketOptions(QLocalServer::UserAccessOption);
  return server.listen(path);
}
void ControlServer::serve() {
  while (server.hasPendingConnections()) {
    auto socket = server.nextPendingConnection();
    socket->setReadBufferSize(2 * 1024 * 1024 + 1);
    auto buffer = std::make_shared<QByteArray>();
    auto consumed = std::make_shared<bool>(false);
    connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    QTimer::singleShot(10000, socket,
                       [socket] { socket->disconnectFromServer(); });
    connect(socket, &QLocalSocket::readyRead, socket,
            [this, socket, buffer, consumed] {
              if (*consumed)
                return;
              buffer->append(socket->readAll());
              if (buffer->size() > 2 * 1024 * 1024) {
                socket->disconnectFromServer();
                return;
              }
              if (!buffer->contains('\n'))
                return;
              *consumed = true;
              QJsonParseError e;
              const auto doc = QJsonDocument::fromJson(
                  buffer->left(buffer->indexOf('\n')), &e);
              QJsonObject result;
              if (e.error != QJsonParseError::NoError || !doc.isObject() ||
                  !doc.object()["arguments"].isObject())
                result = {{"ok", false}, {"error", "Requisição inválida."}};
              else
                result = dispatch(doc.object()["method"].toString(),
                                  doc.object()["arguments"].toObject());
              socket->write(
                  QJsonDocument(result).toJson(QJsonDocument::Compact) + '\n');
              socket->disconnectFromServer();
            });
  }
}
QJsonObject ControlServer::state() const {
  const auto &s = w.session;
  return {{"application", "MotorLab Linux"},
          {"control_api_version", 1},
          {"pid", double(QCoreApplication::applicationPid())},
          {"version", QCoreApplication::applicationVersion()},
          {"connected", s.connected()},
          {"demo", s.isDemo()},
          {"model", s.model()},
          {"firmware", s.firmware()},
          {"writes_enabled", s.writesEnabled()},
          {"motion_enabled", s.motionEnabled()},
          {"watchdog_ms", s.watchdogMs()},
          {"streaming", s.streaming()},
          {"busy",
           !active.isEmpty() || s.busy() || w.applying || w.readsPending > 0},
          {"active_job", active},
          {"continuous", continuousState()},
          {"notice", w.notice->text()},
          {"sample_count", w.sampleCount()},
          {"physical_validation", "not_performed"},
          {"unavailable",
           QJsonArray{"firmware_serial", "firmware_can", "firmware_auxiliary",
                      "automatic_PI_FOC_Vk_Kt", "linearity_correction",
                      "CANopen_write_PDO_motion", "RoboCAN_transport"}}};
}
void ControlServer::requireIdle() const {
  need(!w.manualDriveActive(), "Controle pelos sliders em andamento; zere/pare os canais antes desta operação.");
  need(active.isEmpty() && !w.applying && !w.readsPending &&
           !w.session.uploading(),
       "Uma operação já está em andamento.");
  need(!QApplication::activeModalWidget(),
       "Feche o diálogo modal no MotorLab antes de operar pelo MCP.");
}
void ControlServer::authorize(const QJsonObject &a) {
  need(w.session.connected(), "Conecte primeiro.");
  if (!w.session.isDemo()) {
    need(
        a["confirm"].toBool() &&
            text(a, "expected_model") == w.session.model() &&
            text(a, "expected_firmware") == w.session.firmware(),
        "Operação real requer confirm=true, expected_model e expected_firmware "
        "exatamente como status, após autorização humana específica.");
  }
  need(w.session.setWritesEnabled(true),
       "Modelo/firmware não permite escrita nesta versão.");
}
QJsonObject ControlServer::newJob(const QString &kind) {
  requireIdle();
  while (jobs.size() >= 32) {
    auto oldest = jobs.begin();
    for (auto it = jobs.begin(); it != jobs.end(); ++it)
      if (it.value()["created_utc"].toString() <
          oldest.value()["created_utc"].toString())
        oldest = it;
    jobs.erase(oldest);
  }
  active = QUuid::createUuid().toString(QUuid::WithoutBraces);
  jobs[active] = {{"job_id", active},
                  {"kind", kind},
                  {"state", "running"},
                  {"created_utc", now()},
                  {"source", w.session.isDemo()      ? "demo"
                             : w.session.connected() ? "serial"
                                                     : "offline"},
                  {"model", w.session.model()},
                  {"firmware", w.session.firmware()},
                  {"replies", QJsonArray{}}};
  w.applying = true;
  // A pre-existing telemetry scan may finish while onReply is in maintenance
  // mode. Discard that partial frame so polling can resume after this job.
  w.scanActive = false;
  w.changes.clear();
  w.operationExpected.clear();
  w.operationTarget.clear();
  w.refreshState();
  deadline.start(120000);
  return {{"job_id", active}, {"state", "running"}};
}
void ControlServer::finish(bool ok, const QString &message,
                           const QJsonObject &result) {
  if (active.isEmpty())
    return;
  auto &j = jobs[active];
  j["state"] = ok ? "succeeded" : "failed";
  j["success"] = ok;
  j["message"] = message;
  j["finished_utc"] = now();
  j["result"] = result;
  if (continuous.enabled) {
    tick.setInterval(15);
    auto details = result;
    auto ended = continuousState();
    ended["active"] = false;
    ended["running"] = false;
    ended["stopping"] = false;
    details["continuous"] = ended;
    j["continuous"] = ended;
    j["result"] = details;
    for (auto label : w.speedSources)
      label->setText("Encoder A/B: ?S · Hall/interno: ?BS");
    for (auto label : w.hallRpm)
      label->setText("—");
    for (auto label : w.metrics)
      label->setText("—");
    w.faults->setText("Atualizando estado após parada…");
    w.continuousHistoryNote->setVisible(false);
    w.chart->setEnabled(true);
    w.ioValues->setEnabled(true);
  }
  supervision.stop();
  continuous = Continuous{};
  active.clear();
  waiting.clear();
  steps.clear();
  afterSteps = {};
  tick.stop();
  deadline.stop();
  w.applying = false;
  // Read jobs never acquire control permissions and must not revoke them.
  // Session transport failures still close the connection and disarm normally.
  if (j["kind"].toString() != "read")
    w.session.setWritesEnabled(false);
  w.refreshState();
  w.notify(message);
  w.writeLog(ok ? "MCP OK" : "MCP ERRO", message);
}
QList<ControlServer::Step> ControlServer::stopped() const {
  return {{"!R 0", {}}, {"!EX", {}}, {"?FF", {}, true}};
}
QJsonObject ControlServer::sequence(const QString &kind,
                                    const QList<Step> &cmds, bool tolerate,
                                    std::function<void()> done) {
  need(w.session.connected(), "Conecte primeiro.");
  auto result = newJob(kind);
  w.session.stopStream();
  steps = cmds;
  stepIndex = 0;
  waiting.clear();
  tolerateReject = tolerate;
  afterSteps = std::move(done);
  jobs[active]["total_steps"] = cmds.size();
  jobs[active]["completed_steps"] = 0;
  deadline.start(qMax(30000, cmds.size() * 1500));
  tick.start();
  return result;
}
void ControlServer::next() {
  if (active.isEmpty() || !waiting.isEmpty())
    return;
  if (!w.session.connected()) {
    finish(false, "Conexão encerrada durante a operação.");
    return;
  }
  if (w.session.busy())
    return;
  if (continuous.enabled && continuous.stopping && !continuous.stoppingSequence) {
    continuous.stoppingSequence = true;
    w.session.cancelMotion();
    steps = {{"!EX", {}}, {"?FF", {}, true}};
    stepIndex = 0;
    jobs[active]["phase"] = "stopping";
    afterSteps = [this] {
      finish(continuous.operatorStop, continuous.reason + " !EX confirmado.", {{"stop_confirmed", true}});
    };
  }
  if (stepIndex >= steps.size()) {
    tick.stop();
    if (afterSteps) {
      auto done = std::move(afterSteps);
      afterSteps = {};
      done();
    } else
      finish(true, "Sequência concluída; consulte replies para resultados e "
                   "rejeições individuais.");
    return;
  }
  waiting = steps[stepIndex].command;
  if (!w.session.enqueue(waiting))
    finish(false, "Comando recusado pelo estado da sessão: " +
                      Protocol::redacted(waiting));
}

QJsonObject ControlServer::dispatch(const QString &method,
                                    const QJsonObject &a) {
  try {
    bool found = false;
    for (auto v : schemas)
      if (v.toObject()["name"] == method) {
        validate(a, v.toObject()["inputSchema"].toObject());
        found = true;
        break;
      }
    need(found, "Ferramenta desconhecida.");
    auto reply = [this, &method, &a]() -> QJsonObject {
      auto &s = w.session;
      if (method == "motorlab_status")
        return state();
      if (method == "motorlab_continuous" && text(a, "action") == "renew") {
        need(continuous.enabled && !continuous.stopping &&
                 text(a, "lease_token") == continuous.token,
             "Supervisão inválida, encerrada ou token diferente.");
        if (continuous.lease.elapsed() >= continuous.leaseMs) {
          continuousStop("Supervisão expirou; renovação tardia recusada.");
          need(false, "Supervisão já expirou; movimento não será retomado.");
        }
        continuous.lease.restart();
        return {{"job_id", active}, {"continuous", continuousState()}};
      }
      if (method == "motorlab_job") {
        need(jobs.contains(text(a, "job_id")),
             "Job não encontrado nesta instância.");
        return jobs[text(a, "job_id")];
      }
      if (method == "motorlab_log")
        return {{"text", w.console->toPlainText()
                             .split('\n')
                             .mid(qMax(0, w.console->blockCount() -
                                              a["limit"].toInt(100)))
                             .join('\n')}};
      if (method == "motorlab_ports") {
        QJsonArray ports;
        for (auto dir : {QString("/dev/serial/by-id"), QString("/dev")}) {
          QDir d(dir);
          for (auto name : d.entryList(
                   dir == "/dev" ? QStringList{"ttyACM*", "ttyUSB*", "ttyS*"}
                                 : QStringList{"*"},
                   QDir::Files | QDir::System | QDir::NoDotAndDotDot))
            ports.append(d.absoluteFilePath(name));
        }
        return {{"ports", ports}, {"opened", false}};
      }
      if (method == "motorlab_stop") {
        if (continuous.enabled) {
          if (!continuous.stopping) {
            continuous.operatorStop = true;
            continuousStop("Parada solicitada pelo operador.");
          }
          return {{"job_id", active}, {"state", "running"}};
        }
        need(active.isEmpty() || jobs[active]["kind"] != "firmware",
             "DFU em andamento; não interrompa a gravação.");
        if (!active.isEmpty())
          finish(false, "Cancelado por parada; possíveis efeitos parciais.");
        s.stopStream();
        s.cancelMotion();
        w.refreshState();
        if (s.busy()) {
          const bool sent = s.enqueue("!EX");
          return {{"submitted", sent},
                  {"confirmed", false},
                  {"message", "Parada prioritária; reconecte e consulte ?FF se "
                              "a sessão fechou."}};
        }
        return sequence("stop", {{"!EX", {}}, {"?FF", {}, true}});
      }
      if (method == "motorlab_telemetry") {
        QJsonArray samples;
        for (int n = qMax(0, int(w.recorded.size()) - a["limit"].toInt(20));
             n < w.recorded.size(); ++n) {
          const auto &v = w.recorded[n];
          QJsonObject o{
              {"timestamp", v.timestamp}, {"seconds", v.seconds},
              {"amps1", v.amp1},          {"amps2", v.amp2},
              {"rpm1", v.rpm1},           {"rpm2", v.rpm2},
              {"rpm1_encoder", v.rpm1}, {"rpm2_encoder", v.rpm2},
              {"rpm1_hall", v.hallRpm1}, {"rpm2_hall", v.hallRpm2},
              {"voltage", v.voltage},     {"temperature", v.temperature}};
          o["encoder1"] =
              v.hasEncoder ? QJsonValue(double(v.encoder1)) : QJsonValue();
          o["encoder2"] =
              v.hasEncoder ? QJsonValue(double(v.encoder2)) : QJsonValue();
          o["faults"] =
              v.hasFaults ? QJsonValue(double(v.faults)) : QJsonValue();
          o["flags"] = v.hasFlags ? QJsonValue(double(v.flags)) : QJsonValue();
          samples.append(o);
        }
        return {{"samples", samples},
                {"connected", s.connected()},
                {"source", w.captureDemo ? "demo" : "serial"},
                {"model", w.captureModel},
                {"firmware", w.captureFirmware}};
      }
      if (method == "motorlab_catalog" || method == "motorlab_parameters") {
        QJsonArray entries;
        const bool catalog = method == "motorlab_catalog";
        const auto filter = text(a, "search");
        const auto model = text(a, "model").isEmpty()
                               ? w.offlineModel->currentText()
                               : text(a, "model");
        int matched = 0, offset = a["offset"].toInt(),
            limit = a["limit"].toInt(50);
        for (const auto &p : w.parameters) {
          if (!(p.suffix + " " + p.name).contains(filter, Qt::CaseInsensitive))
            continue;
          if (matched++ < offset || entries.size() >= limit)
            continue;
          QJsonObject o{{"query", "~" + p.suffix},
                        {"name", p.name},
                        {"unit", p.unit},
                        {"scale", p.scale},
                        {"editable", p.editable}};
          if (catalog) {
            o["minimum_raw"] = double(p.spec->minimum);
            o["maximum_raw"] = double(p.spec->maximum);
            o["disruptive"] = p.spec->disruptive;
            o["original"] =
                Catalog::original(model, p.key, p.channel ? p.channel : 1);
          } else {
            o["readable"] = p.known;
            o["raw"] = p.known ? QJsonValue(double(p.raw)) : QJsonValue();
            o["dirty"] = p.dirty;
            o["draft_raw"] =
                p.editor
                    ? QJsonValue(double(qRound64(p.editor->value() * p.scale)))
                    : QJsonValue();
          }
          entries.append(o);
        }
        return {{"entries", entries},        {"total", matched},
                {"offset", offset},          {"connected", s.connected()},
                {"source", w.profileSource}, {"model", model}};
      }
      if (method == "motorlab_view") {
        if (a.contains("model")) {
          requireIdle();
          need(!s.connected(), "Seleção de modelo somente desconectado.");
          w.offlineModel->setCurrentText(text(a, "model"));
        }
        if (a.contains("page"))
          w.selectPage(a["page"].toInt());
        if (a.contains("visible")) {
          if (a["visible"].toBool()) {
            w.show();
            w.raise();
          } else
            w.hide();
        }
        if (a.contains("screenshot_path")) {
          outputPath(text(a, "screenshot_path"), a["overwrite"].toBool());
          need(w.grab().save(text(a, "screenshot_path"), "PNG"),
               "Falha ao salvar captura.");
        }
        return {{"visible", w.isVisible()},
                {"page", w.pages->currentIndex()},
                {"screenshot_path", a["screenshot_path"]}};
      }
      requireIdle();
      if (method == "motorlab_continuous")
        return continuousStart(a);
      if (method == "motorlab_pause") {
        auto commands = stopped();
        if (a["cancel_calibration"].toBool())
          commands.prepend(Step{"%CLMOD 0", {}});
        return sequence("pause", commands);
      }
      if (method == "motorlab_connect") {
        need(!s.connected(),
             "Desconecte explicitamente a fonte atual antes de trocar.");
        if (text(a, "mode") == "serial") {
          need(!text(a, "port").isEmpty(), "Informe port.");
          w.source->setCurrentIndex(1);
          w.port->setCurrentText(text(a, "port"));
          w.baud->setCurrentText(QString::number(a["baud"].toInt(115200)));
        } else
          w.source->setCurrentIndex(0);
        w.connectSource();
        need(s.connected(), w.notice->text());
        return state();
      }
      if (method == "motorlab_disconnect") {
        s.close();
        return state();
      }
      if (method == "motorlab_query" || method == "motorlab_read_parameters") {
        QList<Step> commands;
        if (method == "motorlab_query") {
          auto c = Protocol::command(text(a, "command"));
          need(c.valid && !c.write, "Somente consultas ?/~ conhecidas.");
          commands.append(Step{c.text, {}});
        } else {
          for (auto v : a["queries"].toArray()) {
            auto c = Protocol::command(v.toString());
            need(c.valid && c.text.startsWith('~'),
                 "Consulta de parâmetro inválida.");
            commands.append(Step{c.text, {}});
          }
          if (commands.isEmpty())
            for (const auto &p : w.parameters)
              commands.append(Step{"~" + p.suffix, {}});
        }
        // onReply updates the same editor; don't silently mix a draft with a
        // newer reading.
        for (const auto &cmd : commands)
          for (auto &p : w.parameters)
            if (cmd.command == "~" + p.suffix)
              need(!p.dirty || a["discard_drafts"].toBool(),
                   "Existem rascunhos para a consulta. Use read_parameters com "
                   "discard_drafts=true.");
        for (const auto &cmd : commands)
          for (auto &p : w.parameters)
            if (cmd.command == "~" + p.suffix) {
              p.dirty = false;
              p.known = false;
            }
        return sequence("read", commands, method == "motorlab_read_parameters");
      }
      if (method == "motorlab_stage") {
        QVector<QPair<int, qint64>> staged;
        QSet<QString> seen;
        for (auto v : a["changes"].toArray()) {
          auto item = v.toObject();
          auto query = text(item, "query");
          need(!seen.contains(query), "Consulta duplicada.");
          seen.insert(query);
          int row = -1;
          for (int n = 0; n < w.parameters.size(); ++n)
            if ("~" + w.parameters[n].suffix == query)
              row = n;
          need(row >= 0, "Parâmetro desconhecido.");
          const auto &p = w.parameters[row];
          qint64 raw = qint64(item["raw"].toDouble());
          need(p.editable && p.editor &&
                   Catalog::validValue(*p.spec, raw, p.channel),
               "Valor ou parâmetro não editável.");
          staged.append({row, raw});
        }
        for (auto v : staged) {
          auto &p = w.parameters[v.first];
          QSignalBlocker block(p.editor);
          p.editor->setValue(v.second / p.scale);
          p.dirty = !p.known || p.raw != v.second;
          w.parameterItems[v.first]->setText(0, p.name + " · " + p.suffix +
                                                    (p.dirty ? " *" : ""));
        }
        w.refreshState();
        return {{"staged", staged.size()}, {"transmitted", false}};
      }
      if (method == "motorlab_apply") {
        // Some firmware rejects R0 when no script is loaded. Only tolerate
        // that specific rejection; FF and a fresh FS must still prove idle.
        QList<Step> commands{{"!R 0", {}, false, true}, {"!EX", {}},
                             {"?FF", {}, true}, {"?FS", {}, false, false, true}};
        int count = 0;
        bool disruptive = false;
        for (const auto &p : w.parameters)
          if (p.dirty) {
            need(p.known && p.editor && p.editable,
                 "Leia os parâmetros antes de aplicar rascunhos.");
            const auto raw = qRound64(p.editor->value() * p.scale);
            need(Catalog::validValue(*p.spec, raw, p.channel),
                 "Rascunho inválido.");
            if (raw == p.raw)
              continue;
            commands.append(
                Step{"^" + p.suffix + " " + QString::number(raw), {}});
            commands.append(Step{"~" + p.suffix, QString::number(raw)});
            ++count;
            disruptive |= p.spec->disruptive;
          }
        need(count > 0, "Nenhuma alteração pendente.");
        need(count == 1 || !disruptive, "Parâmetros de comunicação/segurança "
                                        "devem ser aplicados individualmente.");
        authorize(a);
        s.cancelMotion();
        return sequence("apply", commands);
      }
      if (method == "motorlab_maintenance") {
        const auto c = Protocol::command(text(a, "command"));
        need(c.valid && !c.motion && !c.emergency && !c.hex && c.key != "SLD" &&
                 (c.maintenance || c.key == "C" || c.key == "CB"),
             "Use uma manutenção documentada; upload e movimento têm "
             "ferramentas próprias.");
        authorize(a);
        s.cancelMotion();
        auto commands = stopped();
        if (c.key == "EESAV")
          commands = {{"!R 0", {}, false, true}, {"!EX", {}},
                      {"?FF", {}, true}, {"?FS", {}, false, false, true}};
        commands.append(Step{c.text, {}});
        return sequence("maintenance", commands, false, [this, c] {
          if (c.key == "EELD" || c.key == "EERST")
            for (auto &p : w.parameters) {
              p.known = false;
              p.dirty = false;
            }
          finish(true, "Manutenção recebeu ACK; !EX permanece aplicado. "
                       "Persistência requer verificação após reinício.");
        });
      }
      if (method == "motorlab_execute") {
        const auto c = Protocol::command(text(a, "command"));
        need(c.valid && c.motion,
             "Somente comando de movimento/script/saída/calibração "
             "documentado.");
        authorize(a);
        const int duration = a["duration_ms"].toInt();
        const bool release = a["release"].toBool();
        return sequence(
            "execute", {{"~RWD", {}}}, false, [this, c, duration, release] {
              if (!w.session.setMotionEnabled(true)) {
                finish(false, "Watchdog/modelo não permite movimento.");
                return;
              }
              steps.clear();
              if (release)
                steps.append(Step{"!MG", {}});
              steps.append(Step{c.text, {}});
              stepIndex = 0;
              waiting.clear();
              afterSteps = [this, duration, c] {
                const auto id = active;
                jobs[id]["phase"] = "bounded_execution";
                QTimer::singleShot(duration, this, [this, id, c] {
                  if (active != id)
                    return;
                  // Stop the power stage before optional cleanup can fail.
                  // A controller without a running script may reject !R 0;
                  // that must never delay EX or its fault-bit confirmation.
                  steps = {{"!EX", {}}, {"?FF", {}, true}, {"!R 0", {}}};
                  if (c.key == "CLMOD")
                    steps.insert(2, Step{"%CLMOD 0", {}});
                  stepIndex = 0;
                  waiting.clear();
                  afterSteps = [this] {
                    finish(true, "Comando recebeu ACK e !EX foi confirmado ao "
                                 "final. Não comprova resultado físico nem "
                                 "conclusão de calibração.");
                  };
                  tick.start();
                });
              };
              deadline.start(duration + 10000);
              tick.start();
            });
      }
      if (method == "motorlab_stream") {
        need(s.connected(), "Conecte primeiro.");
        if (text(a, "action") == "stop")
          s.stopStream();
        else {
          need(!s.busy(),
               "Aguarde a consulta atual antes de iniciar streaming.");
          need(s.startStream(a["interval_ms"].toInt(100)),
               "Streaming recusado pela Session.");
        }
        return state();
      }
      if (method == "motorlab_profile") {
        const auto action = text(a, "action"), path = text(a, "path");
        if (action.startsWith("export_")) {
          outputPath(path, a["overwrite"].toBool());
          bool ok = false;
          if (action == "export_json")
            ok = w.saveProfile(path);
          if (action == "export_cpr")
            ok = w.saveCpr(path);
          if (action == "export_csv")
            ok = w.saveCsv(path);
          if (action == "export_log")
            ok = writeFile(path, w.console->toPlainText().toUtf8());
          need(ok, "Não foi possível exportar (perfil exige valores conhecidos "
                   "ou rascunhos).");
          return {{"path", path}, {"partial_profile", true}};
        }
        if (action == "import_json") {
          QString error;
          need(w.loadProfile(path, &error), error);
          return {{"transmitted", false}};
        }
        const auto profile = CprProfile::parse(readFile(path));
        need(profile.valid(), profile.error);
        CprProfile other;
        QMap<QString, CprItem> right;
        if (action == "compare_cpr") {
          other = CprProfile::parse(readFile(text(a, "other_path")));
          need(other.valid(), other.error);
          for (auto v : other.items)
            right[v.identity()] = v;
        }
        QJsonArray entries;
        QSet<QString> seen;
        QVector<QPair<int, qint64>> staged;
        int skipped = 0;
        for (const auto &v : profile.items) {
          if (seen.contains(v.identity()))
            continue;
          seen.insert(v.identity());
          QJsonObject o{{"parameter", v.identity()},
                        {"raw", v.value},
                        {"type", v.type},
                        {"scale", v.multiplier},
                        {"path", v.node}};
          if (action == "compare_cpr") {
            const bool exists = right.contains(v.identity());
            auto r = right.take(v.identity());
            o["other_raw"] = exists ? QJsonValue(r.value) : QJsonValue();
            o["same"] = exists && v.value == r.value &&
                        v.multiplier == r.multiplier && v.type == r.type;
          }
          if (action == "import_cpr") {
            bool imported = false, number = false;
            const auto raw = v.value.toLongLong(&number);
            for (int n = 0; n < w.parameters.size(); ++n) {
              const auto &p = w.parameters[n];
              if (p.key == v.command &&
                  (p.channel ? p.channel : 1) == v.index && p.editor &&
                  p.editable && v.type != "String" && number &&
                  v.multiplier == qRound(p.scale) &&
                  Catalog::validValue(*p.spec, raw, p.channel)) {
                staged.append({n, raw});
                imported = true;
                break;
              }
            }
            o["staged"] = imported;
            if (!imported)
              ++skipped;
          }
          entries.append(o);
        }
        for (auto it = right.begin(); it != right.end(); ++it)
          entries.append(QJsonObject{{"parameter", it.key()},
                                     {"other_raw", it->value},
                                     {"same", false}});
        if (action == "import_cpr") {
          const auto model = s.connected() ? Protocol::supportedModel(s.model())
                                           : w.offlineModel->currentText();
          const bool demo = profile.roborun.contains("DEMO");
          need(profile.firmware == "v3.0:" + model,
               "Modelo/geração do CPR diferente.");
          need(!demo || !s.connected() || s.isDemo(),
               "Perfil simulado não pode ser importado em hardware.");
          for (auto v : staged) {
            auto &p = w.parameters[v.first];
            QSignalBlocker block(p.editor);
            p.editor->setValue(v.second / p.scale);
            p.dirty = !p.known || p.raw != v.second;
            w.parameterItems[v.first]->setText(0, p.name + " · " + p.suffix +
                                                      (p.dirty ? " *" : ""));
          }
          w.profileModel = model;
          w.profileSource = demo ? "demo" : "cpr";
          w.profileFirmware.clear();
          w.refreshState();
        }
        return {{"firmware", profile.firmware},
                {"entries", entries},
                {"staged", staged.size()},
                {"skipped", skipped},
                {"transmitted", false}};
      }
      if (method == "motorlab_script") {
        const auto action = text(a, "action");
        if (action == "get")
          return {{"source", w.script->toPlainText()}};
        if (action == "set") {
          need(a.contains("source"), "Informe source.");
          w.script->setPlainText(text(a, "source"));
          return {{"transmitted", false}};
        }
        if (action == "robocan") {
          auto hex = HexFile::parse(text(a, "hex").toLatin1());
          need(hex.valid(), hex.error);
          return {{"transcript", microbasicRoboCan(hex)},
                  {"transmitted", false}};
        }
        const auto source =
            a.contains("source") ? text(a, "source") : w.script->toPlainText();
        const auto exe = qEnvironmentVariable(
            "MOTORLAB_COMPILER", QCoreApplication::applicationDirPath() +
                                     "/../libexec/compiler/motorlab-compiler");
        need(QFileInfo(exe).isExecutable(), "Compilador Linux não encontrado.");
        auto result = newJob("compiler");
        const auto id = active;
        auto proc = new QProcess(this);
        auto bytes = std::make_shared<QByteArray>();
        connect(proc, &QProcess::readyReadStandardOutput, proc, [proc, bytes] {
          bytes->append(proc->readAllStandardOutput());
          if (bytes->size() > 8 * 1024 * 1024)
            proc->kill();
        });
        connect(proc, &QProcess::readyReadStandardError, proc,
                [proc] { proc->readAllStandardError(); });
        connect(proc, &QProcess::errorOccurred, proc,
                [this, proc, id](QProcess::ProcessError e) {
                  if (e == QProcess::FailedToStart) {
                    if (active == id)
                      finish(false, "Compilador não iniciou.");
                    proc->deleteLater();
                  }
                });
        connect(proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                proc,
                [this, proc, bytes, id](int code, QProcess::ExitStatus exit) {
                  bytes->append(proc->readAllStandardOutput());
                  auto result = QJsonDocument::fromJson(*bytes).object();
                  if (active == id)
                    finish(code == 0 && exit == QProcess::NormalExit &&
                               result["ok"].toBool(),
                           "Compilador/simulador offline; consulte result.",
                           result);
                  proc->deleteLater();
                });
        proc->start(exe, QStringList{});
        proc->write(
            QJsonDocument(QJsonObject{{"source", source},
                                      {"simulate", action == "simulate"}})
                .toJson(QJsonDocument::Compact));
        proc->closeWriteChannel();
        QTimer::singleShot(8000, proc, [proc] {
          if (proc->state() != QProcess::NotRunning)
            proc->kill();
        });
        return result;
      }
      if (method == "motorlab_upload_script") {
        auto hex = HexFile::parse(readFile(text(a, "path"), 256 * 1024));
        need(hex.valid(), hex.error);
        authorize(a);
        s.cancelMotion();
        return sequence("upload_script", stopped(), false, [this, hex] {
          deadline.start(120000);
          if (!w.session.uploadScript(hex))
            finish(false, "Upload recusado pela Session.");
        });
      }
      if (method == "motorlab_can_read") {
        auto can = new CanSdo(this);
        if (!can->open(text(a, "interface"), a["node"].toInt())) {
          auto error = can->error;
          delete can;
          need(false, error);
        }
        auto result = newJob("can_read");
        const auto id = active;
        jobs[id]["source"] = "CANopen";
        connect(can, &CanSdo::received, can,
                [this, can, id](quint16 index, quint8 sub, QByteArray data,
                                QString error) {
                  if (active == id)
                    finish(error.isEmpty(),
                           error.isEmpty() ? "Leitura SDO concluída." : error,
                           {{"index", index},
                            {"subindex", sub},
                            {"hex", QString::fromLatin1(data.toHex())}});
                  can->deleteLater();
                });
        if (!can->read(quint16(a["index"].toInt()),
                       quint8(a["subindex"].toInt()))) {
          finish(false, can->error);
          can->deleteLater();
        }
        return result;
      }
      if (method == "motorlab_firmware") {
        const auto model = text(a, "model").isEmpty()
                               ? Protocol::supportedModel(s.model())
                               : text(a, "model");
        auto image = Firmware::parse(readFile(text(a, "path")), model);
        need(image.valid(), image.error);
        if (text(a, "action") == "inspect_usb")
          return {{"model", image.model},
                  {"version", image.version},
                  {"sha256", image.sha256},
                  {"bytes", image.payload.size()},
                  {"address", double(image.address)}};
        outputPath(text(a, "backup_path"));
        need(s.connected() && !s.isDemo(), "DFU exige unidade real por USB.");
        authorize(a);
        auto result = newJob("firmware");
        const auto id = active;
        const auto path = text(a, "path"), backup = text(a, "backup_path");
        // Existing modal DFU implementation keeps a Qt event loop alive. Its
        // immutable copy, USB path binding and byte-for-byte verification are
        // reused, not duplicated.
        QTimer::singleShot(0, this, [this, id, path, backup] {
          w.applying = false;
          w.showFirmwareUpdate(path, backup, true);
          if (active == id)
            finish(false, w.notice->text());
        });
        deadline.stop();
        return result;
      }
      need(false, "Ferramenta sem implementação.");
      return {};
    }();
    return {{"ok", true}, {"result", reply}};
  } catch (const std::exception &e) {
    return {{"ok", false}, {"error", QString::fromUtf8(e.what())}};
  }
}
