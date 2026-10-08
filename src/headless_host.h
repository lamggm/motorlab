#pragma once
#include "control_host.h"
#include <QElapsedTimer>
#include <QJsonArray>
#include <QTimer>

class HeadlessHost final : public ControlHost {
  Q_OBJECT
public:
  explicit HeadlessHost(Session &session);
  QVector<ControlParameter> parameters() const override { return params; }
  void stageParameter(int row, qint64 raw) override;
  void clearParameter(int row) override;
  bool isApplying() const override { return applying; }
  int readsPending() const override { return 0; }
  void beginOperation() override { applying = true; scan = false; }
  void endOperation() override { applying = false; }
  QString noticeText() const override { return notice; }
  void notify(const QString &text) override { notice = text; }
  void writeLog(const QString &direction, const QString &text) override;
  QString logText() const override { return log.join('\n'); }
  QString scriptText() const override { return script; }
  void setScript(const QString &text) override { script = text; }
  QString selectedModel() const override { return model; }
  QString profileSource() const override { return source; }
  void setProfileMetadata(const QString &, const QString &, const QString &) override;
  bool connectDevice(const QString &, const QString &, int) override;
  QJsonObject view(const QJsonObject &) override;
  QJsonObject telemetry(int limit) const override;
  int sampleCount() const override { return samples.size(); }
  bool saveProfile(const QString &) override;
  bool loadProfile(const QString &, QString *) override;
  bool saveCpr(const QString &) override;
  bool saveCsv(const QString &) override;
  void continuousUpdate(const QJsonObject &, int, qint64) override;
  void startFirmware(const QString &, const QString &) override;
  bool headless() const override { return true; }
  bool firmwareRunning() const { return firmwareActive; }
private:
  QVector<ControlParameter> params;
  QString model = "SBLG2360T", profileModel, firmware, source = "offline";
  QString captureModel, captureFirmware, captureSource = "offline";
  QString notice = "CLI pronta; desconectada.", script;
  QStringList log;
  QJsonArray samples;
  QMap<QString, QString> readings;
  QTimer poll;
  QElapsedTimer elapsed;
  bool applying = false, scan = false, firmwareActive = false;
  void onReply(const QString &, const QString &, bool);
  void recordFrame();
  void updateUnits();
};
