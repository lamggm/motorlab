#pragma once
#include "hexfile.h"
#include "protocol.h"
#include <QElapsedTimer>
#include <QMap>
#include <QJsonObject>
#include <QObject>
#include <QQueue>
#include <QSocketNotifier>
#include <QTimer>
#include <termios.h>

class Session : public QObject {
  Q_OBJECT
public:
  explicit Session(QObject *parent = nullptr);
  virtual ~Session() override;
  virtual bool openSerial(const QString &path, int baud);
  virtual void openDemo();
  virtual void close();
  virtual bool enqueue(const QString &command);
  virtual bool connected() const { return live; }
  virtual bool isDemo() const { return demo; }
  virtual bool busy() const { return active || !pending.isEmpty(); }
  virtual bool setWritesEnabled(bool enabled);
  virtual bool writesEnabled() const { return writes; }
  virtual bool setMotionEnabled(bool enabled);
  virtual bool motionEnabled() const { return motion; }
  virtual int watchdogMs() const { return watchdog; }
  virtual void cancelMotion();
  virtual bool startStream(int intervalMs);
  virtual void stopStream();
  virtual bool streaming() const { return stream; }
  virtual bool uploadScript(const HexImage &image);
  virtual bool uploading() const { return upload; }
  virtual QString firmware() const { return fid; }
  virtual QString model() const { return trn; }
  virtual QString usbPath() const;
  virtual bool prepareFirmwareUpdate(const QString &expectedModel,
                             const QString &expectedPath);
  virtual bool enterDfu(const QString &expectedModel = {},
                const QString &expectedPath = {});
  virtual bool otherOperation() const { return false; }
  virtual bool isRemote() const { return false; }
  virtual void detach() {}
  virtual bool setOperationActive(bool) { return true; }
  virtual bool stageShared(const QString &, qint64) { return true; }
  virtual void setSharedScript(const QString &) {}
  virtual QJsonObject backendCall(const QString &, const QJsonObject &) { return {}; }
  int timeoutMs = 900;
signals:
  void backendSnapshot(QJsonObject snapshot);
  void received(QString command, QString payload, bool success);
  void traffic(QString direction, QString text);
  void changed();
  void failed(QString message);
  void streamFrame(QString payload);
  void uploadProgress(int done, int total);
  void uploadFinished(bool success, QString message);

private:
  void startNext();
  bool sendRaw(const QString &text);
  void uploadNext();
  void readReady();
  void line(const QByteArray &value);
  void finish(const QString &payload, bool ok);
  void fail(const QString &message);
  QString demoResponse(const Protocol::Command &command);
  int fd = -1;
  termios original{};
  bool restoreTermios = false, live = false, demo = false, writes = false,
       active = false;
  bool completing = false, ownLock = false, ownExclusive = false;
  quint64 generation = 0;
  QSocketNotifier *reader = nullptr;
  QTimer timeout;
  QElapsedTimer demoClock;
  QByteArray input;
  QQueue<Protocol::Command> pending;
  Protocol::Command current;
  QString fid, trn;
  QMap<QString, qint64> demoParams, demoSaved, demoDefaults;
  bool motion = false, stream = false, upload = false;
  int uploadIndex = -1;
  QStringList uploadRecords;
  QTimer streamTimer;
  quint64 demoFaults = 0;
  int watchdog = -1;
  bool firmwareAuthorized = false;
  bool scriptPaused = false;
  quint64 confirmedFaults = 0;
  QElapsedTimer stopStamp;
  QString serialPath;
};
