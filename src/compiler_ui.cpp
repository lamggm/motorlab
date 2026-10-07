#include "hexfile.h"
#include "robocan.h"
#include "window.h"
#include <QCoreApplication>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QTimer>
#include <QVBoxLayout>
#include <memory>

void Window::compileScript(bool simulate) {
  const QByteArray source = script->toPlainText().toUtf8();
  if (source.size() > 1024 * 1024) {
    notify("Script maior que 1 MiB.");
    return;
  }
  const auto exe = qEnvironmentVariable(
      "MOTORLAB_COMPILER", QCoreApplication::applicationDirPath() +
                               "/../libexec/compiler/motorlab-compiler");
  if (!QFileInfo(exe).isExecutable()) {
    notify("Compilador Linux ausente. Use o pacote completo.");
    return;
  }
  auto dialog = new QDialog(this);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setObjectName("compilerDialog");
  dialog->setWindowTitle(simulate ? "Simulação do bytecode"
                                  : "Compilar MicroBasic");
  dialog->resize(900, 620);
  auto layout = new QVBoxLayout(dialog);
  auto caption =
      new QLabel("Constantes offline do RoboRun+ 3.3. Compatibilidade do "
                 "bytecode com a unidade ainda requer teste em bancada.");
  caption->setWordWrap(true);
  layout->addWidget(caption);
  auto result = new QPlainTextEdit;
  result->setReadOnly(true);
  result->setObjectName("compilerResult");
  result->setPlainText("Compilando…");
  layout->addWidget(result, 1);
  auto save = new QPushButton("Exportar HEX…");
  save->setEnabled(false);
  save->setObjectName("saveCompiledHex");
  layout->addWidget(save);
  auto saveCan = new QPushButton("Exportar roteiro RoboCAN…");
  saveCan->setEnabled(false);
  layout->addWidget(saveCan);
  auto process = new QProcess(dialog);
  auto timeout = new QTimer(dialog);
  timeout->setSingleShot(true);
  auto out = std::make_shared<QByteArray>();
  auto hex = std::make_shared<QByteArray>();
  connect(save, &QPushButton::clicked, dialog, [dialog, hex, result] {
    const auto path = QFileDialog::getSaveFileName(
        dialog, "Exportar bytecode", "script.hex", "Intel HEX (*.hex)");
    if (path.isEmpty())
      return;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(*hex) != hex->size() ||
        !f.commit())
      result->appendPlainText("Falha ao salvar HEX.");
    else
      result->appendPlainText("HEX salvo: " + path);
  });
  connect(saveCan, &QPushButton::clicked, dialog, [dialog, hex, result] {
    const auto data = microbasicRoboCan(HexFile::parse(*hex)).toUtf8();
    if (data.isEmpty())
      return;
    const auto path = QFileDialog::getSaveFileName(
        dialog, "Exportar roteiro RoboCAN", "script-can.txt", "Texto (*.txt)");
    if (path.isEmpty())
      return;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() ||
        !f.commit())
      result->appendPlainText("Falha ao salvar roteiro.");
    else
      result->appendPlainText("Roteiro salvo: " + path +
                              ". SS/TT são identificadores simbólicos; nenhum "
                              "frame foi transmitido.");
  });
  connect(process, &QProcess::readyReadStandardOutput, dialog, [process, out] {
    out->append(process->readAllStandardOutput());
    if (out->size() > 8 * 1024 * 1024)
      process->kill();
  });
  connect(process, &QProcess::readyReadStandardError, dialog,
          [process] { process->readAllStandardError(); });
  connect(timeout, &QTimer::timeout, dialog, [process, result] {
    result->setPlainText("Compilação excedeu o limite de 8 segundos.");
    process->kill();
  });
  connect(process, &QProcess::errorOccurred, dialog,
          [result](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart)
              result->setPlainText(
                  "Não foi possível iniciar o compilador Linux.");
          });
  connect(process, &QProcess::finished, dialog,
          [=](int code, QProcess::ExitStatus status) {
            timeout->stop();
            out->append(process->readAllStandardOutput());
            if (status != QProcess::NormalExit || code != 0) {
              result->appendPlainText(
                  "Processo encerrado sem resultado válido.");
              return;
            }
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(*out, &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject()) {
              result->setPlainText("Resposta inválida do compilador.");
              return;
            }
            const auto obj = doc.object();
            if (!obj["ok"].toBool()) {
              QStringList errors;
              for (auto e : obj["errors"].toArray())
                errors.append(e.toString());
              result->setPlainText(errors.join('\n'));
              return;
            }
            *hex = obj["hex"].toString().toLatin1();
            const auto parsed = HexFile::parse(*hex);
            if (!parsed.valid()) {
              result->setPlainText(parsed.error);
              hex->clear();
              return;
            }
            save->setEnabled(true);
            saveCan->setEnabled(true);
            QString report = QString("Compilado: %1 bytes · CRC32 %2\n")
                                 .arg(obj["size"].toInt())
                                 .arg(obj["crc32"].toString());
            for (auto warning : obj["warnings"].toArray())
              report += warning.toString() + "\n";
            if (simulate) {
              const auto sim = obj["simulation"].toObject();
              report +=
                  sim["completed"].toBool()
                      ? "Simulação concluída.\n"
                      : "Simulação interrompida pelo limite de tempo/passos.\n";
              report += "Entradas simuladas em zero; SetCommand e SetConfig "
                        "ficam locais.\n\n" +
                        sim["output"].toString() + "\n";
              for (auto event : sim["events"].toArray())
                report += event.toString() + "\n";
            }
            result->setPlainText(report + "\n" + obj["listing"].toString());
          });
  connect(dialog, &QDialog::finished, process, [process] {
    if (process->state() != QProcess::NotRunning) {
      process->kill();
      process->waitForFinished(1000);
    }
  });
  process->setProgram(exe);
  process->start();
  process->write(
      QJsonDocument(QJsonObject{{"source", QString::fromUtf8(source)},
                                {"simulate", simulate}})
          .toJson(QJsonDocument::Compact));
  process->closeWriteChannel();
  timeout->start(8000);
  dialog->show();
}
