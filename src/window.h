#pragma once
#include "catalog.h"
#include "chart.h"
#include "session.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabBar>
#include <QTableWidget>
#include <QTreeWidget>

class ControlServer;
class Window : public QMainWindow {
  Q_OBJECT
  friend class ControlServer;
  friend class GuiControlHost;
signals:
  void firmwareUpdateFinished(bool success, QString message);
public:
  explicit Window(QWidget *parent = nullptr, Session *backend = nullptr);
  void startDemo();
  void selectPage(int index);
  bool saveCsv(const QString &path);
  bool saveSnapshot(const QString &path);
  bool saveProfile(const QString &path);
  bool saveCpr(const QString &path);
  void showCpr(bool compare = false, const QString &inputPath = QString());
  bool loadProfile(const QString &path, QString *error = nullptr);
  int sampleCount() const { return recorded.size(); }

protected:
  void closeEvent(QCloseEvent *event) override;

private:
  struct Parameter {
    QString key, name, unit, suffix;
    const ConfigSpec *spec = nullptr;
    int channel = 1;
    double scale = 1;
    qint64 raw = 0;
    bool known = false, editable = false, dirty = false;
    QDoubleSpinBox *editor = nullptr;
    QComboBox *choices = nullptr;
  };
  void buildUi();
  QWidget *monitorPage();
  QWidget *parametersPage();
  QWidget *consolePage();
  QWidget *scriptsPage();
  QWidget *diagnosticsPage();
  QWidget *aboutPage();
  QTableWidget *signalTable(const QString &name);
  void exportTelemetry();
  void connectSource();
  void refreshPorts();
  void refreshState();
  void poll();
  void onReply(const QString &cmd, const QString &payload, bool ok);
  void readParameters();
  void updateParameterRow(int row);
  void applyParameters();
  void applyNext();
  void readNextParameter();
  void maintenance(const QString &command, const QString &title,
                   const QString &description);
  void maintenanceReply(const QString &command, const QString &payload,
                        bool ok);
  void runtime(const QString &command, const QString &description);
  void scriptUpload();
  void showFirmwareUpdate(const QString &inputPath = {}, const QString &backupPath = {}, bool automated = false);
  void showCanInspector();
  void showScriptSimulator();
  void compileScript(bool simulate = false);

  void startStreamingWhenIdle(int remaining);
  void toggleStreaming();
  void onStream(const QString &payload);
  void showSensorSetup();
  void saveLog();
  void executeDocumentedCommand(const QString &text);
  void completeApply(bool success, const QString &message);
  void updateSamples();
  void notify(const QString &message);
  void writeLog(const QString &direction, const QString &message);
  Session &session;
  void syncBackend(const QJsonObject &snapshot);
  bool syncingBackend = false, reviewingOperation = false;
  QString sharedSampleTimestamp;
  QTimer pollTimer;
  QElapsedTimer elapsed;
  QComboBox *source = nullptr, *port = nullptr, *baud = nullptr;
  QPushButton *connection = nullptr, *portRefresh = nullptr;
  QLabel *status = nullptr, *device = nullptr, *firmware = nullptr,
         *notice = nullptr;
  QLabel *detectedModel = nullptr;
  QLabel *footerFirmware = nullptr;
  QLabel *encoders = nullptr, *faults = nullptr, *statusFlags = nullptr;
  QVector<QLabel *> metrics;
  QVector<QLabel *> motorFlags;
  QVector<QLabel *> speedSources;
  QVector<QLabel *> hallRpm;
  QLabel *continuousHistoryNote = nullptr;
  QLabel *digitalOutputs = nullptr;
  Chart *chart = nullptr;
  Chart *diagnosticChart = nullptr;
  QTabBar *navigation = nullptr;
  QStackedWidget *pages = nullptr;
  QTreeWidget *parameterTree = nullptr, *ioTree = nullptr;
  QComboBox *offlineModel = nullptr, *streamRate = nullptr;
  QCheckBox *motionEnabled = nullptr;
  QLabel *manualControlHint = nullptr;
  void refreshManualControls();
  void updateFaultSummary(int ff, int fm1, int fm2);
  QString motorAlarmSummary;
  void stopManualDrive(const QString &reason = "Controle manual desabilitado.");
  bool manualDriveActive() const;
  QVector<QTimer *> manualRepeats;
  QElapsedTimer manualFaultAge;
  bool manualStopping = false;
  int manualFaults = -1;
  QVector<QLabel *> motorCommandStatus;
  QPushButton *streamButton = nullptr;
  QVector<QWidget *> liveActions;
  QVector<QSlider *> motorSliders;
  QVector<QPushButton *> motorSend, motorStop;
  QTableWidget *ioValues = nullptr;
  QLabel *readProgress = nullptr;
  int nextParameter = 0;
  QString operationTarget, operationExpected;
  int operationPhase = 0;
  HexImage pendingHex;
  QVector<QTreeWidgetItem *> parameterItems;
  QTableWidget *chartFeed = nullptr, *diagnosticFeed = nullptr;
  QVector<double> minimums = QVector<double>(6, qQNaN()),
                  maximums = QVector<double>(6, qQNaN());
  QCheckBox *editEnabled = nullptr;
  QPushButton *readParams = nullptr, *applyParams = nullptr,
              *snapshot = nullptr;
  QPlainTextEdit *console = nullptr, *script = nullptr;
  QPlainTextEdit *diagnosticLog = nullptr;
  QLineEdit *query = nullptr;
  QPushButton *sendQuery = nullptr;
  QVector<Parameter> parameters;
  QMap<QString, QString> readings;
  QVector<Sample> recorded;
  bool scanActive = false, applying = false, verifyWrite = false;
  int readsPending = 0;
  QVector<QPair<int, qint64>> changes;
  int applyIndex = 0;
  bool captureDemo = false;
  QString captureFirmware, captureModel;
  QString profileModel, profileFirmware, profileSource = "offline";
};
