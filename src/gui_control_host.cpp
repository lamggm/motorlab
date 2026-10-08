#include "control_host.h"
#include "control.h"
#include "window.h"
#include <QApplication>
#include <QSignalBlocker>
#include <stdexcept>

class GuiControlHost final : public ControlHost {
  Window &w;
public:
  explicit GuiControlHost(Window &window) : ControlHost(window.session, &window), w(window) {
    connect(&w, &Window::firmwareUpdateFinished, this, &ControlHost::firmwareUpdateFinished);
  }
  QVector<ControlParameter> parameters() const override {
    QVector<ControlParameter> result;
    for (const auto &p : w.parameters)
      result.append({p.key, p.name, p.unit, p.suffix, p.spec, p.channel, p.scale,
                     p.raw, p.editor ? qRound64(p.editor->value() * p.scale) : p.raw,
                     p.known, p.editable, p.dirty});
    return result;
  }
  void stageParameter(int row, qint64 raw) override {
    auto &p = w.parameters[row];
    if (!p.editor) return;
    QSignalBlocker block(p.editor);
    p.editor->setValue(raw / p.scale);
    p.dirty = !p.known || p.raw != raw;
    w.parameterItems[row]->setText(0, p.name + " · " + p.suffix + (p.dirty ? " *" : ""));
  }
  void clearParameter(int row) override { auto &p = w.parameters[row]; p.known = false; p.dirty = false; }
  bool isApplying() const override { return w.applying; }
  int readsPending() const override { return w.readsPending; }
  bool manualDriveActive() const override { return w.manualDriveActive(); }
  bool modalActive() const override { return QApplication::activeModalWidget(); }
  void beginOperation() override {
    w.applying = true; w.scanActive = false; w.changes.clear();
    w.operationExpected.clear(); w.operationTarget.clear();
  }
  void endOperation() override { w.applying = false; }
  void refreshState() override { w.refreshState(); }
  QString noticeText() const override { return w.notice->text(); }
  void notify(const QString &text) override { w.notify(text); }
  void writeLog(const QString &direction, const QString &text) override { w.writeLog(direction, text); }
  QString logText() const override { return w.console->toPlainText(); }
  QString scriptText() const override { return w.script->toPlainText(); }
  void setScript(const QString &text) override { w.script->setPlainText(text); }
  QString selectedModel() const override { return w.offlineModel->currentText(); }
  QString profileSource() const override { return w.profileSource; }
  void setProfileMetadata(const QString &model, const QString &firmware, const QString &source) override {
    w.profileModel = model; w.profileFirmware = firmware; w.profileSource = source;
  }
  bool connectDevice(const QString &mode, const QString &port, int baud) override {
    w.source->setCurrentIndex(mode == "serial" ? 1 : 0);
    w.port->setCurrentText(port); w.baud->setCurrentText(QString::number(baud));
    w.connectSource(); return session.connected();
  }
  QJsonObject view(const QJsonObject &a) override {
    if (a.contains("model")) w.offlineModel->setCurrentText(a["model"].toString());
    if (a.contains("page")) w.selectPage(a["page"].toInt());
    if (a.contains("visible")) { if (a["visible"].toBool()) { w.show(); w.raise(); } else w.hide(); }
    if (a.contains("screenshot_path") && !w.grab().save(a["screenshot_path"].toString(), "PNG"))
      throw std::runtime_error("Falha ao salvar captura.");
    return {{"visible", w.isVisible()}, {"page", w.pages->currentIndex()}, {"screenshot_path", a["screenshot_path"]}};
  }
  int sampleCount() const override { return w.sampleCount(); }
  bool saveProfile(const QString &path) override { return w.saveProfile(path); }
  bool loadProfile(const QString &path, QString *error) override { return w.loadProfile(path, error); }
  bool saveCpr(const QString &path) override { return w.saveCpr(path); }
  bool saveCsv(const QString &path) override { return w.saveCsv(path); }
  void startFirmware(const QString &path, const QString &backup) override {
    bool completed = false;
    const auto connection = connect(this, &ControlHost::firmwareUpdateFinished, this,
                                    [&completed](bool, const QString &) { completed = true; });
    w.showFirmwareUpdate(path, backup, true);
    disconnect(connection);
    if (!completed) emit firmwareUpdateFinished(false, w.notice->text());
  }
  QJsonObject telemetry(int limit) const override;
  void continuousUpdate(const QJsonObject &sample, int channel, qint64 counter) override;
  void continuousFinished() override;
};
ControlServer::ControlServer(Window &window) : ControlServer(*new GuiControlHost(window)) {}

QJsonObject GuiControlHost::telemetry(int limit) const {
        QJsonArray samples;
        for (int n = qMax(0, int(w.recorded.size()) - limit);
             n < w.recorded.size(); ++n) {
          const auto &v = w.recorded[n];
          QJsonObject o{
              {"timestamp", v.timestamp}, {"seconds", v.seconds},
              {"amps1", v.amp1},          {"amps2", v.amp2},
              {"rpm1", v.rpm1},           {"rpm2", v.rpm2},
              {"rpm1_encoder", v.rpm1}, {"rpm2_encoder", v.rpm2},
              {"rpm1_hall", v.hallRpm1}, {"rpm2_hall", v.hallRpm2},
              {"voltage", v.voltage},     {"temperature", v.temperature}};
          o["encoder1"] =
              v.hasEncoder ? QJsonValue(double(v.encoder1)) : QJsonValue();
          o["encoder2"] =
              v.hasEncoder ? QJsonValue(double(v.encoder2)) : QJsonValue();
          o["faults"] =
              v.hasFaults ? QJsonValue(double(v.faults)) : QJsonValue();
          o["flags"] = v.hasFlags ? QJsonValue(double(v.flags)) : QJsonValue();
          samples.append(o);
        }
        return {{"samples", samples},
                {"connected", session.connected()},
                {"source", w.captureDemo ? "demo" : "serial"},
                {"model", w.captureModel},
                {"firmware", w.captureFirmware}};
}

void GuiControlHost::continuousFinished() {
    for (auto label : w.speedSources)
      label->setText("Encoder A/B: ?S · Hall/interno: ?BS");
    for (auto label : w.hallRpm)
      label->setText("—");
    for (auto label : w.metrics)
      label->setText("—");
    w.faults->setText("Atualizando estado após parada…");
    w.continuousHistoryNote->setVisible(false);
    w.chart->setEnabled(true);
    w.ioValues->setEnabled(true);
}

void GuiControlHost::continuousUpdate(const QJsonObject &sample, int channel, qint64 counter) {
  const QStringList metricKeys{"amps1", "amps2", "voltage", "encoder_rpm1", "encoder_rpm2", "temperature"};
  for (int i = 0; i < metricKeys.size(); ++i)
    w.metrics[i]->setText(sample.contains(metricKeys[i])
        ? QString::number(sample[metricKeys[i]].toDouble(), 'f', i < 3 ? 1 : 0) : "—");
  for (auto label : w.speedSources)
    label->setText("Encoder A/B: ?S · Hall/interno: ?BS");
  for (int i = 0; i < w.hallRpm.size(); ++i) {
    const auto key = QString("rpm%1").arg(i + 1);
    w.hallRpm[i]->setText(sample.contains(key) ? QString::number(sample[key].toDouble(), 'f', 0) : "—");
  }
  for (int i = 0; i < w.motorFlags.size(); ++i) {
    const auto key = QString("motor_flags%1").arg(i + 1);
    w.motorFlags[i]->setText(sample.contains(key)
        ? Protocol::motorNames(sample[key].toInt()).join(" · ") : "Estado: aguardando leitura");
  }
  w.faults->setText(sample.contains("faults")
      ? Protocol::faultNames(sample["faults"].toInt()).join(" · ") : "—");
  w.updateFaultSummary(sample["faults"].toInt(-1),
                       sample["motor_flags1"].toInt(-1), sample["motor_flags2"].toInt(-1));
  w.statusFlags->setText(sample.contains("flags")
      ? Protocol::statusNames(sample["flags"].toInt()).join(" · ") : "—");
  w.encoders->setText(QString("Encoder C1: %1 · C2: %2\nHall CB%3: %4")
      .arg(sample.contains("encoder_counter1") ? QString::number(sample["encoder_counter1"].toDouble(), 'f', 0) : "—")
      .arg(sample.contains("encoder_counter2") ? QString::number(sample["encoder_counter2"].toDouble(), 'f', 0) : "—")
      .arg(channel).arg(counter));
  w.continuousHistoryNote->setVisible(true);
  w.chart->setEnabled(false);
  w.ioValues->setEnabled(false);
  for (auto feed : {w.chartFeed, w.diagnosticFeed})
    for (int row = 0; row < feed->rowCount(); ++row)
      feed->item(row, 1)->setText("—");

}
