#pragma once
#include "catalog.h"
#include "session.h"
#include <QJsonObject>
#include <QObject>

struct ControlParameter {
  QString key, name, unit, suffix;
  const ConfigSpec *spec = nullptr;
  int channel = 0;
  double scale = 1;
  qint64 raw = 0, draftRaw = 0;
  bool known = false, editable = false, dirty = false;
};
// Backend contract: the command/supervision engine has no widget dependency.
class ControlHost : public QObject {
  Q_OBJECT
public:
  explicit ControlHost(Session &session, QObject *parent = nullptr)
      : QObject(parent), session(session) {}
  Session &session;
  virtual QVector<ControlParameter> parameters() const = 0;
  virtual void stageParameter(int row, qint64 raw) = 0;
  virtual void clearParameter(int row) = 0;
  virtual bool isApplying() const = 0;
  virtual int readsPending() const = 0;
  virtual bool manualDriveActive() const { return false; }
  virtual bool modalActive() const { return false; }
  virtual void beginOperation() = 0;
  virtual void endOperation() = 0;
  virtual void refreshState() {}
  virtual QString noticeText() const = 0;
  virtual void notify(const QString &) = 0;
  virtual void writeLog(const QString &, const QString &) = 0;
  virtual QString logText() const = 0;
  virtual QString scriptText() const = 0;
  virtual void setScript(const QString &) = 0;
  virtual QString selectedModel() const = 0;
  virtual QString profileSource() const = 0;
  virtual void setProfileMetadata(const QString &, const QString &, const QString &) = 0;
  virtual bool connectDevice(const QString &mode, const QString &port, int baud) = 0;
  virtual QJsonObject view(const QJsonObject &) = 0;
  virtual QJsonObject telemetry(int limit) const = 0;
  virtual int sampleCount() const = 0;
  virtual bool saveProfile(const QString &) = 0;
  virtual bool loadProfile(const QString &, QString *) = 0;
  virtual bool saveCpr(const QString &) = 0;
  virtual bool saveCsv(const QString &) = 0;
  virtual void continuousUpdate(const QJsonObject &, int, qint64) {}
  virtual void continuousFinished() {}
  virtual void startFirmware(const QString &, const QString &) = 0;
  virtual bool headless() const { return false; }
signals:
  void firmwareUpdateFinished(bool success, QString message);
};
