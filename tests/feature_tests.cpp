#include "basicvm.h"
#include "cansdo.h"
#include "catalog.h"
#include "cpr.h"
#include "fake_controller.h"
#include "firmware.h"
#include "hexfile.h"
#include "robocan.h"
#include "session.h"
#include <QFile>
#include <QtEndian>
#include <QtTest>
class FeatureTests : public QObject {
  Q_OBJECT
private slots:
  void roboCanOfflineMatchesOriginalSequence() {
    auto image = HexFile::parse(
        ":100000000202020309010601052500FFFFFFFFFFB1\n:00000001FF\n");
    QVERIFY(image.valid());
    auto text = microbasicRoboCan(image);
    QVERIFY(text.contains("--> TT1   8   SS 02 14 FF CB 10 2C 13"));
    QVERIFY(text.contains("--> TT2   8   SS 0F 00 00 02 02 02 03"));
    QVERIFY(text.contains("--> TT2   8   SS 0F 0C 00 FF FF FF FF"));
    QCOMPARE(text.count("<-- SS9   X   TT 02 XX C0"), 1);
    QVERIFY(text.contains("--> TT2   4   SS F0 2C 00"));
    QVERIFY(!text.contains("SS 0F 14 00")); // no emergency release
    QVERIFY(microbasicRoboCan({}).isEmpty());
  }
  void originalCprRoundtripAndMalformed() {
    QFile f(QString(TEST_SOURCE_DIR) +
            "/tests/fixtures/original-SBLG2360T.cpr");
    QVERIFY(f.open(QIODevice::ReadOnly));
    const auto data = f.readAll();
    auto p = CprProfile::parse(data);
    QVERIFY2(p.valid(), qPrintable(p.error));
    QCOMPARE(p.items.size(), 613);
    QCOMPARE(p.firmware, QString("v3.0:SBLG2360T"));
    auto restored = CprProfile::parse(p.xml());
    QVERIFY(restored.valid());
    QCOMPARE(restored.items.size(), p.items.size());
    for (int i = 0; i < p.items.size(); ++i) {
      QCOMPARE(restored.items[i].identity(), p.items[i].identity());
      QCOMPARE(restored.items[i].value, p.items[i].value);
      QCOMPARE(restored.items[i].multiplier, p.items[i].multiplier);
      QCOMPARE(restored.items[i].node, p.items[i].node);
      QCOMPARE(restored.items[i].type, p.items[i].type);
    }
    QVERIFY(!CprProfile::parse(data.left(data.size() / 2)).valid());
    QVERIFY(!CprProfile::parse("<!DOCTYPE x [<!ENTITY a 'hi'>]>" +
                               data.mid(data.indexOf("<ControllerProfile")))
                 .valid());
    auto duplicate = p;
    duplicate.items.append(p.items[0]);
    QVERIFY(!CprProfile::parse(duplicate.xml()).valid());
    auto conflict = p;
    conflict.items.last().value = "123";
    // Locate one actual aliased command and give the second path a conflicting
    // value.
    for (int i = 0; i < conflict.items.size(); ++i)
      if (conflict.items[i].command == "KPF" &&
          conflict.items[i].node.contains("Closed Loop Torque")) {
        conflict.items[i].value = "123";
        break;
      }
    QVERIFY(!CprProfile::parse(conflict.xml()).valid());
    auto overflow = p;
    overflow.items[0].value = "4294967296";
    QVERIFY(!CprProfile::parse(overflow.xml()).valid());
    auto negativeIndex = p;
    negativeIndex.items[0].index = -1;
    QVERIFY(!CprProfile::parse(negativeIndex.xml()).valid());
  }
  void originalTreeOptionsRejectHoles() {
    QVERIFY(Protocol::command("^DINL 10 1").valid);
    QVERIFY(!Protocol::command("^DINL 1023").valid);
    QVERIFY(!Protocol::command("^DINL 11 0").valid);
    QVERIFY(Protocol::command("^EE 32 -32768").valid);
    QVERIFY(!Protocol::command("^EE 33 0").valid);
    QCOMPARE(Catalog::find("LD")->scale, 1000.0);
    QCOMPARE(Catalog::find("MXTRN")->scale, 100.0);
    QVERIFY(!Protocol::command("^BMOD 1 4").valid);
    QVERIFY(!Protocol::command("^CEN 5").valid);

    const auto spec = Catalog::find("ISM");
    QVERIFY(spec);
    QVERIFY(Protocol::command("^ISM 0").valid);
    QVERIFY(Protocol::command("^ISM 2").valid);
    QVERIFY(Protocol::command("^ISM 4").valid);
    QVERIFY(!Protocol::command("^ISM 1").valid);
    QVERIFY(!Protocol::command("^ISM 3").valid);
    auto meta = Catalog::commonOriginal("ALIM", 1);
    QCOMPARE(meta["path"].toArray()[0].toString(), QString("Motor 1"));
    QCOMPARE(meta["path"].toArray()[2].toString(), QString("Amps Limits"));
  }
  void catalogLimitsAndScales() {
    QSet<QString> keys;
    int count = 0;
    for (const auto &s : Catalog::all()) {
      QVERIFY(!keys.contains(s.key));
      keys.insert(s.key);
      QVERIFY(s.minimum <= s.maximum);
      QVERIFY(s.scale >= 1);
      for (int i = s.indexMax ? 1 : 0; i <= s.indexMax; ++i) {
        ++count;
        QVERIFY(Protocol::command("~" + Catalog::suffix(s, i)).valid);
      }
    }
    QVERIFY(count >= 600);
    QCOMPARE(Protocol::motorNames(0x81), QStringList({"AmpLim", "FETs Off"}));
    QCOMPARE(Protocol::faultNames(16), QStringList({"Parada de emergência"}));
    QVERIFY(
        !Protocol::compatible("Roboteq v3.0f SBLMG2XXX", "RoboG4:SBLG2360T"));
    QVERIFY(Protocol::command("^KPG 3 123456789").valid);
    QVERIFY(!Protocol::command("^KPG 5 1").valid);
    QVERIFY(Protocol::command("^TPDM 64 4294967295").valid);
    QVERIFY(!Protocol::command("^TPDM 64 4294967296").valid);
    QVERIFY(Protocol::command("^OVL 600").valid);
    QVERIFY(!Protocol::command("^OVL 601").valid);
    QVERIFY(Protocol::command("^OTL 2 120").valid);
    QVERIFY(!Protocol::command("^OTL 1 86").valid);
    QCOMPARE(Catalog::find("KPF")->scale, 10000.0);
    QCOMPARE(Catalog::find("KPG")->scale, 1000000.0);
    QVERIFY(!Protocol::command("^ISM 1").valid);
    QVERIFY(Protocol::command("!GIQ 1 -450").valid);
    QVERIFY(!Protocol::command("!GIQ 1 451").valid);
  }
  void officialDfuIsBoundToModel_data() {
    QTest::addColumn<QString>("model");
    QTest::newRow("SBLG") << QString("SBLG2360T");
    QTest::newRow("SBLMG") << QString("SBLMG2360T");
  }
  void officialDfuIsBoundToModel() {
    QFETCH(QString, model);
    const auto family = model == "SBLG2360T" ? "SBLG2XXX" : "SBLMG2XXX";
    QFile file(QString(TEST_SOURCE_DIR) + "/firmware/" + family +
               "-Firmware-Update-v30f-071526.dfu");
    QVERIFY(file.open(QIODevice::ReadOnly));
    auto bytes = file.readAll();
    auto valid = Firmware::parse(bytes, model);
    QVERIFY2(valid.valid(), qPrintable(valid.error));
    QCOMPARE(valid.version, QString("v3.0f"));
    QCOMPARE(valid.address, quint32(0x08010000));
    QVERIFY(valid.payload.size() > 200000);
    QVERIFY(!Firmware::parse(bytes,
                             model == "SBLG2360T" ? "SBLMG2360T" : "SBLG2360T")
                 .valid());
    bytes[1000] ^= 1;
    QVERIFY(!Firmware::parse(bytes, model).valid());
    qToLittleEndian(Firmware::crc(bytes.left(bytes.size() - 4)),
                    bytes.data() + bytes.size() - 4);
    QVERIFY(!Firmware::parse(bytes, model)
                 .valid()); // Valid CRC cannot bypass SHA/model allowlist.
    QVERIFY(!Firmware::parse(bytes.left(290), model).valid());
    QVERIFY(!Firmware::parse(file.fileName().toUtf8(), model).valid());
    QCOMPARE(Firmware::dfuArguments("1-4.2"),
             QStringList({"-d", ",0483:df11", "-p", "1-4.2", "-a", "0"}));
    QVERIFY(Firmware::dfuArguments("1-4;echo x").isEmpty());
    QVERIFY(Firmware::dfuArguments("").isEmpty());
  }
  void hexValidationRejectsCorruptionAndFirmwareAddresses() {
    auto good = HexFile::parse(":0400000001020304F2\n:00000001FF\n");
    QVERIFY2(good.valid(), qPrintable(good.error));
    QCOMPARE(good.bytes, QByteArray::fromHex("01020304"));
    for (auto data : {":0400000001020304F1\n:00000001FF", ":0400000001020304F2",
                      ":00000001FF\n:00000001FF",
                      ":020000040800F2\n:0400000001020304F2\n:00000001FF",
                      ":0400000001020304F2\n:0400000001020304F2\n:00000001FF"})
      QVERIFY(!HexFile::parse(data).valid());
  }
  void motionNeedsIdentityWritesAndPositiveWatchdog() {
    FakeController endpoint;
    Session session;
    QSignalSpy replies(&session, &Session::received);
    QSignalSpy logs(&session, &Session::traffic);
    QVERIFY(session.openSerial(endpoint.path, 115200));
    QTRY_COMPARE(replies.size(), 2);
    QVERIFY(!session.enqueue("!G 1 10"));
    QVERIFY(session.setWritesEnabled(true));
    QVERIFY(!session.setMotionEnabled(true));
    endpoint.params["RWD"] = 0;
    QVERIFY(session.enqueue("~RWD"));
    QTRY_COMPARE(replies.size(), 3);
    QVERIFY(!session.setMotionEnabled(true));
    endpoint.params["RWD"] = 1000;
    QVERIFY(session.enqueue("~RWD"));
    QTRY_COMPARE(replies.size(), 4);
    QVERIFY(session.setMotionEnabled(true));
    QVERIFY(session.enqueue("!G 1 10"));
    QTRY_COMPARE(replies.size(), 5);
    QVERIFY(session.enqueue("?A"));
    QVERIFY(session.enqueue("!G 1 99"));
    QVERIFY(session.enqueue("!EX"));
    QTRY_VERIFY(endpoint.commands.contains("!EX"));
    QVERIFY(!session.motionEnabled());
    QVERIFY(!endpoint.commands.contains("!G 1 99"));
    QVERIFY(!session.enqueue("!MG"));
    QVERIFY(!session.enterDfu());
  }
  void secretsAreRedactedIncludingTimeoutAndEcho() {
    QVERIFY(!Protocol::redacted("Erro para %UK 1234567.").contains("1234567"));
    FakeController endpoint;
    Session session;
    session.timeoutMs = 100;
    QSignalSpy replies(&session, &Session::received),
        logs(&session, &Session::traffic), failures(&session, &Session::failed);
    QVERIFY(session.openSerial(endpoint.path, 115200));
    QTRY_COMPARE(replies.size(), 2);
    QVERIFY(session.setWritesEnabled(true));
    QVERIFY(session.enqueue("%LK 1234567"));
    QTRY_COMPARE(replies.size(), 3);
    endpoint.silent.insert("%UK 7654321");
    QVERIFY(session.enqueue("%UK 7654321"));
    QTRY_COMPARE(failures.size(), 1);
    for (const auto &entry : logs) {
      QVERIFY(!entry[1].toString().contains("1234567"));
      QVERIFY(!entry[1].toString().contains("7654321"));
    }
    QVERIFY(!failures[0][0].toString().contains("7654321"));
  }
  void hexTransferUsesHldAndEveryAckWithoutExecution() {
    FakeController endpoint;
    Session session;
    QSignalSpy replies(&session, &Session::received),
        finished(&session, &Session::uploadFinished);
    auto image = HexFile::parse(":0400000001020304F2\n:00000001FF\n");
    QVERIFY(session.openSerial(endpoint.path, 115200));
    QTRY_COMPARE(replies.size(), 2);
    QVERIFY(!session.uploadScript(image));
    QVERIFY(session.setWritesEnabled(true));
    QVERIFY(!session.uploadScript(
        HexFile::parse(":0400000001020304F2\n:00000001FF\n")));
    QVERIFY(session.enqueue("!R 0"));
    QTRY_COMPARE(replies.size(), 3);
    QVERIFY(session.enqueue("!EX"));
    QTRY_COMPARE(replies.size(), 4);
    QVERIFY(session.enqueue("?FF"));
    QTRY_COMPARE(replies.size(), 5);
    QVERIFY(session.uploadScript(image));
    QTRY_COMPARE(finished.size(), 1);
    QVERIFY(finished[0][0].toBool());
    QVERIFY(endpoint.commands.contains("%SLD"));
    QVERIFY(endpoint.commands.contains(":00000001FF"));
    QVERIFY(!endpoint.commands.contains("!R 1"));
    QVERIFY(!endpoint.commands.contains("!R 2"));
    QVERIFY(!session.uploadScript(image));
  }
  void rejectedHexDoesNotContinueOrRun() {
    FakeController endpoint;
    endpoint.overrides[":0400000001020304F2"] = "-";
    Session session;
    QSignalSpy replies(&session, &Session::received),
        finished(&session, &Session::uploadFinished);
    QVERIFY(session.openSerial(endpoint.path, 115200));
    QTRY_COMPARE(replies.size(), 2);
    QVERIFY(session.setWritesEnabled(true));
    QVERIFY(!session.uploadScript(
        HexFile::parse(":0400000001020304F2\n:00000001FF\n")));
    QVERIFY(session.enqueue("!R 0"));
    QTRY_COMPARE(replies.size(), 3);
    QVERIFY(session.enqueue("!EX"));
    QTRY_COMPARE(replies.size(), 4);
    QVERIFY(session.enqueue("?FF"));
    QTRY_COMPARE(replies.size(), 5);
    QVERIFY(session.uploadScript(
        HexFile::parse(":0400000001020304F2\n:00000001FF\n")));
    QTRY_COMPARE(finished.size(), 1);
    QVERIFY(!finished[0][0].toBool());
    QVERIFY(!session.writesEnabled());
    QVERIFY(!endpoint.commands.contains(":00000001FF"));
  }
  void uploadRejectsStaleOrInvalidStopProof_data() {
    QTest::addColumn<QString>("fault");
    QTest::newRow("negative") << QString("-1");
    QTest::newRow("overflow") << QString("65536");
    QTest::newRow("not-stopped") << QString("0");
  }
  void uploadRejectsStaleOrInvalidStopProof() {
    QFETCH(QString, fault);
    FakeController endpoint;
    Session session;
    QSignalSpy replies(&session, &Session::received);
    QVERIFY(session.openSerial(endpoint.path, 115200));
    QTRY_COMPARE(replies.size(), 2);
    QVERIFY(session.setWritesEnabled(true));
    QVERIFY(session.enqueue("!R 0"));
    QTRY_COMPARE(replies.size(), 3);
    QVERIFY(session.enqueue("!EX"));
    QTRY_COMPARE(replies.size(), 4);
    QVERIFY(session.enqueue("?FF"));
    QTRY_COMPARE(replies.size(), 5);
    endpoint.overrides["?FF"] = "FF=" + fault;
    QVERIFY(session.enqueue("?FF"));
    QTRY_COMPARE(replies.size(), 6);
    QVERIFY(!session.uploadScript(
        HexFile::parse(":0400000001020304F2\n:00000001FF\n")));
    QVERIFY(!endpoint.commands.contains("%SLD"));
  }
  void streamInterleavesWithQueriesAndStops() {
    FakeController endpoint;
    Session session;
    QSignalSpy replies(&session, &Session::received),
        frames(&session, &Session::streamFrame);
    QVERIFY(session.openSerial(endpoint.path, 115200));
    QTRY_COMPARE(replies.size(), 2);
    QVERIFY(!session.startStream(1));
    QVERIFY(session.startStream(50));
    QVERIFY(session.enqueue("?A"));
    endpoint.send("ML=1:2:3:4:5:6:7:8:9:10:11:12");
    QTRY_COMPARE(frames.size(), 1);
    QTRY_COMPARE(replies.size(), 3);
    QCOMPARE(replies.last()[1].toString(), QString("125:43"));
    session.stopStream();
    QVERIFY(!session.streaming());
    QTRY_VERIFY(endpoint.commands.contains("#"));
    QVERIFY(endpoint.commands.contains("#C"));
  }
  void sdoExpeditedAndSegmentedDataAreStrict() {
    SdoTransfer s;
    auto begin = s.begin(0x1008, 0);
    QCOMPARE(quint8(begin[0]), quint8(0x40));
    auto b = QByteArray::fromHex("4309100074657374");
    s.accept(b);
    QVERIFY(s.active); // Unrelated object ignored.
    b = QByteArray::fromHex("4b08100034120000");
    s.accept(b);
    QVERIFY(!s.active);
    QCOMPARE(s.value, QByteArray::fromHex("3412"));
    s.begin(0x1008, 0);
    auto next = s.accept(QByteArray::fromHex("410810000a000000"));
    QCOMPARE(quint8(next[0]), quint8(0x60));
    next = s.accept(QByteArray::fromHex("0041424344454647"));
    QCOMPARE(quint8(next[0]), quint8(0x70));
    s.accept(QByteArray::fromHex("1948494a00000000"));
    QVERIFY(!s.active);
    QVERIFY(s.error.isEmpty());
    QCOMPARE(s.value, QByteArray("ABCDEFGHIJ"));
    s.begin(0x1008, 0);
    s.accept(QByteArray::fromHex("410810000a000000"));
    s.accept(QByteArray::fromHex("1041424344454647"));
    QVERIFY(!s.error.isEmpty());
    s.begin(0x1008, 0);
    s.accept(QByteArray::fromHex("8008100000000206"));
    QVERIFY(s.error.contains("06020000"));
    s.begin(0x1008, 0);
    s.accept(QByteArray::fromHex("4108100000200000"));
    QVERIFY(!s.active);
    QVERIFY(!s.error.isEmpty());
  }
  void simulatorLoopsConditionsArraysAndSubroutines() {
    BasicVm vm;
    QVERIFY2(
        vm.compile(
            "Option Explicit\nDim i As Integer\nDim sum As Integer\nDim a[4] "
            "As Integer\nFor i = 0 To 3\na[i] = i+1\nsum = sum+a[i]\nNext "
            "i\nIf sum = 10 Then\nGoSub result\nElse\nPrint(\"bad\")\nEnd "
            "If\nTerminate\nresult:\nPrint(sum,\"\\n\")\nReturn\n"),
        qPrintable(vm.error));
    vm.run();
    QVERIFY2(vm.error.isEmpty(), qPrintable(vm.error));
    QCOMPARE(vm.output, QString("10\n"));
    QVERIFY(vm.done());
    QCOMPARE(vm.variables["a[3]"], 4);
  }
  void simulatorValuesConfigurationAndTimeStayLocal() {
    BasicVm vm;
    QVERIFY2(vm.compile("Dim v As Integer\nv = "
                        "GetValue(_A,1)\nSetConfig(_ALIM,1,v*2)\nSetCommand(_G,"
                        "1,50)\nWait(500)\nPrint(GetConfig(_ALIM,1))\n"),
             qPrintable(vm.error));
    vm.setInput("A", 1, 125);
    vm.run();
    QVERIFY2(vm.error.isEmpty(), qPrintable(vm.error));
    QCOMPARE(vm.output, QString("250"));
    QCOMPARE(vm.milliseconds, qint64(500));
    QCOMPARE(vm.commands["G 1"], 50);
  }
  void simulatorTimersAndForBoundsUseDocumentedSemantics() {
    BasicVm vm;
    QVERIFY2(vm.compile("SetTimerCount(0,1000)\nSetTimerState(0,0)\nWait(400)"
                        "\nPrint(GetTimerCount(0),\" \",GetTimerState(0),\" "
                        "\")\nSetTimerState(0,1)\nWait(1000)\nPrint("
                        "GetTimerCount(0),\" \",GetTimerState(0))"),
             qPrintable(vm.error));
    vm.run();
    QVERIFY2(vm.error.isEmpty(), qPrintable(vm.error));
    QCOMPARE(vm.output, QString("600 0 600 1"));
    QVERIFY(vm.compile("n=3\nFor i=1 To n\nn=1\ns=s+i\nNext\nPrint(s)"));
    vm.run();
    QVERIFY(vm.error.isEmpty());
    QCOMPARE(vm.output, QString("6"));
    QVERIFY(!vm.compile("Print(\"a\" + \"b\")"));
    QVERIFY(!vm.compile("Print(\"\\q\")"));
  }
  void simulatorDistinguishesBooleanAndIntegerOperators() {
    BasicVm vm;
    QVERIFY2(vm.compile("Dim b As Boolean\nb = True\nPrint(Not b,\" \",Not "
                        "1,\" \",True And False,\" \",ToBool(7),\"\\n\")\n"),
             qPrintable(vm.error));
    vm.run();
    QVERIFY2(vm.error.isEmpty(), qPrintable(vm.error));
    QCOMPARE(vm.output, QString("False -2 False True\n"));
    QVERIFY(!vm.compile("x=_A+1"));
  }
  void simulatorRejectsUnsupportedSyntaxAndRuntimeErrors() {
    BasicVm vm;
    QVERIFY(!vm.compile("If 1 Then\nElse\nElse\nEnd If"));
    QVERIFY(!vm.compile("Unsupported(3)"));
    QVERIFY(!vm.compile("Option Explicit\nx=2"));
    QVERIFY(vm.compile("x=4/0"));
    vm.run();
    QVERIFY(vm.error.contains("zero"));
    QVERIFY(vm.compile("Dim a[2] As Integer\na[3]=1"));
    vm.run();
    QVERIFY(vm.error.contains("array"));
    QVERIFY(vm.compile("While True\nx=x+1\nEnd While"));
    vm.run(100000);
    QVERIFY(!vm.done());
    vm.step();
    QVERIFY(vm.error.contains("100.000"));
  }
};
QTEST_GUILESS_MAIN(FeatureTests)
#include "feature_tests.moc"
