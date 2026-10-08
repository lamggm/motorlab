#include "protocol.h"
#include "session.h"
#include <QSocketNotifier>
#include <QtTest>
#include <fcntl.h>
#include <pty.h>
#include <unistd.h>

#include "fake_controller.h"

class ProtocolTests : public QObject {
  Q_OBJECT
private slots:
  void validatesReadAndWriteBoundaries() {
    for (const QString &q : Protocol::telemetryCommands())
      QVERIFY(Protocol::command(q).valid);
    QVERIFY(Protocol::command("~ALIM 1").valid);
    QVERIFY(Protocol::command("!G 1 1000").valid);
    QVERIFY(Protocol::command("%EESAV").valid);
    QVERIFY(Protocol::command("^RWD 0").valid);
    QVERIFY(Protocol::command("^ALIM 2 450").valid);
    QVERIFY(Protocol::command("^MXRPM 1 65535").valid);
    for (const QString &q :
         {"^ALIM 1 451", "^ALIM 1 9", "^ALIM 3 200", "^MXRPM 1 65536",
          "^MXRPM 1 9", "?V 0", "?V 4", "?FID 1", "?A 1 2", "?A\r!G 1 1000",
          "?A\n", "# C", "~ALIM 999999999999999999999"})
      QVERIFY2(!Protocol::command(q).valid, qPrintable(q));
  }
  void parsesReplies() {
    auto r = Protocol::reply("A = 125:43");
    QVERIFY(r.numeric);
    QCOMPARE(r.numbers, QVector<qint64>({125, 43}));
    QCOMPARE(Protocol::reply("S=-210:40").numbers[0], qint64(-210));
    QVERIFY(!Protocol::reply("A=hello").numeric);
    QVERIFY(!Protocol::reply("A=1:").numeric);
    QVERIFY(!Protocol::reply("A=99999999999999999999999").numeric);
    QCOMPARE(Protocol::reply("+").kind, Protocol::Reply::Ack);
    QCOMPARE(Protocol::reply("?").kind, Protocol::Reply::Error);
    QCOMPARE(Protocol::reply("?FID").kind, Protocol::Reply::Other);
    QCOMPARE(Protocol::reply("FID=Roboteq v3.0f").payload,
             QString("Roboteq v3.0f"));
  }
  void requiresExactModelAndFirmwareForWrites() {
    QVERIFY(
        Protocol::compatible("Roboteq v3.0f SBLMG2XXX", "RoboG4:SBLMG2360T"));
    QVERIFY(Protocol::compatible("Roboteq v3.0f SBLG2XXX", "RoboG4:SBLG2360T"));
    QVERIFY(!Protocol::compatible("Roboteq v3.0f", "RoboG4:SBLMG2360TS"));
    QVERIFY(!Protocol::compatible("Roboteq v3.0f", "RoboG4:SBLG2360TS"));
    QVERIFY(!Protocol::compatible("Roboteq v3.0e", "RoboG4:SBLMG2360T"));
    QVERIFY(!Protocol::compatible("Roboteq v3.0e", "RoboG4:SBLG2360T"));
    QVERIFY(!Protocol::compatible("Roboteq v3.0f.beta", "RoboG4:SBLMG2360T"));
    QVERIFY(!Protocol::compatible("Roboteq v3.0f.beta", "RoboG4:SBLG2360T"));
    QVERIFY(!Protocol::compatible("Roboteq v3.0f", "RoboG4:SBLG2360T-extra"));
    QVERIFY(!Protocol::compatible("Roboteq v3.0f", "RoboG4:UNKNOWN"));
    QVERIFY(!Protocol::compatible("", "RoboG4:SBLG2360T"));
    QVERIFY(!Protocol::compatible("Roboteq v3.0f", ""));
  }
  void nativeSerialHandlesEchoFragmentsAndReadback_data() {
    QTest::addColumn<QString>("model");
    QTest::addColumn<QString>("family");
    QTest::newRow("SBLMG2360T")
        << QString("SBLMG2360T") << QString("SBLMG2XXX");
    QTest::newRow("SBLG2360T") << QString("SBLG2360T") << QString("SBLG2XXX");
  }
  void nativeSerialHandlesEchoFragmentsAndReadback() {
    QFETCH(QString, model);
    QFETCH(QString, family);
    FakeController device;
    Session session;
    device.overrides["?TRN"] = "TRN=RoboG4:" + model;
    device.overrides["?FID"] = "FID=Roboteq v3.0f " + family;
    QSignalSpy replies(&session, &Session::received);
    QVERIFY(session.openSerial(device.path, 115200));
    QVERIFY(!session.setWritesEnabled(true));
    QVERIFY(!session.enqueue("^ALIM 1 100"));
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 2, 1500);
    QCOMPARE(session.model(), "RoboG4:" + model);
    QVERIFY(session.enqueue("?A"));
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 3, 1000);
    QCOMPARE(replies[2][1].toString(), QString("125:43"));
    QVERIFY(session.setWritesEnabled(true));
    QVERIFY(session.enqueue("^ALIM 1 150"));
    QVERIFY(session.enqueue("~ALIM 1"));
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 5, 1500);
    QCOMPARE(replies[3][1].toString(), QString("+"));
    QCOMPARE(replies[4][1].toString(), QString("150"));
    QVERIFY(!device.commands.contains("^ALIM 1 100"));
    session.close();
    QVERIFY(!session.writesEnabled());
  }
  void normalMotorStopPreservesOtherChannelAndAuthorization() {
    FakeController device; Session session;
    QVERIFY(session.openSerial(device.path, 115200));
    QTRY_VERIFY(!session.model().isEmpty());
    QVERIFY(session.enqueue("~RWD"));
    QTRY_COMPARE(session.watchdogMs(), 1000);
    QVERIFY(session.setWritesEnabled(true));
    QVERIFY(session.setMotionEnabled(true));
    device.silent.insert("?V 2");
    QVERIFY(session.enqueue("?V 2"));
    QTRY_VERIFY(device.commands.contains("?V 2"));
    QVERIFY(session.enqueue("!P 1 1000"));
    QVERIFY(session.enqueue("!S 2 100"));
    QVERIFY(session.enqueue("!MS 1"));
    QVERIFY(session.motionEnabled());
    device.send("V=512");
    QTRY_VERIFY(device.commands.contains("!S 2 100"));
    QVERIFY(device.commands.contains("!MS 1"));
    QVERIFY(!device.commands.contains("!P 1 1000"));
    QVERIFY(!device.commands.contains("!EX"));
    QVERIFY(session.motionEnabled());
    QVERIFY(session.writesEnabled());
    QVERIFY(session.enqueue("!P 1 0"));
    QTRY_VERIFY(device.commands.contains("!P 1 0"));
  }
  void otherFirmwareRemainsReadable_data() {
    QTest::addColumn<QString>("model");
    QTest::newRow("SBLMG2360T") << QString("SBLMG2360T");
    QTest::newRow("SBLG2360T") << QString("SBLG2360T");
  }
  void otherFirmwareRemainsReadable() {
    QFETCH(QString, model);
    FakeController device;
    device.overrides["?TRN"] = "TRN=RoboG4:" + model;
    device.overrides["?FID"] = "FID=Roboteq v3.0e";
    Session session;
    QSignalSpy replies(&session, &Session::received);
    QVERIFY(session.openSerial(device.path, 115200));
    QTRY_COMPARE(replies.size(), 2);
    QCOMPARE(session.model(), "RoboG4:" + model);
    QCOMPARE(session.firmware(), QString("Roboteq v3.0e"));
    QVERIFY(!session.setWritesEnabled(true));
    QVERIFY(!session.enqueue("^ALIM 1 100"));
    QVERIFY(session.enqueue("?A"));
    QVERIFY(session.enqueue("~ALIM 1"));
    QTRY_COMPARE(replies.size(), 4);
    QCOMPARE(replies[2][1].toString(), QString("125:43"));
    QCOMPARE(replies[3][1].toString(), QString("200"));
    QVERIFY(session.connected());
    for (const auto &cmd : device.commands)
      QVERIFY(!cmd.startsWith('^'));
  }
  void unsupportedQueryDoesNotFabricateValues() {
    FakeController device;
    device.overrides["?S"] = "?";
    Session session;
    QSignalSpy replies(&session, &Session::received);
    QVERIFY(session.openSerial(device.path, 115200));
    QTRY_COMPARE(replies.size(), 2);
    QVERIFY(session.enqueue("?S"));
    QVERIFY(session.enqueue("?V 2"));
    QTRY_COMPARE(replies.size(), 4);
    QVERIFY(!replies[2][2].toBool());
    QCOMPARE(replies[3][1].toString(), QString("512"));
  }
  void timeoutClosesAndClearsAuthorization() {
    FakeController device;
    Session session;
    session.timeoutMs = 150;
    QSignalSpy replies(&session, &Session::received),
        failures(&session, &Session::failed);
    QVERIFY(session.openSerial(device.path, 115200));
    QTRY_COMPARE(replies.size(), 2);
    QVERIFY(session.setWritesEnabled(true));
    device.silent.insert("?V 2");
    QVERIFY(session.enqueue("?V 2"));
    QVERIFY(session.enqueue("^ALIM 1 150"));
    QTRY_COMPARE_WITH_TIMEOUT(failures.size(), 1, 1000);
    QVERIFY(!session.connected());
    QVERIFY(!session.writesEnabled());
    QVERIFY(!device.commands.contains("^ALIM 1 150"));
    const QByteArray late("V=512\r");
    ::write(device.master, late.constData(), late.size());
    QTest::qWait(50);
    QCOMPARE(replies.size(), 2);
  }
  void malformedValueClosesSession() {
    FakeController device;
    device.overrides["?A"] = "A=bad:43";
    Session session;
    QSignalSpy replies(&session, &Session::received),
        failures(&session, &Session::failed);
    QVERIFY(session.openSerial(device.path, 115200));
    QTRY_COMPARE(replies.size(), 2);
    QVERIFY(session.enqueue("?A"));
    QTRY_COMPARE(failures.size(), 1);
    QVERIFY(!session.connected());
    QCOMPARE(replies.size(), 2);
  }
  void refusesWrongModelWrites() {
    FakeController device;
    device.overrides["?TRN"] = "TRN=RoboG4:SBLMG2360TS";
    Session session;
    QSignalSpy replies(&session, &Session::received);
    QVERIFY(session.openSerial(device.path, 115200));
    QTRY_COMPARE(replies.size(), 2);
    QVERIFY(!session.setWritesEnabled(true));
    QVERIFY(!session.enqueue("^ALIM 1 100"));
  }
  void changedIdentityDisarmsQueuedWrites_data() {
    QTest::addColumn<QString>("query");
    QTest::addColumn<QString>("reply");
    QTest::newRow("unsupported-firmware")
        << QString("?FID") << QString("FID=Roboteq v3.0e SBLMG2XXX");
    QTest::newRow("other-supported-model")
        << QString("?TRN") << QString("TRN=RoboG4:SBLG2360T");
  }
  void changedIdentityDisarmsQueuedWrites() {
    QFETCH(QString, query);
    QFETCH(QString, reply);
    FakeController device;
    Session session;
    QSignalSpy replies(&session, &Session::received);
    QVERIFY(session.openSerial(device.path, 115200));
    QTRY_COMPARE(replies.size(), 2);
    QVERIFY(session.setWritesEnabled(true));
    device.overrides[query] = reply;
    QVERIFY(session.enqueue(query));
    QVERIFY(session.enqueue("^ALIM 1 150"));
    QTRY_COMPARE(replies.size(), 3);
    QTest::qWait(30);
    QVERIFY(!session.writesEnabled());
    QVERIFY(!device.commands.contains("^ALIM 1 150"));
  }
  void duplicateAckDoesNotCompleteNextWrite() {
    FakeController device;
    device.fragment = false;
    device.overrides["^ALIM 1 150"] = "+\r+";
    device.silent.insert("^ALIM 2 180");
    Session session;
    session.timeoutMs = 150;
    QSignalSpy replies(&session, &Session::received),
        failures(&session, &Session::failed);
    QVERIFY(session.openSerial(device.path, 115200));
    QTRY_COMPARE(replies.size(), 2);
    QVERIFY(session.setWritesEnabled(true));
    QVERIFY(session.enqueue("^ALIM 1 150"));
    QVERIFY(session.enqueue("^ALIM 2 180"));
    QTRY_COMPARE(failures.size(), 1);
    QCOMPARE(replies.size(), 3);
  }
  void rejectsSecondOpenAndRestoresPort() {
    FakeController device;
    Session first, second;
    QSignalSpy replies(&first, &Session::received);
    QVERIFY(first.openSerial(device.path, 115200));
    QTRY_COMPARE(replies.size(), 2);
    QVERIFY(!second.openSerial(device.path, 115200));
    QVERIFY(first.enqueue("?A"));
    QTRY_COMPARE(replies.size(), 3);
    first.close();
    QVERIFY(second.openSerial(device.path, 115200));
  }
  void demoAndRealSessionCannotMix() {
    Session session;
    QSignalSpy replies(&session, &Session::received);
    session.openDemo();
    session.close();
    QTest::qWait(60);
    QCOMPARE(replies.size(), 0);
    FakeController device;
    QVERIFY(session.openSerial(device.path, 115200));
    QTRY_COMPARE(replies.size(), 2);
    QVERIFY(!session.firmware().contains("SIMULADO"));
  }
};
QTEST_GUILESS_MAIN(ProtocolTests)
#include "protocol_tests.moc"
