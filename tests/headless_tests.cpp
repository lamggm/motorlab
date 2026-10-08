#include "control.h"
#include "fake_controller.h"
#include "headless_host.h"
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>
class HeadlessTests : public QObject {
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
  void streamDoesNotInventHallReadings() {
    Session session; HeadlessHost host(session);
    QVERIFY(host.connectDevice("demo", {}, 115200));
    // The 12-field capture has DI/DO at slots 8/9, not Hall RPM.
    session.streamFrame("58:51:810:790:508:36:8400:8200:3:1:0:8");
    const auto sample = host.telemetry(1)["samples"].toArray().last().toObject();
    QCOMPARE(sample["rpm1_encoder"].toInt(),810);
    QCOMPARE(sample["rpm2_encoder"].toInt(),790);
    QVERIFY(sample["rpm1_hall"].isNull());
    QVERIFY(sample["rpm2_hall"].isNull());
    QCOMPARE(sample["encoder1"].toInt(),8400);
    QCOMPARE(sample["faults"].toInt(),0);
    QCOMPARE(sample["flags"].toInt(),8);
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
    Session session; HeadlessHost w(session);
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
    Session session; HeadlessHost w(session);
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
    QVERIFY(!(&w.session)->motionEnabled());
  }
  void counterReferenceGuard_data() {
    QTest::addColumn<QString>("key");
    QTest::addColumn<QString>("badQuery");
    QTest::addColumn<QString>("badReply");
    QTest::addColumn<bool>("sent");
    QTest::addColumn<bool>("success");
    QTest::newRow("encoder-idle-no-script") << "C" << "" << "" << true << true;
    QTest::newRow("internal-idle-no-script") << "CB" << "" << "" << true << true;
    QTest::newRow("closed-loop-motor1") << "C" << "~MMOD 1" << "MMOD=3" << false << false;
    QTest::newRow("closed-loop-motor2") << "C" << "~MMOD 2" << "MMOD=1" << false << false;
    QTest::newRow("encoder-moving") << "C" << "?S" << "S=1:0" << false << false;
    QTest::newRow("hall-moving") << "CB" << "?BS 1" << "BS=1" << false << false;
    QTest::newRow("script-active") << "C" << "?FS" << "FS=128" << false << false;
    QTest::newRow("stop-unconfirmed") << "C" << "?FF" << "FF=0" << false << false;
    QTest::newRow("mode-unreadable") << "C" << "~MMOD 1" << "-" << false << false;
    QTest::newRow("write-rejected") << "C" << "!C 1 0" << "-" << true << false;
    QTest::newRow("readback-mismatch") << "C" << "?C 1" << "C=123" << true << false;
  }
  void counterReferenceGuard() {
    QFETCH(QString, key); QFETCH(QString, badQuery); QFETCH(QString, badReply);
    QFETCH(bool, sent); QFETCH(bool, success);
    FakeController fake;
    fake.overrides["!R 0"] = "-";
    fake.overrides["?FS"] = "FS=8";
    fake.overrides["?S"] = "S=0:0";
    fake.overrides["?BS 1"] = "BS=0";
    fake.overrides["?BS 2"] = "BS=0";
    fake.overrides["?" + key + " 1"] = key + "=0";
    if (!badQuery.isEmpty()) fake.overrides[badQuery] = badReply;
    Session session; HeadlessHost w(session); ControlServer c(w);
    QVERIFY(call(c, "connect", {{"mode", "serial"}, {"port", fake.path}})["ok"].toBool());
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    const auto st = call(c, "status")["result"].toObject();
    fake.commands.clear();
    const QString command = "!" + key + " 1 0";
    auto id = start(c, "maintenance", {{"command", command}, {"confirm", true},
                       {"expected_model", st["model"]}, {"expected_firmware", st["firmware"]}});
    QVERIFY(!id.isEmpty());
    QTRY_VERIFY(result(c, id)["state"].toString() != "running");
    QCOMPARE(result(c, id)["success"].toBool(), success);
    QCOMPARE(fake.commands.contains(command), sent);
    if (sent) {
      QVERIFY(fake.commands.indexOf("~MMOD 2") < fake.commands.indexOf(command));
      QVERIFY(fake.commands.indexOf("?BS 2") < fake.commands.indexOf(command));
    }
    QVERIFY(!fake.commands.contains("!MG"));
    QVERIFY(!fake.commands.contains("%EESAV"));
    QVERIFY(!(&w.session)->motionEnabled());
  }
  void realSessionRequiresExactIdentityAndVerifiesRam() {
    FakeController fake;
    Session session; HeadlessHost w(session);
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
    Session session; HeadlessHost w(session);
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
  void motionStopsBeforeRejectedScriptCleanup() {
    FakeController fake;
    fake.overrides["!R 0"] = "-";
    Session session; HeadlessHost w(session);
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
            {"max_speed_rpm", 600}, {"max_current_a", 10},
            {"max_temperature_c", 60}, {"max_power_percent", 25},
            {"confirm", true}, {"expected_model", st["model"]},
            {"expected_firmware", st["firmware"]}};
  }
  void encoderSpeedSupervision_data() {
    QTest::addColumn<QString>("scenario");
    for (const auto &s : {"normal", "rated-positive", "rated-negative", "rated-overcurrent", "rated-excess-power", "rated-overspeed", "rated-19a", "wrong-mode", "wrong-feedback", "excess-current-limit", "excess-power", "encoder-reverse", "encoder-overspeed", "encoder-missing", "encoder-stuck"})
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
    Session session; HeadlessHost w(session); ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    auto args = continuousArgs(c);
    args["mode"] = "encoder_speed";
    args["max_current_a"] = 3;
    if (rated) { fake.alim = 115; args["max_current_a"] = 13; }
    if (scenario == "rated-19a") { fake.alim = 190; args["max_current_a"] = 19; }
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
      if (scenario == "normal" || scenario == "rated-positive" || scenario == "rated-negative" || scenario == "rated-19a") {
        QTRY_VERIFY2(fake.commands.count(motorCommand) >= 3, qPrintable(result(c, id)["message"].toString()));
        QCOMPARE(call(c, "status")["result"].toObject()["continuous"].toObject()["unit"].toString(), QString("encoder_speed_rpm"));
        QVERIFY(call(c, "stop")["ok"].toBool());
        QTRY_VERIFY(result(c, id)["state"].toString() != "running");
        QVERIFY2(result(c, id)["state"].toString() == "succeeded",
                 qPrintable(result(c, id)["message"].toString()));
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
    Session session; HeadlessHost w(session); ControlServer c(w);
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
  void positionSupervision_data() {
    QTest::addColumn<QString>("scenario");
    for (const auto name : {"hold-zero", "hold-target", "travel-window", "lease", "loop-error", "moving", "far-origin", "velocity-zero", "speed-limit", "guard-below-nominal", "current-limit", "measured-current-limit", "hold-fast-trajectory"})
      QTest::newRow(name) << QString(name);
  }
  void positionSupervision() {
    QFETCH(QString, scenario);
    FakeController fake;
    fake.alim = 115;
    fake.params["MMOD 1"] = 3; fake.params["EMOD 1"] = 18;
    fake.params["EPPR 1"] = 1000; fake.params["MXRPM 1"] = 3000;
    fake.params["MVEL 1"] = 30;
    fake.params["MXPF 1"] = fake.params["MXPR 1"] = 25;
    fake.overrides["?A"] = "A=1:0";
    fake.overrides["?S"] = "S=0:0";
    fake.overrides["?C"] = "C=0:0";
    fake.overrides["?F 1"] = "F=0";
    fake.overrides["?RMP 1"] = "RMP=0";
    fake.overrides["?TR 1"] = "TR=0";
    fake.overrides["?VDV 1"] = "VDV=0";
    fake.overrides["?E 1"] = "E=0";
    fake.overrides["?MA 1"] = "MA=0"; fake.overrides["?MA 2"] = "MA=1";
    if (scenario == "moving") fake.overrides["?S"] = "S=1:0";
    if (scenario == "far-origin") fake.overrides["?C"] = "C=5000:0";
    if (scenario == "velocity-zero") fake.params["MVEL 1"] = 0;
    if (scenario == "speed-limit") fake.params["MVEL 1"] = 3001;
    if (scenario == "hold-fast-trajectory") fake.params["MVEL 1"] = 200;
    if (scenario == "current-limit") fake.alim = 116;
    Session session; HeadlessHost w(session); ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    auto args = continuousArgs(c);
    args["mode"] = "encoder_position"; args["value"] = scenario == "hold-target" ? 100 : 0;
    args["max_current_a"] = 11.5; args["max_speed_rpm"] = 3000;
    args["lease_ms"] = 5000; args["no_motion_ms"] = 1000;
    if (scenario == "hold-target") {
      for (const auto &key : {"max_speed_rpm", "max_current_a", "max_temperature_c", "max_power_percent"})
        args.remove(key);
    }
    if (scenario == "lease") args["lease_ms"] = 1000;
    if (scenario == "speed-limit") args["max_speed_rpm"] = 3400;
    if (scenario == "guard-below-nominal") args["max_speed_rpm"] = 2999;
    const auto id = start(c, "continuous", args);
    QVERIFY(!id.isEmpty());
    if (scenario == "moving" || scenario == "far-origin" || scenario == "velocity-zero" || scenario == "speed-limit" ||
        scenario == "guard-below-nominal" || scenario == "current-limit") {
      QTRY_COMPARE(result(c, id)["state"].toString(), QString("failed"));
      QVERIFY(!fake.commands.contains("!MG"));
      QVERIFY(!fake.commands.contains("!P 1 0"));
      return;
    }
    if (scenario == "lease") {
      QTRY_COMPARE_WITH_TIMEOUT(result(c, id)["state"].toString(), QString("failed"), 3000);
      QVERIFY(result(c, id)["message"].toString().contains("Supervisão expirou"));
      QVERIFY(result(c, id)["result"].toObject()["stop_confirmed"].toBool());
      return;
    }
    QTRY_VERIFY(fake.commands.count(scenario == "hold-fast-trajectory" ? "!S 1 200" : "!S 1 30") >= 2);
    QVERIFY(fake.commands.count("!G 1 0") >= 2);
    QVERIFY(fake.commands.indexOf("!G 1 0") < fake.commands.indexOf(scenario == "hold-fast-trajectory" ? "!S 1 200" : "!S 1 30"));
    QTRY_VERIFY(fake.commands.count(scenario == "hold-target" ? "!P 1 100" : "!P 1 0") >= 2);
    for (const auto &command : fake.commands) QVERIFY(!command.startsWith("!PR "));
    if (scenario == "hold-target") fake.overrides["?C"] = "C=100:0";
    if (scenario.startsWith("hold")) {
      QTest::qWait(1200); // Rest at target must not trip the no-motion guard.
      QCOMPARE(result(c, id)["state"].toString(), QString("running"));
      const auto samples = result(c, id)["samples"].toArray();
      QVERIFY(!samples.isEmpty());
      QVERIFY(samples.last().toObject().contains("target_error_counts"));
      QVERIFY(!samples.last().toObject().contains("speed_error_rpm"));
      QVERIFY(call(c, "stop")["ok"].toBool());
      QTRY_COMPARE(result(c, id)["state"].toString(), QString("succeeded"));
    } else {
      if (scenario == "travel-window") fake.overrides["?C"] = "C=101:0";
      if (scenario == "loop-error") fake.overrides["?FM 1"] = "FM=4";
      if (scenario == "measured-current-limit") fake.overrides["?A"] = "A=116:0";
      QTRY_COMPARE_WITH_TIMEOUT(result(c, id)["state"].toString(), QString("failed"), 3000);
    }
    QVERIFY(result(c, id)["result"].toObject()["stop_confirmed"].toBool());
    QVERIFY(!fake.commands.contains("%EESAV"));
  }
  void continuousRejectsInvalidRanges() {
    FakeController fake; Session session; HeadlessHost w(session); ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    auto args = continuousArgs(c);
    args["value"] = 1001;
    QVERIFY(!call(c, "continuous", args)["ok"].toBool());
    args["value"] = 100; args["max_speed_rpm"] = 3601;
    QVERIFY(!call(c, "continuous", args)["ok"].toBool());
    args["max_speed_rpm"] = 600; args["max_current_a"] = 19.1;
    QVERIFY(!call(c, "continuous", args)["ok"].toBool());
    args["max_speed_rpm"] = 3400; args["max_current_a"] = 11.5;
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
    Session session; HeadlessHost w(session);
    ControlServer c(w);
    call(c, "connect", {{"mode", "serial"}, {"port", fake.path}});
    QTRY_VERIFY(!call(c, "status")["result"].toObject()["model"].toString().isEmpty());
    auto args = continuousArgs(c);
    args["value"] = 1000; args["max_speed_rpm"] = 3000; args["max_current_a"] = 11.5;
    const auto started = call(c, "continuous", args);
    QVERIFY(started["ok"].toBool());
    const auto id = started["result"].toObject()["job_id"].toString();
    QTRY_COMPARE_WITH_TIMEOUT(result(c, id)["state"].toString(), QString("failed"), 4000);
    QVERIFY(result(c, id)["result"].toObject()["stop_confirmed"].toBool());
    QVERIFY(result(c, id)["message"].toString().contains("expirou"));
    QVERIFY(fake.commands.count("!G 1 1000") > 1);
    QVERIFY(fake.faults & 16);
    const auto count = fake.commands.count("!G 1 1000");
    QTest::qWait(300);
    QCOMPARE(fake.commands.count("!G 1 1000"), count);
    QVERIFY(!call(c, "continuous", {{"action", "renew"},
        {"lease_token", started["result"].toObject()["lease_token"]}})["ok"].toBool());
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
    Session session; HeadlessHost w(session);
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
    Session session; HeadlessHost w(session);
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
    Session session; HeadlessHost w(session);
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
    Session session; HeadlessHost w(session);
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
  void stopInterruptsOutstandingReplyWithoutFalseConfirmation() {
    FakeController fake;
    Session session; HeadlessHost w(session);
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
    Session session; HeadlessHost w(session);
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
  }};
QTEST_GUILESS_MAIN(HeadlessTests)
#include "headless_tests.moc"
