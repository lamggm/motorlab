#pragma once
#include <QJsonArray>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QLocalServer>
#include <QPointer>
#include <QTimer>
#include <functional>
class Window;
class ControlServer : public QObject {
  Q_OBJECT
public:
  explicit ControlServer(Window &window);
  bool listen(const QString &path);
  QString error() const { return server.errorString(); }
  QJsonObject dispatch(const QString &method, const QJsonObject &args);

private:
  struct Step {
    QString command;
    QString expected;
    bool stop = false;
    bool pauseAttempt = false;
    bool scriptIdle = false;
  };
  Window &w;
  QLocalServer server;
  QJsonArray schemas;
  QMap<QString, QJsonObject> jobs;
  QString active, waiting;
  QList<Step> steps;
  int stepIndex = 0;
  bool tolerateReject = false;
  QTimer tick, deadline;
  QTimer supervision;
  struct Continuous {
    bool enabled = false, running = false, stopping = false, stoppingSequence = false;
    bool operatorStop = false, encoderSpeed = false, fastCapture = false;
    int channel = 1, value = 0, leaseMs = 3000, maxAmps = 10, maxRpm = 600;
    int maxTemperature = 60, noMotionMs = 2000, maxPowerPercent = 25;
    QString token, reason;
    QElapsedTimer lease, motorAck, progress, encoderProgress;
    bool haveCounter = false, haveEncoder = false;
    qint64 encoderCounter = 0;
    qint64 counter = 0;
    quint64 cycles = 0;
    QJsonObject sample;
  } continuous;
  QJsonObject continuousStart(const QJsonObject &args);
  QJsonObject continuousState() const;
  void continuousCycle();
  void continuousObserve(const QString &command, const QString &payload, bool ok);
  void continuousStop(const QString &reason);
  std::function<void()> afterSteps;
  QJsonObject state() const;
  QJsonObject newJob(const QString &kind);
  void finish(bool success, const QString &message,
              const QJsonObject &result = {});
  void next();
  QJsonObject sequence(const QString &kind, const QList<Step> &steps,
                       bool tolerate = false, std::function<void()> done = {});
  void requireIdle() const;
  void authorize(const QJsonObject &args);
  QList<Step> stopped() const;
  void serve();
};
