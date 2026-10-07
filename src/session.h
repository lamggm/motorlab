#pragma once
#include "hexfile.h"
#include "protocol.h"
#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QQueue>
#include <QSocketNotifier>
#include <QTimer>
#include <termios.h>

class Session : public QObject {
  Q_OBJECT
public:
  explicit Session(QObject *parent = nullptr);
  ~Session() override;
  bool openSerial(const QString &path, int baud);
  void openDemo();
  void close();
  bool enqueue(const QString &command);
  bool connected() const { return live; }
  bool isDemo() const { return demo; }
  bool busy() const { return active || !pending.isEmpty(); }
  bool setWritesEnabled(bool enabled);
  bool writesEnabled() const { return writes; }
  bool setMotionEnabled(bool enabled);
  bool motionEnabled() const { return motion; }
  int watchdogMs() const { return watchdog; }
  void cancelMotion();
  bool startStream(int intervalMs);
  void stopStream();
  bool streaming() const { return stream; }
  bool uploadScript(const HexImage &image);
  bool uploading() const { return upload; }
  QString firmware() const { return fid; }
  QString model() const { return trn; }
  QString usbPath() const;
  bool prepareFirmwareUpdate(const QString &expectedModel,
                             const QString &expectedPath);
  bool enterDfu(const QString &expectedModel = {},
                const QString &expectedPath = {});
  int timeoutMs = 900;
signals:
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
