#include "firmware_job.h"
#include <QFile>
#include <QStandardPaths>
FirmwareJob::FirmwareJob(Session &s, QObject *parent) : QObject(parent), session(s) {
  process.setProcessChannelMode(QProcess::MergedChannels); deadline.setSingleShot(true); enter.setInterval(30);
  connect(&process,&QProcess::readyReadStandardOutput,this,[this] {
    output += QString::fromLocal8Bit(process.readAllStandardOutput()); output = output.right(65536);
  });
  connect(&process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) finish(false,"dfu-util não iniciou; confira o estado DFU antes de operar.");
  });
  connect(&deadline,&QTimer::timeout,this,[this] { finish(false,"Tempo de atualização esgotado; Flash não confirmada."); });
  connect(&enter,&QTimer::timeout,this,[this] {
    if (session.busy()) return;
    enter.stop();
    if (!session.enterDfu(image.model,usb)) { finish(false,"Modelo/USB mudou; DFU não iniciado."); return; }
    phase = 3; launch({"-l"},5000);
  });
  connect(&session,&Session::received,this,[this](const QString &cmd,const QString &payload,bool ok) {
    if (ended || phase < 0 || phase > 2 || cmd != waiting) return;
    if (!ok) { finish(false,"Parada rejeitada; atualização não iniciada."); return; }
    if (phase == 0) { phase = 1; waiting = "!EX"; }
    else if (phase == 1) { phase = 2; waiting = "?FF"; }
    else {
      auto r = Protocol::reply("FF="+payload);
      if (!r.numeric || r.numbers.size()!=1 || r.numbers[0]<0 || r.numbers[0]>65535 || !(r.numbers[0]&16)) {
        finish(false,"EX não confirmado; atualização cancelada."); return;
      }
      waiting.clear(); enter.start(); return;
    }
    if (!session.enqueue(waiting)) finish(false,"Preparação DFU recusada.");
  });
  connect(&session,&Session::failed,this,[this](const QString &message) { if (phase <= 2) finish(false,message); });
  connect(&process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus exit) {
    if (ended) return;
    deadline.stop(); output += QString::fromLocal8Bit(process.readAllStandardOutput());
    const bool ok = code==0 && exit==QProcess::NormalExit;
    if (phase==3) {
      if (ok && output.contains("Found DFU") && output.contains("alt=0")) {
        phase=4; launch({"-D",dir.filePath("firmware.dfu")},120000);
      } else if (++attempts<10) {
        QTimer::singleShot(700,this,[this] { if (!ended) launch({"-l"},5000); });
      } else finish(false,"DFU não encontrado no mesmo caminho USB; não gravado.");
    } else if (phase==4) {
      if (!ok) { finish(false,"Falha na gravação; Flash não confirmada."); return; }
      phase=5; launch({"-s",QString("0x%1:%2").arg(image.address,0,16).arg(image.payload.size()),"-U",dir.filePath("readback.bin")},120000);
    } else if (phase==5) {
      QFile f(dir.filePath("readback.bin"));
      const bool match=ok && f.open(QIODevice::ReadOnly) && f.size()==image.payload.size() && f.readAll()==image.payload;
      finish(match,match?"Flash comparada byte a byte. Reinicie, reconecte e confira FID antes de operar.":"Gravação terminou, mas comparação da Flash não confirmada.");
    }
  });
}
void FirmwareJob::finish(bool success,const QString &message) {
  if (ended) return;
  ended=true; deadline.stop(); enter.stop();
  if (process.state()!=QProcess::NotRunning) { process.kill(); process.waitForFinished(1000); }
  emit finished(success,message);
}
void FirmwareJob::launch(const QStringList &more,int timeout) {
  output.clear(); deadline.start(timeout); process.start(executable,args+more);
}
void FirmwareJob::start(const QString &path) {
  usb=session.usbPath(); args=Firmware::dfuArguments(usb); executable=QStandardPaths::findExecutable("dfu-util");
  if (!session.connected() || session.isDemo() || args.isEmpty() || executable.isEmpty()) {
    finish(false,"DFU exige unidade USB identificada e dfu-util instalado."); return;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly) || file.size()>1024*1024) { finish(false,"Arquivo DFU inacessível."); return; }
  image=Firmware::parse(file.readAll(),Protocol::supportedModel(session.model()));
  if (!image.valid() || !dir.isValid()) { finish(false,image.valid()?"Falha ao preparar DFU.":image.error); return; }
  QFile frozen(dir.filePath("firmware.dfu"));
  if (!frozen.open(QIODevice::WriteOnly) || frozen.write(image.container)!=image.container.size()) { finish(false,"Falha ao congelar firmware."); return; }
  frozen.close(); frozen.setPermissions(QFile::ReadOwner);
  session.stopStream(); session.cancelMotion();
  if (!session.prepareFirmwareUpdate(image.model,usb)) { finish(false,"Modelo/USB mudou; atualização recusada."); return; }
  phase=0; waiting="!R 0"; deadline.start(15000);
  if (!session.enqueue(waiting)) finish(false,"Preparação DFU recusada.");
}
