#pragma once
#include "session.h"
class RemoteSession final : public Session {
  Q_OBJECT
public:
  explicit RemoteSession(const QString &socketPath, QObject *parent = nullptr);
  ~RemoteSession() override;
  bool attach(bool launch = true);
  bool openSerial(const QString &, int) override;
  void openDemo() override;
  void close() override;
  bool enqueue(const QString &) override;
  bool connected() const override { return snapshot["connected"].toBool(); }
  bool isDemo() const override { return snapshot["demo"].toBool(); }
  bool busy() const override {
    return snapshot["session_busy"].toBool() ||
           snapshot["other_operation"].toBool();
  }
  bool setWritesEnabled(bool) override;
  bool writesEnabled() const override {
    return snapshot["writes_enabled"].toBool();
  }
  bool setMotionEnabled(bool) override;
  bool motionEnabled() const override {
    return snapshot["motion_enabled"].toBool();
  }
  int watchdogMs() const override { return snapshot["watchdog_ms"].toInt(-1); }
  void cancelMotion() override;
  bool startStream(int) override;
  void stopStream() override;
  bool streaming() const override { return snapshot["streaming"].toBool(); }
  bool uploadScript(const HexImage &) override;
  bool uploading() const override { return snapshot["uploading"].toBool(); }
  QString firmware() const override { return snapshot["firmware"].toString(); }
  QString model() const override { return snapshot["model"].toString(); }
  QString usbPath() const override { return snapshot["usb_path"].toString(); }
  bool prepareFirmwareUpdate(const QString &, const QString &) override;
  bool enterDfu(const QString &, const QString &) override;
  bool isRemote() const override { return true; }
  void detach() override;
  bool setOperationActive(bool) override;
  bool stageShared(const QString &, qint64) override;
  void setSharedScript(const QString &) override;
  QJsonObject backendCall(const QString &, const QJsonObject &) override;
  QString error() const { return lastError; }
  bool otherOperation() const override {
    return snapshot["other_operation"].toBool();
  }

private:
  QString path, client, lastError;
  QTimer poll;
  QJsonObject snapshot;
  quint64 serial = 0, lastReceipt = 0;
  QMap<QString, quint64> replyAfter;
  bool attached = false, syncing = false, operation = false,
       refreshDrafts = false;
  bool action(const QString &, QJsonObject = {});
  void sync();
  void accept(const QJsonObject &, bool initial = false);
  void lost(const QString &);
};
