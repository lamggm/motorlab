#include "headless_host.h"
#include "control.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QSocketNotifier>
#include <csignal>
#include <fcntl.h>
#include <cerrno>
#include <cstdio>
#include <unistd.h>
namespace { int shutdownPipe[2] = {-1,-1}; void terminate(int) { const int savedErrno=errno; const char byte=1; if(shutdownPipe[1]>=0) { const auto unused=::write(shutdownPipe[1],&byte,1); (void)unused; } errno=savedErrno; } }
int main(int argc,char **argv) {
  QCoreApplication app(argc,argv);
  app.setApplicationName("MotorLab CLI"); app.setApplicationVersion("0.7.0");
  QCommandLineParser options; options.setApplicationDescription("Backend CLI nativo do MotorLab. Sem interface gráfica; inicia desconectado.");
  options.addHelpOption(); options.addVersionOption();
  options.addOption({"control-socket","Socket Unix privado compatível com o MCP.","path"});
  options.addOption({"hidden","Compatibilidade de argumentos: CLI não tem janela."});
  options.process(app);
  QString runtime=qEnvironmentVariable("XDG_RUNTIME_DIR",QString("/run/user/%1").arg(getuid()));
  if (!QFileInfo(runtime).isDir()) runtime=QDir::homePath()+"/.local/state";
  const auto path=options.isSet("control-socket")?options.value("control-socket"):runtime+"/motorlab-cli/control.sock";
  const auto parent=QFileInfo(path).absolutePath();
  if (!QFileInfo(parent).exists()) { QDir().mkpath(parent); QFile::setPermissions(parent,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner); }
  Session session; HeadlessHost host(session); ControlServer control(host);
  if (!control.listen(path)) { std::fprintf(stderr,"Socket: %s\n",qPrintable(control.error())); return 3; }
  if (::pipe2(shutdownPipe,O_NONBLOCK|O_CLOEXEC)!=0) return 4;
  QSocketNotifier notifier(shutdownPipe[0],QSocketNotifier::Read);
  QObject::connect(&notifier,&QSocketNotifier::activated,&app,[&] {
    char bytes[64]; while (::read(shutdownPipe[0],bytes,sizeof bytes)>0) {}
    if (host.firmwareRunning()) {
      std::fprintf(stderr,"DFU em andamento; encerramento adiado até concluir a gravação.\n");
      QObject::connect(&host,&ControlHost::firmwareUpdateFinished,&app,[&](bool,const QString &) { app.quit(); },Qt::SingleShotConnection);
      return;
    }
    notifier.setEnabled(false); session.stopStream(); session.close(); app.quit();
  });
  std::signal(SIGINT,terminate); std::signal(SIGTERM,terminate);
  QObject::connect(&app,&QCoreApplication::aboutToQuit,&app,[&] { session.close(); });
  const int result=app.exec(); ::close(shutdownPipe[0]); ::close(shutdownPipe[1]); return result;
}
