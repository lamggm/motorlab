#pragma once
#include "session.h"
#include "firmware.h"
#include <QProcess>
#include <QTemporaryDir>
class FirmwareJob final : public QObject {
  Q_OBJECT
public:
  explicit FirmwareJob(Session &session, QObject *parent = nullptr);
  void start(const QString &path);
signals:
  void finished(bool success, QString message);
private:
  Session &session;
  FirmwareImage image;
  QTemporaryDir dir;
  QProcess process;
  QTimer deadline, enter;
  QString executable, usb, waiting, output;
  QStringList args;
  int phase = -1, attempts = 0;
  bool ended = false;
  void finish(bool success, const QString &message);
  void launch(const QStringList &, int timeout);
};
