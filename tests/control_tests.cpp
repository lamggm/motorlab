#include "control.h"
#include "fake_controller.h"
#include "window.h"
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>
class ControlTests : public QObject {
  Q_OBJECT
  QJsonObject call(ControlServer &c, const QString &method,
                   QJsonObject args = {}) {
    return c.dispatch("motorlab_" + method, args);
  }
  QJsonObject result(ControlServer &c, const QString &id) {
    return call(c, "job", {{"job_id", id}})["result"].toObject();
  }
  QString start(ControlServer &c, const QString &method,
                QJsonObject args = {}) {
    const auto r = call(c, method, args);
    if (!r["ok"].toBool())
      qWarning() << r;
    return r["result"].toObject()["job_id"].toString();
  }
private slots:
  void readJobDoesNotRevokeManualPermission() {
    FakeController fake;
    Window w;
    ControlServer c(w);
    QVERIFY(call(c, "connect", {{"mode", "serial"}, {"port", fake.path}})["ok"].toBool());
    auto session = w.findChild<Session *>();
    QTRY_VERIFY(!session->model().isEmpty());
    auto id = start(c, "query", {{"command", "~RWD"}});
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("succeeded"));
    QVERIFY(session->setWritesEnabled(true));
    QVERIFY(session->setMotionEnabled(true));
    id = start(c, "query", {{"command", "?S"}});
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("succeeded"));
    QVERIFY(session->writesEnabled());
    QVERIFY(session->motionEnabled());
    QVERIFY(!fake.commands.contains("!EX"));
    fake.overrides["?BS 1"] = "-";
    id = start(c, "query", {{"command", "?BS 1"}});
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("failed"));
    QVERIFY(session->writesEnabled());
    QVERIFY(session->motionEnabled());
    QVERIFY(!fake.commands.contains("!EX"));
  }
  void bothManualChannelsRepeatAndMcpCannotInterruptThem() {
    FakeController fake;
    Window w;
    ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    auto session = w.findChild<Session *>();
    QTRY_VERIFY(!session->model().isEmpty());
    auto id = start(c, "query", {{"command", "~RWD"}});
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("succeeded"));
    QTRY_VERIFY(w.sampleCount() > 0);
    QVERIFY(session->setWritesEnabled(true));
    QVERIFY(session->setMotionEnabled(true));
    w.findChild<QSlider *>("motorCommand1")->setValue(40);
    w.findChild<QSlider *>("motorCommand2")->setValue(-30);
    QTRY_VERIFY(fake.commands.count("!G 1 40") >= 3);
    QTRY_VERIFY(fake.commands.count("!G 2 -30") >= 3);
    const auto denied = call(c, "query", {{"command", "?S"}});
    QVERIFY(!denied["ok"].toBool());
    QVERIFY(session->motionEnabled());
    QVERIFY(!fake.commands.contains("!EX"));
    w.findChild<QPushButton *>("motorStop1")->click();
    w.findChild<QPushButton *>("motorStop2")->click();
    QTRY_VERIFY(fake.commands.contains("!G 1 0"));
    QTRY_VERIFY(fake.commands.contains("!G 2 0"));
    id = start(c, "query", {{"command", "?S"}});
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("succeeded"));
    QVERIFY(session->motionEnabled());
    QVERIFY(!fake.commands.contains("!EX"));
  }
  void rejectedPauseNeedsFreshIdleStatus_data() {
    QTest::addColumn<QString>("fs");
    QTest::addColumn<bool>("allowed");
    QTest::newRow("idle") << QString("FS=8") << true;
    QTest::newRow("script-active") << QString("FS=128") << false;
    QTest::newRow("setup-active") << QString("FS=256") << false;
    QTest::newRow("rejected-status") << QString("-") << false;
    QTest::newRow("malformed-status") << QString("FS=8:0") << false;
  }
  void rejectedPauseNeedsFreshIdleStatus() {
    QFETCH(QString, fs);
    QFETCH(bool, allowed);
    FakeController fake;
    fake.overrides["!R 0"] = "-";
    fake.overrides["?FS"] = fs;
    Window w;
    ControlServer c(w);
    QVERIFY(call(c, "connect", {{"mode", "serial"}, {"port", fake.path}})["ok"].toBool());
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    auto id = start(c, "read_parameters", {{"queries", QJsonArray{"~EPPR 1"}}});
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("succeeded"));
    QVERIFY(call(c, "stage", {{"changes", QJsonArray{QJsonObject{{"query", "~EPPR 1"}, {"raw", 1000}}}}})["ok"].toBool());
    const auto st = call(c, "status")["result"].toObject();
    fake.commands.clear();
    id = start(c, "apply", {{"confirm", true}, {"expected_model", st["model"]}, {"expected_firmware", st["firmware"]}});
    QTRY_VERIFY(result(c, id)["state"].toString() != "running");
    QCOMPARE(result(c, id)["success"].toBool(), allowed);
    QCOMPARE(fake.commands.contains("^EPPR 1 1000"), allowed);
    if (allowed) {
      QVERIFY(fake.commands.indexOf("?FF") < fake.commands.indexOf("?FS"));
      QVERIFY(fake.commands.indexOf("?FS") < fake.commands.indexOf("^EPPR 1 1000"));
    }
    QVERIFY(!fake.commands.contains("!MG"));
    QVERIFY(!fake.commands.contains("%EESAV"));
  }
  void eepromGuard_data() {
    QTest::addColumn<QString>("pauseReply");
    QTest::addColumn<QString>("stopReply");
    QTest::addColumn<QString>("ff");
    QTest::addColumn<QString>("fs");
    QTest::addColumn<QString>("saveReply");
    QTest::addColumn<bool>("sent");
    QTest::addColumn<bool>("saved");
    QTest::newRow("no-script") << "-" << "+" << "FF=16" << "FS=8" << "+" << true << true;
    QTest::newRow("paused-script") << "+" << "+" << "FF=16" << "FS=8" << "+" << true << true;
    QTest::newRow("stop-rejected") << "-" << "-" << "FF=16" << "FS=8" << "+" << false << false;
    QTest::newRow("stop-unconfirmed") << "-" << "+" << "FF=0" << "FS=8" << "+" << false << false;
    QTest::newRow("malformed-faults") << "-" << "+" << "FF=-1" << "FS=8" << "+" << false << false;
    QTest::newRow("script-active") << "-" << "+" << "FF=16" << "FS=128" << "+" << false << false;
    QTest::newRow("setup-active") << "-" << "+" << "FF=16" << "FS=256" << "+" << false << false;
    QTest::newRow("status-rejected") << "-" << "+" << "FF=16" << "-" << "+" << false << false;
    QTest::newRow("status-malformed") << "-" << "+" << "FF=16" << "FS=8:0" << "+" << false << false;
    QTest::newRow("save-rejected") << "-" << "+" << "FF=16" << "FS=8" << "-" << true << false;
  }
  void eepromGuard() {
    QFETCH(QString, pauseReply);
    QFETCH(QString, stopReply);
    QFETCH(QString, ff);
    QFETCH(QString, fs);
    QFETCH(QString, saveReply);
    QFETCH(bool, sent);
    QFETCH(bool, saved);
    FakeController endpoint;
    endpoint.overrides["!R 0"] = pauseReply;
    endpoint.overrides["!EX"] = stopReply;
    endpoint.overrides["?FF"] = ff;
    endpoint.overrides["?FS"] = fs;
    endpoint.overrides["%EESAV"] = saveReply;
    Window w;
    ControlServer c(w);
    QVERIFY(call(c, "connect", {{"mode", "serial"}, {"port", endpoint.path}})["ok"].toBool());
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    const auto st = call(c, "status")["result"].toObject();
    endpoint.commands.clear();
    const auto id = start(c, "maintenance", {{"command", "%EESAV"}, {"confirm", true},
                          {"expected_model", st["model"]}, {"expected_firmware", st["firmware"]}});
    QVERIFY(!id.isEmpty());
    QTRY_VERIFY(result(c, id)["state"].toString() != "running");
    QCOMPARE(result(c, id)["success"].toBool(), saved);
    QCOMPARE(endpoint.commands.contains("%EESAV"), sent);
    if (sent) {
      const int pause = endpoint.commands.indexOf("!R 0");
      const int stop = endpoint.commands.indexOf("!EX", pause + 1);
      const int faults = endpoint.commands.indexOf("?FF", stop + 1);
      const int flags = endpoint.commands.indexOf("?FS", faults + 1);
      const int save = endpoint.commands.indexOf("%EESAV", flags + 1);
      QVERIFY(pause >= 0 && stop > pause && faults > stop && flags > faults && save > flags);
      QCOMPARE(endpoint.commands.count("%EESAV"), 1);
    }
    QVERIFY(!endpoint.commands.contains("!MG"));
    for (const auto &command : endpoint.commands)
      QVERIFY(!command.startsWith('^'));
    QVERIFY(!w.findChild<Session *>()->motionEnabled());
  }
  void schemaOfflineAndLocalDrafts() {
    Window w;
    ControlServer c(w);
    QVERIFY(!call(c, "status")["result"].toObject()["connected"].toBool());
    QVERIFY(!call(c, "connect", {{"mode", "serial"}, {"baud", "115200"}})["ok"]
                 .toBool());
    QVERIFY(!call(c, "status", {{"extra", true}})["ok"].toBool());
    QVERIFY(!call(c, "execute",
                  {{"command", "!G 1 30"}, {"duration_ms", 1.5}})["ok"]
                 .toBool());
    auto staged =
        call(c, "stage",
             {{"changes",
               QJsonArray{QJsonObject{{"query", "~ALIM 1"}, {"raw", 123}}}}});
    QVERIFY(staged["ok"].toBool());
    QVERIFY(!call(c, "apply")["ok"].toBool());
    QCOMPARE(call(c, "parameters", {{"search", "ALIM 1"}})["result"]
                 .toObject()["entries"]
                 .toArray()[0]
                 .toObject()["draft_raw"]
                 .toInt(),
             123);
    QVERIFY(!call(c, "stage",
                  {{"changes",
                    QJsonArray{QJsonObject{{"query", "~ALIM 1"}, {"raw", 140}},
                               QJsonObject{{"query", "~INVALID"},
                                           {"raw", 12}}}}})["ok"]
                 .toBool());
    QCOMPARE(call(c, "parameters", {{"search", "ALIM 1"}})["result"]
                 .toObject()["entries"]
                 .toArray()[0]
                 .toObject()["draft_raw"]
                 .toInt(),
             123);
  }
  void realSessionRequiresExactIdentityAndVerifiesRam() {
    FakeController fake;
    Window w;
    ControlServer c(w);
    QVERIFY(call(c, "connect", {{"mode", "serial"}, {"port", fake.path}})["ok"]
                .toBool());
    QTRY_VERIFY(!call(c, "status")["result"]
                     .toObject()["firmware"]
                     .toString()
                     .isEmpty());
    QTRY_VERIFY(
        !call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    for (const auto &cmd : fake.commands)
      QVERIFY2(!Protocol::command(cmd).write, qPrintable(cmd));
    auto id = start(c, "read_parameters", {{"queries", QJsonArray{"~ALIM 1"}}});
    QVERIFY(!id.isEmpty());
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("succeeded"));
    QVERIFY(call(c, "stage",
                 {{"changes", QJsonArray{QJsonObject{{"query", "~ALIM 1"},
                                                     {"raw", 125}}}}})["ok"]
                .toBool());
    QVERIFY(!call(c, "apply")["ok"].toBool());
    QVERIFY(!call(c, "apply",
                  {{"confirm", true},
                   {"expected_model", "RoboG4:SBLG2360T"},
                   {"expected_firmware", "wrong"}})["ok"]
                 .toBool());
    QVERIFY(!fake.commands.contains("^ALIM 1 125"));
    auto st = call(c, "status")["result"].toObject();
    QJsonObject grant{{"confirm", true},
                      {"expected_model", st["model"]},
                      {"expected_firmware", st["firmware"]}};
    id = start(c, "apply", grant);
    QVERIFY(!id.isEmpty());
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("succeeded"));
    QCOMPARE(fake.alim, 125);
    QVERIFY(fake.commands.indexOf("!R 0") < fake.commands.indexOf("!EX"));
    QVERIFY(fake.commands.indexOf("!EX") <
            fake.commands.indexOf("^ALIM 1 125"));
    QVERIFY(!call(c, "status")["result"].toObject()["writes_enabled"].toBool());
    QVERIFY(!fake.commands.contains("%EESAV"));
    auto replies = result(c, id)["replies"].toArray();
    QCOMPARE(replies.last().toObject()["payload"].toString(), QString("125"));
  }
  void failureNeverClaimsWriteSucceeded() {
    FakeController fake;
    Window w;
    ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(
        !call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    auto id = start(c, "read_parameters", {{"queries", QJsonArray{"~ALIM 1"}}});
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("succeeded"));
    call(c, "stage",
         {{"changes",
           QJsonArray{QJsonObject{{"query", "~ALIM 1"}, {"raw", 123}}}}});
    auto st = call(c, "status")["result"].toObject();
    QJsonObject grant{{"confirm", true},
                      {"expected_model", st["model"]},
                      {"expected_firmware", st["firmware"]}};
    fake.overrides["?FF"] = "FF=0";
    id = start(c, "apply", grant);
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("failed"));
    QVERIFY(!fake.commands.contains("^ALIM 1 123"));
    fake.overrides.remove("?FF");
    fake.overrides["~ALIM 1"] = "ALIM=200";
    id = start(c, "apply", grant);
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("failed"));
    QVERIFY(fake.commands.contains("^ALIM 1 123"));
    QVERIFY(!call(c, "status")["result"].toObject()["writes_enabled"].toBool());
  }
  void boundedMotionAndStop() {
    FakeController fake;
    Window w;
    ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(
        !call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    auto st = call(c, "status")["result"].toObject();
    QJsonObject args{{"confirm", true},
                     {"expected_model", st["model"]},
                     {"expected_firmware", st["firmware"]},
                     {"command", "!G 1 20"},
                     {"duration_ms", 100},
                     {"release", true}};
    auto id = start(c, "execute", args);
    QVERIFY(!id.isEmpty());
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("succeeded"));
    QVERIFY(fake.commands.indexOf("~RWD") < fake.commands.indexOf("!G 1 20"));
    QVERIFY(fake.commands.indexOf("!MG") < fake.commands.indexOf("!G 1 20"));
    QVERIFY(fake.commands.indexOf("!EX") > fake.commands.indexOf("!G 1 20"));
    QVERIFY(fake.faults & 16);
    QVERIFY(!call(c, "status")["result"].toObject()["motion_enabled"].toBool());
    fake.params["RWD"] = 0;
    id = start(c, "execute", args);
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("failed"));
    QCOMPARE(fake.commands.count("!G 1 20"), 1);
  }
  void motionStopsBeforeRejectedScriptCleanup() {
    FakeController fake;
    fake.overrides["!R 0"] = "-";
    Window w;
    ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    const auto st = call(c, "status")["result"].toObject();
    const auto id = start(c, "execute", {{"confirm", true},
        {"expected_model", st["model"]}, {"expected_firmware", st["firmware"]},
        {"command", "!G 1 100"}, {"duration_ms", 100}});
    QVERIFY(!id.isEmpty());
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("failed"));
    const auto replies = result(c, id)["replies"].toArray();
    QCOMPARE(replies.size(), 5);
    QCOMPARE(replies[2].toObject()["command"].toString(), QString("!EX"));
    QVERIFY(replies[2].toObject()["ok"].toBool());
    QCOMPARE(replies[3].toObject()["command"].toString(), QString("?FF"));
    QCOMPARE(replies[3].toObject()["payload"].toString(), QString("16"));
    QCOMPARE(replies[4].toObject()["command"].toString(), QString("!R 0"));
    QVERIFY(!replies[4].toObject()["ok"].toBool());
    QVERIFY(fake.faults & 16);
    QCOMPARE(fake.commands.count("!G 1 100"), 1);
    QVERIFY(!call(c, "status")["result"].toObject()["motion_enabled"].toBool());
    QVERIFY(!call(c, "status")["result"].toObject()["writes_enabled"].toBool());
  }
  QJsonObject continuousArgs(ControlServer &c) {
    const auto st = call(c, "status")["result"].toObject();
    return {{"action", "start"}, {"channel", 1}, {"value", 100},
            {"lease_ms", 1000}, {"no_motion_ms", 5000},
            {"confirm", true}, {"expected_model", st["model"]},
            {"expected_firmware", st["firmware"]}};
  }
  void encoderSpeedSupervision_data() {
    QTest::addColumn<QString>("scenario");
    for (const auto &s : {"normal", "rated-positive", "rated-negative", "rated-overcurrent", "rated-excess-power", "rated-overspeed", "wrong-mode", "wrong-feedback", "excess-current-limit", "excess-power", "encoder-reverse", "encoder-overspeed", "encoder-missing", "encoder-stuck"})
      QTest::newRow(s) << QString(s);
  }
  void encoderSpeedSupervision() {
    QFETCH(QString, scenario);
    FakeController fake;
    fake.alim = 30;
    fake.params["MMOD 1"] = 1;
    fake.params["EMOD 1"] = 18;
    fake.params["EPPR 1"] = 1000;
    fake.params["MXPF 1"] = fake.params["MXPR 1"] = 25;
    fake.params["MXRPM 1"] = 200;
    fake.overrides["?A"] = "A=2:0";
    fake.overrides["?S"] = "S=0:0";
    fake.overrides["?F 1"] = "F=100";
    fake.overrides["?RMP 1"] = "RMP=100";
    if (scenario == "wrong-mode") fake.params["MMOD 1"] = 0;
    if (scenario == "wrong-feedback") fake.params["EMOD 1"] = 0;
    if (scenario == "excess-current-limit") fake.alim = 160;
    if (scenario == "excess-power") fake.params["MXPF 1"] = 100;
    const bool rated = scenario.startsWith("rated-");
    const int target = scenario == "rated-negative" ? -3000 : rated ? 3000 : 100;
    const QString motorCommand = QString("!S 1 %1").arg(target);
    if (rated) {
      fake.params["MXRPM 1"] = 3000;
      fake.params["MXPF 1"] = fake.params["MXPR 1"] = 100;
    }
    Window w; ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    auto args = continuousArgs(c);
    args["mode"] = "encoder_speed";
    args["max_current_a"] = 3;
    if (rated) { fake.alim = 115; args["max_current_a"] = 13; }
    args["value"] = target;
    args["max_speed_rpm"] = rated ? 3400 : 250;
    if (rated && scenario != "rated-excess-power") args["max_power_percent"] = 100;
    args["lease_ms"] = 3000;
    args["no_motion_ms"] = 1000;
    const auto id = start(c, "continuous", args);
    QVERIFY(!id.isEmpty());
    if (scenario.startsWith("wrong-") || scenario.startsWith("excess-") || scenario == "rated-excess-power") {
      QTRY_COMPARE(result(c, id)["state"].toString(), QString("failed"));
      QVERIFY(!fake.commands.contains(motorCommand));
    } else {
      QTRY_VERIFY(fake.commands.contains(motorCommand));
      fake.overrides["?S"] = scenario == "encoder-reverse" ? "S=-40:0"
          : scenario == "encoder-overspeed" ? "S=300:0"
          : scenario == "encoder-missing" ? "-" : scenario == "rated-overspeed" ? "S=3500:0" : QString("S=%1:0").arg(target);
      if (scenario == "rated-overcurrent") fake.overrides["?A"] = "A=135:0";
      QTimer sensor;
      int count = 0;
      connect(&sensor, &QTimer::timeout, &w, [&] {
        fake.overrides["?CB 1"] = "CB=" + QString::number(++count);
        if (scenario != "encoder-stuck") fake.overrides["?C"] = "C=" + QString::number(count * 100) + ":0";
      });
      sensor.start(50);
      if (scenario == "normal" || scenario == "rated-positive" || scenario == "rated-negative") {
        QTRY_VERIFY2(fake.commands.count(motorCommand) >= 3, qPrintable(result(c, id)["message"].toString()));
        QCOMPARE(call(c, "status")["result"].toObject()["continuous"].toObject()["unit"].toString(), QString("encoder_speed_rpm"));
        QVERIFY(call(c, "stop")["ok"].toBool());
        QTRY_COMPARE(result(c, id)["state"].toString(), QString("succeeded"));
      } else QTRY_COMPARE_WITH_TIMEOUT(result(c, id)["state"].toString(), QString("failed"), 3500);
    }
    QVERIFY(result(c, id)["result"].toObject()["stop_confirmed"].toBool());
    QVERIFY(!fake.commands.contains("!G 1 100"));
    QVERIFY(!call(c, "status")["result"].toObject()["motion_enabled"].toBool());
  }
  void fastCaptureKeepsCyclesAndStopsOnMotorLoopError_data() {
    QTest::addColumn<QString>("guard");
    QTest::newRow("loop-error") << QString("LoopErr");
    QTest::newRow("flux-current") << QString("flux");
    QTest::newRow("torque-current") << QString("torque");
  }
  void fastCaptureKeepsCyclesAndStopsOnMotorLoopError() {
    QFETCH(QString, guard);
    FakeController fake;
    fake.alim = 30;
    fake.params["MMOD 1"] = 1; fake.params["EMOD 1"] = 18;
    fake.params["EPPR 1"] = 1000; fake.params["MXRPM 1"] = 200;
    fake.params["MXPF 1"] = fake.params["MXPR 1"] = 25;
    fake.overrides["?A"] = "A=2:0";
    fake.overrides["?S"] = "S=100:0";
    fake.overrides["?F 1"] = "F=100";
    fake.overrides["?RMP 1"] = "RMP=100";
    fake.overrides["?E 1"] = "E=0";
    fake.overrides["?P 1"] = "P=120";
    fake.overrides["?MA 1"] = "MA=0";
    fake.overrides["?MA 2"] = "MA=5";
    Window w; ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    auto args = continuousArgs(c);
    args["mode"] = "encoder_speed"; args["capture_fast"] = true;
    args["max_current_a"] = 3; args["max_speed_rpm"] = 250;
    args["lease_ms"] = 3000;
    const auto id = start(c, "continuous", args);
    QTRY_VERIFY(result(c, id)["samples"].toArray().size() >= 2);
    const auto sample = result(c, id)["samples"].toArray().first().toObject();
    QCOMPARE(sample["feedback_rpm"].toInt(), 100);
    QCOMPARE(sample["ramped_command"].toInt(), 100);
    QCOMPARE(sample["speed_error_rpm"].toInt(), 0);
    QCOMPARE(sample["pwm_per_mille"].toInt(), 120);
    QCOMPARE(sample["flux_amps"].toDouble(), 0.0);
    QCOMPARE(sample["torque_amps"].toDouble(), 0.5);
    QVERIFY(!sample["cycle_started_utc"].toString().isEmpty());
    if (guard == "LoopErr") fake.overrides["?FM 1"] = "FM=4"; // FF remains zero.
    else fake.overrides[guard == "flux" ? "?MA 1" : "?MA 2"] = "MA=-35";
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("failed"));
    QVERIFY(result(c, id)["message"].toString().contains(guard == "LoopErr" ? "LoopErr" : "FOC"));
    QVERIFY(result(c, id)["result"].toObject()["stop_confirmed"].toBool());
    QVERIFY(fake.faults & 16);
    const int sent = fake.commands.count("!S 1 100");
    QTest::qWait(200);
    QCOMPARE(fake.commands.count("!S 1 100"), sent);
  }
  void continuousRangeDoesNotExpandOpenLoop() {
    FakeController fake; Window w; ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    auto args = continuousArgs(c);
    args["value"] = 101;
    QVERIFY(!call(c, "continuous", args)["ok"].toBool());
    args["value"] = 100; args["max_speed_rpm"] = 3400;
    QVERIFY(!call(c, "continuous", args)["ok"].toBool());
    args["max_speed_rpm"] = 600; args["max_current_a"] = 13;
    QVERIFY(!call(c, "continuous", args)["ok"].toBool());
    args["max_speed_rpm"] = 3400;
    args["mode"] = "encoder_speed"; args["value"] = 3001;
    QVERIFY(!call(c, "continuous", args)["ok"].toBool());
    args["value"] = 3000; args["max_speed_rpm"] = 2000;
    QVERIFY(!call(c, "continuous", args)["ok"].toBool());
    QVERIFY(!call(c, "status")["result"].toObject()["continuous"].toObject()["active"].toBool());
    for (const auto &command : fake.commands) QVERIFY(!command.startsWith("!G ") && !command.startsWith("!S "));
  }
  void continuousLeaseExpiresWithoutRenewal() {
    FakeController fake;
    fake.overrides["?A"] = "A=0:0";
    Window w;
    ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    const auto started = call(c, "continuous", continuousArgs(c));
    QVERIFY(started["ok"].toBool());
    const auto id = started["result"].toObject()["job_id"].toString();
    QTRY_COMPARE_WITH_TIMEOUT(result(c, id)["state"].toString(), QString("failed"), 4000);
    QVERIFY(result(c, id)["result"].toObject()["stop_confirmed"].toBool());
    QVERIFY(result(c, id)["message"].toString().contains("expirou"));
    QVERIFY(fake.commands.count("!G 1 100") > 1);
    QVERIFY(fake.faults & 16);
    const auto count = fake.commands.count("!G 1 100");
    QTest::qWait(300);
    QCOMPARE(fake.commands.count("!G 1 100"), count);
    QVERIFY(!call(c, "continuous", {{"action", "renew"},
        {"lease_token", started["result"].toObject()["lease_token"]}})["ok"].toBool());
  }
  void continuousRenewAndOperatorStop() {
    FakeController fake;
    fake.overrides["?A"] = "A=0:0";
    fake.overrides["!R 0"] = "-";
    fake.overrides["?BS 1"] = "BS=123";
    Window w;
    ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    const auto r = call(c, "continuous", continuousArgs(c))["result"].toObject();
    const auto id = r["job_id"].toString();
    QVERIFY(!id.isEmpty());
    QVERIFY(!call(c, "continuous", {{"action", "renew"}, {"lease_token", "wrong"}})["ok"].toBool());
    for (int i = 0; i < 4; ++i) {
      QTest::qWait(400);
      QVERIFY(call(c, "continuous", {{"action", "renew"}, {"lease_token", r["lease_token"]}})["ok"].toBool());
    }
    QCOMPARE(w.findChild<QLabel *>("hallRpm1")->text(), QString("123"));
    QCOMPARE(w.findChild<QLabel *>("metric3")->text(), QString("850"));
    QVERIFY(w.findChild<QCheckBox *>("enableMotion")->isChecked());
    QCOMPARE(w.findChild<QLabel *>("speedSource1")->text(), QString("Encoder A/B: ?S · Hall/interno: ?BS"));
    QVERIFY(!call(c, "execute", {{"command", "!G 2 100"}, {"duration_ms", 100}})["ok"].toBool());
    QVERIFY(call(c, "stop")["ok"].toBool());
    QVERIFY(call(c, "stop")["ok"].toBool());
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("succeeded"));
    QVERIFY(!result(c, id)["continuous"].toObject()["active"].toBool());
    QVERIFY(result(c, id)["result"].toObject()["stop_confirmed"].toBool());
    QVERIFY(fake.commands.count("!G 1 100") > 1);
    QVERIFY(!fake.commands.contains("!R 0"));
    QVERIFY(!call(c, "status")["result"].toObject()["motion_enabled"].toBool());
    QCOMPARE(w.findChild<QLabel *>("speedSource1")->text(), QString("Encoder A/B: ?S · Hall/interno: ?BS"));
    QVERIFY(!w.findChild<QCheckBox *>("enableMotion")->isChecked());
  }
  void continuousGuard_data() {
    QTest::addColumn<QString>("query");
    QTest::addColumn<QString>("reply");
    QTest::newRow("fault") << "?FF" << "FF=4";
    QTest::newRow("current") << "?A" << "A=120:0";
    QTest::newRow("speed") << "?BS 1" << "BS=601";
    QTest::newRow("heat") << "?T 2" << "T=60";
    QTest::newRow("invalid") << "?CB 1" << "CB=garbage";
    QTest::newRow("mode") << "~MMOD 1" << "MMOD=1";
    QTest::newRow("watchdog") << "~RWD" << "RWD=0";
    QTest::newRow("mixed") << "~MXMD" << "MXMD=1:0";
    QTest::newRow("script") << "?FS" << "FS=128";
    QTest::newRow("other-channel") << "?P 2" << "P=1";
    QTest::newRow("latched-other-command") << "?M 2" << "M=100";
  }
  void continuousGuard() {
    QFETCH(QString, query);
    QFETCH(QString, reply);
    FakeController fake;
    fake.overrides["?A"] = "A=0:0";
    fake.overrides[query] = reply;
    Window w;
    ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    const auto id = start(c, "continuous", continuousArgs(c));
    QTRY_COMPARE_WITH_TIMEOUT(result(c, id)["state"].toString(), QString("failed"), 4000);
    QVERIFY(!fake.commands.contains("!G 1 100"));
    if (query == "?CB 1")
      QVERIFY(!call(c, "status")["result"].toObject()["connected"].toBool());
    else
      QVERIFY(fake.commands.contains("!EX"));
  }
  void continuousRuntimeOvercurrentStops() {
    FakeController fake;
    fake.overrides["?A"] = "A=0:0";
    Window w;
    ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    auto args = continuousArgs(c);
    args["lease_ms"] = 5000;
    const auto id = start(c, "continuous", args);
    QTRY_VERIFY(fake.commands.contains("!G 1 100"));
    fake.overrides["?A"] = "A=120:0";
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("failed"));
    QVERIFY(result(c, id)["message"].toString().contains("corrente"));
    QVERIFY(result(c, id)["result"].toObject()["stop_confirmed"].toBool());
    QVERIFY(fake.faults & 16);
  }
  void continuousNoProgressStops() {
    FakeController fake;
    fake.overrides["?A"] = "A=0:0";
    Window w;
    ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    auto args = continuousArgs(c);
    args["lease_ms"] = 5000;
    args["no_motion_ms"] = 1000;
    const auto id = start(c, "continuous", args);
    QTRY_COMPARE_WITH_TIMEOUT(result(c, id)["state"].toString(), QString("failed"), 4000);
    QVERIFY(result(c, id)["message"].toString().contains("Sem progresso"));
    QVERIFY(result(c, id)["result"].toObject()["stop_confirmed"].toBool());
  }
  void continuousSerialLossNeverRestarts() {
    FakeController fake;
    fake.overrides["?A"] = "A=0:0";
    Window w;
    ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    auto args = continuousArgs(c);
    args["lease_ms"] = 5000;
    const auto id = start(c, "continuous", args);
    QTRY_VERIFY(fake.commands.contains("!G 1 100"));
    fake.silent.insert("?A");
    QTRY_COMPARE_WITH_TIMEOUT(result(c, id)["state"].toString(), QString("failed"), 4000);
    QVERIFY(!call(c, "status")["result"].toObject()["connected"].toBool());
    const int count = fake.commands.count("!G 1 100");
    QTest::qWait(300);
    QCOMPARE(fake.commands.count("!G 1 100"), count);
    QVERIFY(!result(c, id)["result"].toObject()["stop_confirmed"].toBool());
  }
  void demoCompilerProfilesAndUpload() {
    Window w;
    ControlServer c(w);
    QTemporaryDir dir;
    call(c, "connect", {{"mode", "demo"}});
    QTRY_VERIFY(
        !call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    auto id = start(c, "read_parameters",
                    {{"queries", QJsonArray{"~ALIM 1", "~MMOD 1"}}});
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("succeeded"));
    const auto path = dir.filePath("profile.json");
    QVERIFY(
        call(c, "profile", {{"action", "export_json"}, {"path", path}})["ok"]
            .toBool());
    QVERIFY(
        !call(c, "profile", {{"action", "export_json"}, {"path", path}})["ok"]
             .toBool());
    QVERIFY(
        call(c, "profile", {{"action", "import_json"}, {"path", path}})["ok"]
            .toBool());
    id =
        start(c, "script", {{"action", "simulate"}, {"source", "print(42)\n"}});
    QVERIFY(!id.isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(result(c, id)["state"].toString(),
                              QString("succeeded"), 10000);
    auto compiled = result(c, id)["result"].toObject();
    QVERIFY(
        compiled["simulation"].toObject()["output"].toString().contains("42"));
    QFile f(dir.filePath("script.hex"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(compiled["hex"].toString().toUtf8());
    f.close();
    id = start(c, "upload_script", {{"path", f.fileName()}});
    QTRY_COMPARE_WITH_TIMEOUT(result(c, id)["state"].toString(),
                              QString("succeeded"), 10000);
    QVERIFY(!call(c, "status")["result"].toObject()["motion_enabled"].toBool());
  }
  void pollingResumesAfterJobInterruptsTelemetryScan() {
    FakeController fake;
    Window w;
    ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(fake.commands.contains("?A"));
    const int before = w.sampleCount();
    const auto id = start(c, "query", {{"command", "~ALIM 1"}});
    QTRY_COMPARE(result(c, id)["state"].toString(), QString("succeeded"));
    QTRY_VERIFY(w.sampleCount() > before);
  }
  void stopInterruptsOutstandingReplyWithoutFalseConfirmation() {
    FakeController fake;
    Window w;
    ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(
        !call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    fake.silent.insert("~ALIM 1");
    const auto id = start(c, "query", {{"command", "~ALIM 1"}});
    QTRY_VERIFY(fake.commands.contains("~ALIM 1"));
    const auto stop = call(c, "stop")["result"].toObject();
    QVERIFY(stop["submitted"].toBool());
    QVERIFY(!stop["confirmed"].toBool());
    QCOMPARE(result(c, id)["state"].toString(), QString("failed"));
    QVERIFY(!call(c, "status")["result"].toObject()["connected"].toBool());
    QTRY_VERIFY(fake.commands.contains("!EX"));
  }
  void unknownFirmwareAndQueryInjectionRejected() {
    FakeController fake;
    fake.overrides["?FID"] = "FID=Roboteq v99.0 SBLMG2XXX";
    Window w;
    ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(
        !call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    QVERIFY(!call(c, "query", {{"command", "?FF\r!G 1 30"}})["ok"].toBool());
    QVERIFY(!call(c, "query", {{"command", "!G 1 30"}})["ok"].toBool());
    auto st = call(c, "status")["result"].toObject();
    QVERIFY(!call(c, "maintenance",
                  {{"command", "%EESAV"},
                   {"confirm", true},
                   {"expected_model", st["model"]},
                   {"expected_firmware", st["firmware"]}})["ok"]
                 .toBool());
    QVERIFY(!fake.commands.contains("%EESAV"));
  }
};
QTEST_MAIN(ControlTests)
#include "control_tests.moc"
