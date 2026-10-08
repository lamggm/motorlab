#include "fake_controller.h"
#include "remote_session.h"
#include "window.h"
#include <QJsonArray>
#include <QMessageBox>
#include <QProcess>
#include <QSignalSpy>
#include <QSlider>
#include <QTemporaryDir>
#include <QtTest>

class SharedBackendTests : public QObject {
  Q_OBJECT
  QTemporaryDir dir;
  QProcess daemon;
  QString socket;
  QJsonObject call(RemoteSession &s, const QString &method,
                   QJsonObject args = {}) {
    return s.backendCall("motorlab_" + method, args);
  }
  QJsonObject entry(RemoteSession &s, const QString &query) {
    const auto result =
        call(s, "parameters", {{"search", query}, {"limit", 100}})["result"]
            .toObject();
    for (const auto &v : result["entries"].toArray())
      if (v.toObject()["query"].toString() == "~" + query)
        return v.toObject();
    return {};
  }
private slots:
  void init() {
    QVERIFY(dir.isValid());
    socket = dir.filePath("control.sock");
    daemon.start(QCoreApplication::applicationDirPath() + "/motorlabd",
                 {"--control-socket", socket});
    QVERIFY(daemon.waitForStarted());
    QTRY_VERIFY(QFileInfo::exists(socket));
  }
  void cleanup() {
    daemon.terminate();
    QVERIFY(daemon.waitForFinished(3000));
    QFile::remove(socket);
  }
  void parametersAndTelemetryAreShared() {
    FakeController fake;
    fake.overrides["?BS 1"] = "BS=849";
    RemoteSession remote(socket);
    Window gui(nullptr, &remote);
    QVERIFY(remote.attach(false));
    QVERIFY(!remote.connected());
    const auto pid =
        call(remote, "status")["result"].toObject()["pid"].toInteger();
    QCOMPARE(pid, daemon.processId());
    QVERIFY(
        call(remote, "connect", {{"mode", "serial"}, {"port", fake.path}})["ok"]
            .toBool());
    QTRY_COMPARE(remote.model(), QString("RoboG4:SBLMG2360T"));
    QTRY_COMPARE(remote.watchdogMs(), 1000);
    QVERIFY(!remote.writesEnabled());
    QVERIFY(!remote.motionEnabled());
    QVERIFY(remote.enqueue("~ALIM 1"));
    auto editor = gui.findChild<QDoubleSpinBox *>("draft_ALIM_1");
    QTRY_COMPARE(editor->value(), 20.0);
    QVERIFY(remote.setWritesEnabled(true));
    editor->setValue(15.0);
    QCOMPARE(entry(remote, "ALIM 1")["draft_raw"].toInt(), 150);
    QVERIFY(entry(remote, "ALIM 1")["dirty"].toBool());
    const auto staged =
        call(remote, "stage",
             {{"changes",
               QJsonArray{QJsonObject{{"query", "~ALIM 1"}, {"raw", 120}}}}});
    QVERIFY2(staged["ok"].toBool(), qPrintable(staged["error"].toString()));
    QTRY_COMPARE(editor->value(), 12.0);
    QTRY_VERIFY(gui.sampleCount() > 0);
    const auto telemetry = call(remote, "telemetry", {{"limit", 1}})["result"]
                               .toObject()["samples"]
                               .toArray()
                               .last()
                               .toObject();
    QCOMPARE(telemetry["rpm1_encoder"].toInt(), 850);
    QCOMPARE(telemetry["rpm1_hall"].toInt(), 849);
    QTRY_VERIFY(gui.findChild<QLabel *>("hallRpm1")->text().contains("849"));
    gui.findChild<QPlainTextEdit *>("scriptEditor")
        ->setPlainText("Print(\"shared\")\n");
    QCOMPARE(call(remote, "script", {{"action", "get"}})["result"]
                 .toObject()["source"]
                 .toString(),
             QString("Print(\"shared\")\n"));
    QVERIFY(call(remote, "script",
                 {{"action", "set"}, {"source", "Print(42)\n"}})["ok"]
                .toBool());
    QTRY_COMPARE(gui.findChild<QPlainTextEdit *>("scriptEditor")->toPlainText(),
                 QString("Print(42)\n"));
    for (const auto &cmd : fake.commands)
      QVERIFY(!cmd.startsWith('!') && !cmd.startsWith('^') &&
              !cmd.startsWith('%'));
  }
  void guiApplyUsesDaemonAndReadback() {
    FakeController fake;
    fake.overrides["?FS"] = "FS=8";
    RemoteSession remote(socket);
    Window gui(nullptr, &remote);
    QVERIFY(remote.attach(false));
    QVERIFY(remote.openSerial(fake.path, 115200));
    QTRY_COMPARE(remote.watchdogMs(), 1000);
    QVERIFY(remote.enqueue("~ALIM 1"));
    auto editor = gui.findChild<QDoubleSpinBox *>("draft_ALIM_1");
    QTRY_COMPARE(editor->value(), 20.0);
    QVERIFY(remote.setWritesEnabled(true));
    editor->setValue(12.0);
    QCOMPARE(entry(remote, "ALIM 1")["draft_raw"].toInt(), 120);
    QTimer accept;
    accept.setInterval(10);
    bool protectedReview = false;
    connect(&accept, &QTimer::timeout, [&] {
      if (auto box =
              qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
        protectedReview =
            !call(remote, "stage",
                  {{"changes", QJsonArray{QJsonObject{{"query", "~ALIM 1"},
                                                      {"raw", 180}}}}})["ok"]
                 .toBool();
        box->button(QMessageBox::Yes)->click();
      }
    });
    accept.start();
    gui.findChild<QPushButton *>("applyButton")->click();
    QVERIFY(protectedReview);
    QTRY_VERIFY2(fake.commands.contains("^ALIM 1 120"),
                 qPrintable(fake.commands.join(" | ")));
    QTRY_COMPARE(entry(remote, "ALIM 1")["raw"].toInt(), 120);
    QTRY_VERIFY(!entry(remote, "ALIM 1")["dirty"].toBool());
    QVERIFY(fake.commands.indexOf("!EX") <
            fake.commands.indexOf("^ALIM 1 120"));
    QVERIFY(!fake.commands.contains("%EESAV"));
    QTRY_VERIFY(
        !call(remote, "status")["result"].toObject()["gui_operation"].toBool());
  }
  void slidersOwnChannelAndNormalCloseSendsZero() {
    FakeController fake;
    RemoteSession remote(socket), observer(socket);
    Window gui(nullptr, &remote);
    QVERIFY(remote.attach(false));
    QVERIFY(observer.attach(false));
    QVERIFY(remote.openSerial(fake.path, 115200));
    QTRY_COMPARE(remote.watchdogMs(), 1000);
    QTRY_VERIFY(gui.sampleCount() > 0);
    QVERIFY(remote.setWritesEnabled(true));
    QVERIFY(remote.setMotionEnabled(true));
    gui.findChild<QSlider *>("motorCommand1")->setValue(500);
    QTRY_VERIFY(fake.commands.count("!G 1 500") >= 3);
    QVERIFY(!call(observer, "query", {{"command", "?S"}})["ok"].toBool());
    QVERIFY(!observer.enqueue("!G 1 300"));
    observer.detach();
    QTest::qWait(250);
    QVERIFY(remote.connected());
    QVERIFY(!fake.commands.contains("!EX"));
    gui.close();
    QTRY_VERIFY(fake.commands.contains("!G 1 0"));
    QVERIFY(!fake.commands.contains("!EX"));
    QVERIFY(call(remote, "status")["result"].toObject()["connected"].toBool());
  }
  void exclusiveOperationAndEmergencyPreemption() {
    FakeController fake;
    RemoteSession owner(socket), observer(socket);
    QVERIFY(owner.attach(false));
    QVERIFY(observer.attach(false));
    QVERIFY(owner.openSerial(fake.path, 115200));
    QTRY_COMPARE(owner.watchdogMs(), 1000);
    QTRY_VERIFY(!owner.busy());
    QVERIFY(owner.setOperationActive(true));
    QVERIFY(!observer.setOperationActive(true));
    QVERIFY(!call(observer, "query", {{"command", "?S"}})["ok"].toBool());
    QVERIFY(owner.enqueue("~ALIM 1"));
    QTest::qWait(200);
    QVERIFY(owner.setOperationActive(false));
    QVERIFY(call(observer, "query", {{"command", "?S"}})["ok"].toBool());
    QTRY_VERIFY(!call(owner, "status")["result"].toObject()["busy"].toBool());
    QVERIFY(owner.setWritesEnabled(true));
    QVERIFY(owner.setMotionEnabled(true));
    QVERIFY(owner.enqueue("!G 1 500"));
    QTRY_VERIFY(fake.commands.contains("!G 1 500"));
    QVERIFY(call(observer, "stop")["ok"].toBool());
    QTRY_VERIFY(fake.commands.contains("!EX"));
    QTRY_VERIFY(!owner.motionEnabled());
  }
  void expiredClientStopsOwnedMotion() {
    FakeController fake;
    RemoteSession remote(socket);
    QVERIFY(remote.attach(false));
    QVERIFY(remote.openSerial(fake.path, 115200));
    QTRY_COMPARE(remote.watchdogMs(), 1000);
    QTRY_VERIFY(!remote.busy());
    QVERIFY(remote.setWritesEnabled(true));
    QVERIFY(remote.setMotionEnabled(true));
    // Raw private client simulates a GUI process that dies without detach.
    const QString token = "12345678-1234-1234-1234-123456789abc";
    auto gui = [&](QString action, QJsonObject args = {}) {
      args["client"] = token;
      args["action"] = action;
      return remote.backendCall("gui_session", args);
    };
    QVERIFY(gui("attach")["ok"].toBool());
    QVERIFY(gui("enqueue", {{"command", "!G 1 500"}})["ok"].toBool());
    QTRY_VERIFY(fake.commands.contains("!G 1 500"));
    QTRY_VERIFY_WITH_TIMEOUT(fake.commands.contains("!EX"), 4000);
    QVERIFY(!gui("enqueue", {{"command", "!G 1 500"}})["ok"].toBool());
    QCOMPARE(fake.commands.count("!G 1 500"), 1);
  }
  void expiredObserverAndReaderDoNotStopOthers() {
    FakeController fake;
    RemoteSession remote(socket);
    QVERIFY(remote.attach(false));
    QVERIFY(remote.openSerial(fake.path, 115200));
    QTRY_COMPARE(remote.watchdogMs(), 1000);
    QTRY_VERIFY(!remote.busy());
    const QString token = "12345678-1234-1234-1234-123456789abc";
    auto gui = [&](QString action, QJsonObject args = {}) {
      args["client"] = token;
      args["action"] = action;
      return remote.backendCall("gui_session", args);
    };
    QVERIFY(gui("attach")["ok"].toBool());
    QVERIFY(gui("operation", {{"enabled", true}})["ok"].toBool());
    QVERIFY(gui("enqueue", {{"command", "~ALIM 1"}})["ok"].toBool());
    QTest::qWait(2300);
    QVERIFY(!fake.commands.contains("!EX"));
    QVERIFY(remote.connected());
    QVERIFY(
        !call(remote, "status")["result"].toObject()["gui_operation"].toBool());
  }
  void graphicalEmergencyRevokesAnotherClientTransaction() {
    FakeController fake;
    fake.fragment = false;
    RemoteSession owner(socket), observer(socket);
    Window observerGui(nullptr, &observer);
    QVERIFY(owner.attach(false));
    QVERIFY(observer.attach(false));
    QVERIFY(owner.openSerial(fake.path, 115200));
    QTRY_COMPARE(owner.watchdogMs(), 1000);
    QTRY_VERIFY(!owner.busy());
    auto emergency = observerGui.findChild<QPushButton *>("emergencyStop");
    QTRY_VERIFY(emergency->isEnabled());
    QSignalSpy failures(&owner, &Session::failed);
    QVERIFY(owner.setOperationActive(true));
    emergency->click();
    QTRY_VERIFY(
        !call(owner, "status")["result"].toObject()["gui_operation"].toBool());
    QTRY_VERIFY(!failures.isEmpty());
    QVERIFY(failures.first()[0].toString().contains("Exclusividade revogada"));
    QVERIFY(!owner.motionEnabled());
    QVERIFY(!fake.commands.contains("%EESAV"));
  }
  void earlierReplyIsNotAnAckForNewCommand() {
    RemoteSession remote(socket);
    QVERIFY(remote.attach(false));
    remote.openDemo();
    QTRY_COMPARE(remote.watchdogMs(), 1000);
    QTest::qWait(300);
    QSignalSpy replies(&remote, &Session::received);
    const auto started = call(remote, "query", {{"command", "~MXRPM 1"}});
    QVERIFY(started["ok"].toBool());
    const auto id = started["result"].toObject()["job_id"].toString();
    QTest::qSleep(1000); // drain any already queued demo scan without
                         // delivering this client's events
    QCOMPARE(call(remote, "job", {{"job_id", id}})["result"]
                 .toObject()["state"]
                 .toString(),
             QString("succeeded"));
    QVERIFY(remote.enqueue("~MXRPM 1"));
    QTest::qSleep(60);
    auto count = [&] {
      int n = 0;
      for (const auto &r : replies)
        if (r[0].toString() == "~MXRPM 1")
          ++n;
      return n;
    };
    QTRY_COMPARE(count(), 1);
    QTest::qWait(200);
    QCOMPARE(count(), 1);
  }
  void backendLossDoesNotReconnectOrReplay() {
    RemoteSession remote(socket);
    QVERIFY(remote.attach(false));
    remote.openDemo();
    QTRY_VERIFY(remote.connected());
    daemon.terminate();
    QVERIFY(daemon.waitForFinished(3000));
    QTRY_VERIFY(!remote.connected());
    QVERIFY(!remote.motionEnabled());
    QVERIFY(!remote.enqueue("!G 1 500"));
    QVERIFY(daemon.state() == QProcess::NotRunning);
    daemon.start(QCoreApplication::applicationDirPath() + "/motorlabd",
                 {"--control-socket", socket});
    QVERIFY(daemon.waitForStarted());
    QTest::qWait(300);
    QVERIFY(!remote.connected());
  }
};
QTEST_MAIN(SharedBackendTests)
#include "shared_backend_tests.moc"
