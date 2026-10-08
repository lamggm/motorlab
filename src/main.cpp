#include "window.h"
#include "remote_session.h"
#include <unistd.h>
#include "control.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QTabBar>
#include <cstdio>

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QCoreApplication::setApplicationName("MotorLab Linux");
  QCoreApplication::setApplicationVersion("0.8.0");
  QCommandLineParser options;
  options.setApplicationDescription(
      "Interface nativa Linux para Roboteq SBLG2360T e SBLMG2360T. Inicia "
      "desconectada.");
  options.addHelpOption();
  options.addVersionOption();
  options.addOption(
      {"demo", "Iniciar em demonstração, sem acesso a porta serial."});
  options.addOption(
      {"screenshot", "Capturar uma tela da demonstração e sair.", "arquivo"});
  options.addOption({"screenshot-size",
                     "Dimensões da janela para captura, por exemplo 1280x720.",
                     "larguraxaltura"});
  options.addOption({"page",
                     "Página: 0 configuração, 1 monitoramento, 2 console, 3 "
                     "diagnóstico, 4 scripts, 5 sobre.",
                     "numero", "0"});
  options.addOption({"control-socket", "API local para MCP, socket Unix privado.", "path"});
  options.addOption({"backend-socket", "Socket do motorlabd compartilhado.", "path"});
  options.addOption({"hidden", "Iniciar janela oculta (MCP pode mostrá-la)."});
  options.process(app);
  QPixmap icon(128, 128);
  icon.fill(QColor("#101e33"));
  QPainter painter(&icon);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(QColor("#62a8ff"), 9));
  painter.drawRoundedRect(QRectF(21, 26, 86, 76), 16, 16);
  painter.setPen(QPen(QColor("#45d4b5"), 8));
  painter.drawLine(36, 82, 51, 49);
  painter.drawLine(51, 49, 66, 75);
  painter.drawLine(66, 75, 92, 45);
  painter.end();
  app.setWindowIcon(QIcon(icon));
  QString runtime=qEnvironmentVariable("XDG_RUNTIME_DIR",QString("/run/user/%1").arg(getuid()));
  if(!QFileInfo(runtime).isDir()) runtime=QDir::homePath()+"/.local/state";
  const QString backendPath=options.isSet("backend-socket") ? options.value("backend-socket") : runtime+"/motorlab-cli/control.sock";
  RemoteSession backend(backendPath);
  Window window(nullptr, &backend);
  if (!backend.attach()) { std::fprintf(stderr,"Backend: %s\n",qPrintable(backend.error())); return 4; }
  if (options.isSet("screenshot-size")) {
    const auto parts = options.value("screenshot-size").split('x');
    bool widthOk = false, heightOk = false;
    const int width = parts.value(0).toInt(&widthOk),
              height = parts.value(1).toInt(&heightOk);
    if (!options.isSet("screenshot") || parts.size() != 2 || !widthOk ||
        !heightOk || width < 1100 || width > 3840 || height < 680 ||
        height > 2160)
      return 2;
    window.resize(width, height);
  }
  ControlServer control(window);
  if (options.isSet("control-socket") && !control.listen(options.value("control-socket"))) {
    std::fprintf(stderr, "Control socket: %s\n", qPrintable(control.error()));
    return 3;
  }
  if (!options.isSet("hidden")) window.show();
  if (options.isSet("demo") || options.isSet("screenshot"))
    window.startDemo();
  if (options.isSet("screenshot")) {
    bool ok = false;
    int page = options.value("page").toInt(&ok);
    if (!ok || page < 0 || page > 5)
      return 2;
    const QString path = options.value("screenshot");
    QTimer::singleShot(4500, &window, [&window, &app, path, page] {
      window.selectPage(page);
      if (page == 0)
        window.findChild<QPushButton *>("readParameters")->click();
      QTimer::singleShot(
          page == 0 ? 14000 : 150, &window, [&window, &app, path] {
            const bool saved = window.grab().save(path);
            std::fprintf(stdout, "%s\n",
                         saved ? "Screenshot saved" : "Screenshot failed");
            app.exit(saved ? 0 : 1);
          });
    });
  }
  return app.exec();
}
