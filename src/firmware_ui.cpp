#include "firmware.h"
#include "window.h"
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVBoxLayout>
namespace {
class UpdateDialog : public QDialog {
public:
  bool locked = false;
  using QDialog::QDialog;
  void reject() override {
    if (!locked)
      QDialog::reject();
  }
};
} // namespace
void Window::showFirmwareUpdate(const QString &inputPath, const QString &backupPath, bool automated) {
  if (!session.connected() || session.isDemo() ||
      applying || readsPending || session.uploading()) {
    notify("Conecte e identifique o controlador por USB.");
    return;
  }
  const auto usb = session.usbPath();
  const auto args = Firmware::dfuArguments(usb);
  if (args.isEmpty()) {
    notify("Atualização requer USB CDC identificado; adaptador RS232 não "
           "oferece DFU USB.");
    return;
  }
  const auto executable = QStandardPaths::findExecutable("dfu-util");
  if (executable.isEmpty()) {
    notify("Instale dfu-util pelo gerenciador da sua distribuição para usar "
           "atualização USB.");
    return;
  }
  const auto model = Protocol::supportedModel(session.model());
  const auto file = !inputPath.isEmpty() ? inputPath : QFileDialog::getOpenFileName(
      this, "Firmware oficial para " + model, QString(), "DfuSe (*.dfu)");
  if (file.isEmpty())
    return;
  QFile f(file);
  if (!f.open(QIODevice::ReadOnly) || f.size() > 1024 * 1024) {
    notify("DFU inacessível ou muito grande.");
    return;
  }
  const auto image = Firmware::parse(f.readAll(), model);
  if (!image.valid()) {
    notify(image.error);
    return;
  }
  if (std::any_of(parameters.begin(), parameters.end(),
                  [](const Parameter &p) { return p.dirty; }) ||
      !std::any_of(parameters.begin(), parameters.end(),
                   [](const Parameter &p) { return p.known; })) {
    notify("Leia a configuração e descarte/aplique os rascunhos antes de "
           "preparar a atualização.");
    return;
  }
  const int known = int(std::count_if(parameters.begin(), parameters.end(),
                                      [](const Parameter &p) { return p.known; }));
  const auto backup =
      !backupPath.isEmpty() ? backupPath : QFileDialog::getSaveFileName(this, "Salvar perfil antes da atualização",
                                   "antes-do-firmware.json", "Perfil (*.json)");
  if (backup.isEmpty() || (!session.isRemote() && !saveProfile(backup))) {
    notify("Leia a configuração e salve um perfil antes de atualizar.");
    return;
  }
  const QString details =
      QString("Atualizar %1 para %2 pelo USB %3.\n\nO script será pausado e "
              "!EX será confirmado. O perfil salvo contém %5 de %6 campos do catálogo;\n"
              "não é uma cópia completa da memória. O carregador USB será aberto e a Flash da "
              "aplicação será regravada.\nNão interrompa a alimentação nem "
              "desconecte o cabo.\nSHA-256: %4\n\nApós gravar, será tentada a "
              "comparação da Flash por leitura USB. O retorno à aplicação "
              "exige reiniciar o controlador e conferir ?FID. Confirmar?")
          .arg(model, image.version, usb, image.sha256)
          .arg(known).arg(parameters.size());
  if (!automated && QMessageBox::question(this, "Revisar atualização", details,
                            QMessageBox::Yes | QMessageBox::Cancel,
                            QMessageBox::Cancel) != QMessageBox::Yes)
    return;
  if(session.isRemote()) {
    const auto reply=session.backendCall("motorlab_firmware",{{"action","update_usb"},{"path",file},{"backup_path",backup},
        {"confirm",true},{"expected_model",session.model()},{"expected_firmware",session.firmware()}});
    if(!reply["ok"].toBool()) { notify(reply["error"].toString()); return; }
    const auto job=reply["result"].toObject()["job_id"].toString();
    UpdateDialog dialog(this); dialog.setWindowTitle("Atualização USB pelo motorlabd"); dialog.resize(700,260); dialog.locked=true;
    auto layout=new QVBoxLayout(&dialog); auto state=new QLabel("Daemon preparando/gravando firmware…"); state->setWordWrap(true); layout->addWidget(state);
    auto close=new QPushButton("Fechar"); close->setEnabled(false); layout->addWidget(close); connect(close,&QPushButton::clicked,&dialog,&QDialog::accept);
    QTimer timer(&dialog); timer.setInterval(500);
    connect(&timer,&QTimer::timeout,&dialog,[&] {
      const auto result=session.backendCall("motorlab_job",{{"job_id",job}});
      if(!result["ok"].toBool()) {
        timer.stop(); state->setText("Conexão com daemon perdida. Atualização não confirmada; consulte o job antes de operar."); dialog.locked=false; close->setEnabled(true); return;
      }
      const auto status=result["result"].toObject(); state->setText(status["message"].toString("Atualização em andamento no daemon…"));
      if(status["state"].toString()!="running") {
        timer.stop(); dialog.locked=false; close->setEnabled(true); notify(state->text()); emit firmwareUpdateFinished(status["success"].toBool(),state->text());
        if(automated) QTimer::singleShot(0,&dialog,&QDialog::accept);
      }
    }); timer.start(); dialog.exec(); return;
  }
  QTemporaryDir dir;
  if (!dir.isValid()) {
    notify("Não foi possível preparar a cópia imutável do firmware.");
    return;
  }
  const auto frozen = dir.filePath("firmware.dfu"),
             readback = dir.filePath("verificacao.bin");
  QFile copy(frozen);
  if (!copy.open(QIODevice::WriteOnly) ||
      copy.write(image.container) != image.container.size()) {
    notify("Falha ao preparar o DFU.");
    return;
  }
  copy.close();
  copy.setPermissions(QFile::ReadOwner);
  session.stopStream();
  session.cancelMotion();
  if (!session.prepareFirmwareUpdate(image.model, usb)) {
    notify("Modelo, versão ou USB mudou. Atualização não iniciada.");
    return;
  }
  pollTimer.stop();
  scanActive = false;
  applying = true;
  refreshState();
  UpdateDialog dialog(this);
  dialog.setWindowTitle("Atualização USB nativa · " + model);
  dialog.resize(700, 460);
  dialog.locked = true;
  auto layout = new QVBoxLayout(&dialog);
  auto state = new QLabel("Confirmando script pausado e parada de emergência…");
  state->setWordWrap(true);
  layout->addWidget(state);
  auto log = new QPlainTextEdit;
  log->setReadOnly(true);
  log->setMaximumBlockCount(2000);
  layout->addWidget(log, 1);
  auto close = new QPushButton("Fechar");
  close->setEnabled(false);
  layout->addWidget(close);
  connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
  QProcess proc(&dialog);
  proc.setProcessChannelMode(QProcess::MergedChannels);
  QTimer deadline(&dialog);
  deadline.setSingleShot(true);
  int phase = 0, attempts = 0;
  QString waiting = "!R 0", output;
  auto finish = [&](const QString &text, bool verified = false) {
    deadline.stop();
    dialog.locked = false;
    close->setEnabled(true);
    state->setText(text);
    applying = false;
    refreshState();
    notify(text);
    writeLog("DFU", text);
    emit firmwareUpdateFinished(verified, text);
    if (automated) QTimer::singleShot(0, &dialog, &QDialog::accept);
  };
  auto launch = [&](const QStringList &more, int limit) {
    output.clear();
    proc.setProgram(executable);
    proc.setArguments(args + more);
    deadline.start(limit);
    proc.start();
  };
  connect(&proc, &QProcess::readyReadStandardOutput, &dialog, [&] {
    const auto chunk = QString::fromLocal8Bit(proc.readAllStandardOutput());
    output += chunk;
    if (output.size() > 65536)
      output = output.right(65536);
    log->appendPlainText(chunk.trimmed());
  });
  connect(&proc, &QProcess::errorOccurred, &dialog,
          [&](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart)
              finish("Não foi possível executar dfu-util. Controlador pode "
                     "estar em DFU; regrave antes de usar.");
          });
  connect(&deadline, &QTimer::timeout, &dialog, [&] {
    // A bounded failure ends the host operation; it never resumes motor
    // commands.
    if (proc.state() != QProcess::NotRunning) {
      proc.kill();
      phase = 99;
    }
    finish("Tempo de atualização esgotado. Estado da Flash não confirmado; "
           "mantenha o controlador fora de operação.");
  });
  connect(&proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          &dialog, [&](int code, QProcess::ExitStatus exit) {
            deadline.stop();
            if (phase == 99)
              return;
            if (phase == 3) {
              if (code == 0 && exit == QProcess::NormalExit &&
                  output.contains("Found DFU") && output.contains("alt=0")) {
                phase = 4;
                state->setText(
                    "Gravando aplicação oficial. Mantenha cabo e alimentação.");
                launch({"-D", frozen}, 120000);
              } else if (++attempts < 10) {
                QTimer::singleShot(700, &dialog, [&] { launch({"-l"}, 5000); });
              } else
                finish("DFU não localizado no caminho USB identificado. Nenhum "
                       "arquivo foi gravado; verifique acesso USB/permissões e "
                       "reconecte.");
            } else if (phase == 4) {
              if (code != 0 || exit != QProcess::NormalExit) {
                finish("Falha durante gravação. Estado da Flash não "
                       "confirmado; recupere pelo DFU antes de usar.");
                return;
              }
              phase = 5;
              state->setText(
                  "Gravação concluída. Comparando o conteúdo da Flash…");
              launch({"-s",
                      QString("0x%1:%2")
                          .arg(image.address, 0, 16)
                          .arg(image.payload.size()),
                      "-U", readback},
                     120000);
            } else if (phase == 5) {
              QFile actual(readback);
              const bool match = code == 0 && exit == QProcess::NormalExit &&
                                 actual.open(QIODevice::ReadOnly) &&
                                 actual.size() == image.payload.size() &&
                                 actual.readAll() == image.payload;
              finish(match ? "Flash comparada byte a byte com o firmware "
                             "oficial. Reinicie o controlador, reconecte e "
                             "confirme a versão em ?FID antes de usar."
                           : "dfu-util confirmou a gravação, mas a comparação "
                             "por leitura não foi confirmada. Reinicie, "
                             "confira ?FID e valide na bancada antes de usar.", match);
            }
          });
  QTimer enter(&dialog);
  enter.setInterval(30);
  connect(&enter, &QTimer::timeout, &dialog, [&] {
    if (session.busy())
      return;
    enter.stop();
    if (!session.enterDfu(image.model, usb)) {
      finish("Não foi possível solicitar DFU. Atualização não iniciada.");
      return;
    }
    phase = 3;
    state->setText("Aguardando o DFU no mesmo caminho USB…");
    launch({"-l"}, 5000);
  });
  connect(&session, &Session::received, &dialog,
          [&](const QString &cmd, const QString &payload, bool ok) {
            if (phase > 2 || cmd != waiting)
              return;
            if (!ok) {
              phase = 99;
              finish("Parada rejeitada. Atualização não iniciada.");
              return;
            }
            if (phase == 0) {
              phase = 1;
              waiting = "!EX";
            } else if (phase == 1) {
              phase = 2;
              waiting = "?FF";
            } else {
              const auto r = Protocol::reply("FF=" + payload);
              if (!r.numeric || r.numbers.size() != 1 || r.numbers[0] < 0 ||
                  r.numbers[0] > 65535 || !(r.numbers[0] & 16)) {
                phase = 99;
                finish("!EX não confirmado por ?FF. Atualização cancelada.");
                return;
              }
              waiting.clear();
              enter.start();
              return;
            }
            if (!session.enqueue(waiting)) {
              phase = 99;
              finish("Falha na preparação. Atualização cancelada.");
            }
          });
  connect(&session, &Session::failed, &dialog, [&](const QString &message) {
    if (phase <= 2) {
      phase = 99;
      finish("Atualização não iniciada: " + message);
    }
  });
  if (!session.enqueue(waiting))
    finish("Não foi possível iniciar a preparação.");
  dialog.exec();
  pollTimer.start();
  applying = false;
  refreshState();
}
