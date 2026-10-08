#include "cpr.h"
#include "motor_profiles.h"
#include <QDialog>
#include "fake_controller.h"
#include "window.h"
#include "config_browser.h"
#include "config_text.h"
#include <QAbstractButton>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QScrollBar>
#include <QSlider>
#include <QWheelEvent>
#include <QTemporaryDir>
#include <QtTest>

static QTreeWidgetItem *parameter(QTreeWidget *tree, const QString &suffix) {
  QTreeWidgetItemIterator it(tree);
  while (*it) {
    if ((*it)->data(0, Qt::UserRole).toString() == suffix)
      return *it;
    ++it;
  }
  return nullptr;
}
class UiTests : public QObject {
  Q_OBJECT
private slots:
  void motorProfileDialogAndLocalStaging() {
    Window window;window.show();QTemporaryDir dir;
    auto profiles=MotorProfiles::library(dir.path());QString error;
    QVERIFY(window.stageMotorProfile(profiles[2].toObject(),2,false,&error));
    QCOMPARE(window.findChild<QDoubleSpinBox *>("draft_ALIM_2")->value(),11.5);
    QCOMPARE(window.findChild<QDoubleSpinBox *>("draft_EMOD_2")->value(),34.0);
    QCOMPARE(window.findChild<QComboBox *>("choice_MMOD_2")->currentData().toInt(),3);
    auto before=window.findChild<QDoubleSpinBox *>("draft_ALIM_1")->value();
    auto broken=profiles[0].toObject();broken["scope"]="catalog_configuration_profile";
    QVERIFY(!window.stageMotorProfile(broken,1,false,&error));
    QCOMPARE(window.findChild<QDoubleSpinBox *>("draft_ALIM_1")->value(),before);
    window.showMotorProfiles();auto dialog=window.findChild<QDialog *>("motorProfilesDialog");QVERIFY(dialog);
    auto select=dialog->findChild<QComboBox *>("motorProfileSelection");QVERIFY(select->count()>=3);
    QVERIFY(select->itemText(0).contains("125-J"));QVERIFY(select->itemText(1).contains("98-J"));
    auto calibration=dialog->findChild<QCheckBox *>("motorProfileCalibration");QVERIFY(!calibration->isEnabled());
    auto table=dialog->findChild<QTableWidget *>("motorProfilePreview");QCOMPARE(table->rowCount(),72);
    select->setCurrentIndex(2);QVERIFY(calibration->isEnabled());calibration->setChecked(true);QCOMPARE(table->rowCount(),90);
    auto channel=dialog->findChild<QComboBox *>("motorProfileChannel");channel->setCurrentIndex(1);
    select->setCurrentIndex(0);QVERIFY(!calibration->isChecked());QCOMPARE(table->rowCount(),72);
    dialog->findChild<QPushButton *>("motorProfilePrepare")->click();
    QCOMPARE(window.findChild<QDoubleSpinBox *>("draft_ALIM_2")->value(),19.0);
    QCOMPARE(window.findChild<QComboBox *>("choice_MMOD_2")->currentData().toInt(),1);
    QCOMPARE(window.findChild<QDoubleSpinBox *>("draft_ALIM_1")->value(),before);
    if(qEnvironmentVariableIsSet("MOTORLAB_PROFILE_SCREENSHOT")) {
      window.showMotorProfiles();QTest::qWait(150);
      QVERIFY(window.findChild<QDialog *>("motorProfilesDialog"));
      QVERIFY(window.findChild<QDialog *>("motorProfilesDialog")->grab().save(qEnvironmentVariable("MOTORLAB_PROFILE_SCREENSHOT")));
    }
  }

  void motorLoopErrorIsVisibleWithNoGeneralFault() {
    FakeController endpoint;
    endpoint.overrides["?FM 1"] = "FM=4";
    endpoint.overrides["?FM 2"] = "FM=128";
    Window window; window.show();
    window.findChild<QComboBox *>("sourceSelector")->setCurrentIndex(1);
    window.findChild<QComboBox *>("portSelector")->setEditText(endpoint.path);
    window.findChild<QPushButton *>("connectButton")->click();
    auto summary = window.findChild<QLabel *>("faultSummary");
    QTRY_VERIFY(summary->text().contains("Motor 1: LoopErr: erro de acompanhamento"));
    QVERIFY(!summary->text().contains("Motor 2:"));
    QVERIFY(!summary->styleSheet().isEmpty());
    auto session = window.findChild<Session *>();
    QVERIFY(session->enqueue("~RWD"));
    QTRY_COMPARE(session->watchdogMs(), 1000);
    QVERIFY(session->setWritesEnabled(true));
    QVERIFY(session->setMotionEnabled(true));
    QTRY_VERIFY(window.findChild<QLabel *>("manualControlHint")->text().contains("LoopErr"));
    QVERIFY(!window.findChild<QLabel *>("manualControlHint")->text().contains("PRONTO"));
    endpoint.overrides["?FM 1"] = "FM=0";
    QTRY_VERIFY(!summary->text().contains("LoopErr"));
    QVERIFY(summary->styleSheet().isEmpty());
    endpoint.overrides["?FM 2"] = "FM=4";
    QTRY_VERIFY(summary->text().contains("Motor 2: LoopErr"));
    endpoint.overrides["?FM 2"] = "FM=0";
    endpoint.overrides["?FM 1"] = "FM=1";
    QTRY_VERIFY(summary->text().contains("limitação de corrente ativa"));
    QTRY_VERIFY(window.findChild<QLabel *>("manualControlHint")->text().contains("PRONTO"));
    QVERIFY(!summary->styleSheet().contains("#48232b"));
    for (const auto &cmd : endpoint.commands)
      QVERIFY(!cmd.startsWith('!') && !cmd.startsWith('^') && !cmd.startsWith('%'));
  }

  void wheelScrollNeverEditsConfigurationOrMotorCommand() {
    FakeController endpoint;
    endpoint.overrides["~ALIM 1"] = "ALIM=115";
    endpoint.params["MMOD 1"] = 1;
    Window window;
    window.show();
    window.findChild<QComboBox *>("sourceSelector")->setCurrentIndex(1);
    window.findChild<QComboBox *>("portSelector")->setEditText(endpoint.path);
    window.findChild<QPushButton *>("connectButton")->click();
    auto session = window.findChild<Session *>();
    QTRY_VERIFY(!session->model().isEmpty());
    QVERIFY(session->enqueue("~ALIM 1"));
    QVERIFY(session->enqueue("~MMOD 1"));
    auto numeric = window.findChild<QDoubleSpinBox *>("draft_ALIM_1");
    auto choice = window.findChild<QComboBox *>("choice_MMOD_1");
    auto tree = window.findChild<QTreeWidget *>("parametersTree");
    QTRY_COMPARE(numeric->value(), 11.5);
    window.findChild<QCheckBox *>("enableWrites")->setChecked(true);
    QTRY_VERIFY(numeric->isEnabled());
    QTRY_VERIFY(choice->isEnabled());
    tree->expandAll();
    window.findChild<QLineEdit *>("parameterFilter")->setText("Motor");
    const auto wheel = [](QWidget *widget) {
      const QPoint center = widget->rect().center();
      QWheelEvent event(center, widget->mapToGlobal(center), QPoint(),
                        QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                        Qt::NoScrollPhase, false);
      QApplication::sendEvent(widget, &event);
    };
    for (bool focused : {false, true}) {
      tree->scrollToItem(parameter(tree, "ALIM 1"), QAbstractItemView::PositionAtTop);
      if (focused) numeric->setFocus(); else tree->setFocus();
      wheel(numeric);
      QCOMPARE(numeric->value(), 11.5);
      QVERIFY(!parameter(tree, "ALIM 1")->text(0).endsWith(" *"));
      tree->scrollToItem(parameter(tree, "MMOD 1"), QAbstractItemView::PositionAtTop);
      if (focused) choice->setFocus(); else tree->setFocus();
      wheel(choice);
      QCOMPARE(choice->currentData().toInt(), 1);
      QVERIFY(!parameter(tree, "MMOD 1")->text(0).endsWith(" *"));
    }
    // An ignored wheel event still scrolls the surrounding parameter tree.
    tree->scrollToItem(parameter(tree, "ALIM 1"), QAbstractItemView::PositionAtTop);
    const int beforeScroll = tree->verticalScrollBar()->value();
    wheel(numeric);
    QVERIFY(tree->verticalScrollBar()->value() > beforeScroll);
    // Deliberate keyboard editing after selecting the field remains available.
    numeric->setFocus();
    QTest::keyClick(numeric, Qt::Key_Up);
    QCOMPARE(numeric->value(), 11.6);
    choice->setFocus();
    QTest::keyClick(choice, Qt::Key_Down);
    QVERIFY(choice->currentData().toInt() != 1);
    // Motor sliders receive the same protection against accidental wheel input.
    auto slider = window.findChild<QSlider *>("motorCommand1");
    slider->setEnabled(true);
    wheel(slider);
    QCOMPARE(slider->value(), 0);
    for (const auto &cmd : endpoint.commands)
      QVERIFY(!cmd.startsWith('^') && !cmd.startsWith('!') && !cmd.startsWith('%'));
  }

  void documentedUnitsFollowCaptureModeWithoutChangingRawValues() {
    FakeController endpoint;
    endpoint.params["TID 1"] = 25;
    endpoint.params["ATGD 1"] = 500;
    endpoint.params["ACTR 1"] = 2500;
    endpoint.params["PMOD 1"] = 1;
    endpoint.params["MMOD 1"] = 1;
    Window window;
    window.show();
    window.findChild<QComboBox *>("sourceSelector")->setCurrentIndex(1);
    window.findChild<QComboBox *>("portSelector")->setEditText(endpoint.path);
    window.findChild<QPushButton *>("connectButton")->click();
    auto session = window.findChild<Session *>();
    QTRY_VERIFY(!session->model().isEmpty());
    auto power = window.findChild<QTreeWidget *>("parametersTree");
    auto io = window.findChild<QTreeWidget *>("ioCategories");
    const auto row = [&](const QString &key) {
      auto item = parameter(power, key);
      return item ? item : parameter(io, key);
    };
    QCOMPARE(row("ATGD 1")->text(3), QString("ms"));
    QCOMPARE(row("CGT 1")->text(3), QString("ms"));
    QCOMPARE(row("CGT 2")->text(3), QString("fator"));
    QCOMPARE(row("EPPR 1")->text(3), QString("pulsos/volta"));
    QCOMPARE(row("B25 1")->text(3), QString("K"));
    QCOMPARE(row("R25 1")->text(3), QString("Ω"));
    QCOMPARE(row("HSAT 1")->text(3), QString("1/512 volta"));
    QCOMPARE(row("PCTR 1")->text(3), QString("conforme PMOD"));
    for (const auto &cmd : {"~TID 1", "~ATGD 1", "~ACTR 1", "~PMOD 1", "~MMOD 1"})
      QVERIFY(session->enqueue(cmd));
    QTRY_COMPARE(row("PCTR 1")->text(3), QString("µs"));
    QTRY_COMPARE(row("FEW 1")->text(3), QString("RPM"));
    QCOMPARE(row("TID 1")->text(1), QString("2.5"));
    QCOMPARE(row("TID 1")->text(3), QString("A"));
    QCOMPARE(row("ATGD 1")->text(1), QString("500"));
    QCOMPARE(row("ACTR 1")->text(1), QString("2500"));
    QCOMPARE(row("ACTR 1")->text(3), QString("mV"));
    for (const auto &entry : QList<QPair<int, QString>>{{2, "Hz"}, {3, "1/4095 ciclo"}, {6, "pulsos"}, {0, "conforme PMOD"}}) {
      endpoint.params["PMOD 1"] = entry.first;
      QVERIFY(session->enqueue("~PMOD 1"));
      QTRY_COMPARE(row("PCTR 1")->text(3), entry.second);
      QCOMPARE(row("PMAX 1")->text(3), entry.second);
    }
    window.findChild<QCheckBox *>("enableWrites")->setChecked(true);
    auto mode = window.findChild<QComboBox *>("choice_MMOD_1");
    mode->setCurrentIndex(mode->findData(3));
    QCOMPARE(row("FEW 1")->text(3), QString("conforme MMOD"));
    mode->setCurrentIndex(mode->findData(1));
    QCOMPARE(row("FEW 1")->text(3), QString("RPM"));
    for (const auto &cmd : endpoint.commands)
      QVERIFY(!cmd.startsWith('^') && !cmd.startsWith('!') && !cmd.startsWith('%'));
  }

  void stoApplyUsesGlobalRegisterAndVerifiedStop_data() {
    QTest::addColumn<QString>("pauseReply");
    QTest::addColumn<QString>("stopReply");
    QTest::addColumn<QString>("faultReply");
    QTest::addColumn<QString>("statusReply");
    QTest::addColumn<bool>("success");
    QTest::newRow("no-script") << "-" << "+" << "FF=16" << "FS=8" << true;
    QTest::newRow("script-paused") << "+" << "+" << "FF=16" << "FS=8" << true;
    QTest::newRow("stop-rejected") << "-" << "-" << "FF=16" << "FS=8" << false;
    QTest::newRow("stop-unconfirmed") << "-" << "+" << "FF=0" << "FS=8" << false;
    QTest::newRow("fault-query-rejected") << "-" << "+" << "-" << "FS=8" << false;
    QTest::newRow("script-running") << "-" << "+" << "FF=16" << "FS=136" << false;
    QTest::newRow("setup-running") << "-" << "+" << "FF=16" << "FS=264" << false;
    QTest::newRow("status-rejected") << "-" << "+" << "FF=16" << "-" << false;
    QTest::newRow("status-invalid") << "-" << "+" << "FF=16" << "FS=invalid" << false;
  }
  void stoApplyUsesGlobalRegisterAndVerifiedStop() {
    QFETCH(QString, pauseReply);
    QFETCH(QString, stopReply);
    QFETCH(QString, faultReply);
    QFETCH(QString, statusReply);
    QFETCH(bool, success);
    FakeController endpoint;
    Window window;
    window.show();
    window.findChild<QComboBox *>("sourceSelector")->setCurrentIndex(1);
    window.findChild<QComboBox *>("portSelector")->setEditText(endpoint.path);
    window.findChild<QPushButton *>("connectButton")->click();
    auto session = window.findChild<Session *>();
    QTRY_VERIFY(!session->model().isEmpty());
    QVERIFY(session->enqueue("~STO"));
    auto row = parameter(window.findChild<QTreeWidget *>("parametersTree"), "STO");
    QTRY_COMPARE(row->text(1), QString("0"));
    window.findChild<QCheckBox *>("enableWrites")->setChecked(true);
    auto choice = window.findChild<QComboBox *>("choice_STO_0");
    QVERIFY(choice->isEnabled());
    choice->setCurrentIndex(choice->findData(1));
    endpoint.overrides["!R 0"] = pauseReply;
    endpoint.overrides["!EX"] = stopReply;
    endpoint.overrides["?FF"] = faultReply;
    endpoint.overrides["?FS"] = statusReply;
    const int first = endpoint.commands.size();
    QTimer::singleShot(30, [] {
      if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
        box->button(QMessageBox::Yes)->click();
    });
    window.findChild<QPushButton *>("applyButton")->click();
    if (success) {
      QTRY_COMPARE(row->text(1), QString("1"));
      QTRY_VERIFY(window.findChild<QPushButton *>("applyButton")->isEnabled());
      const auto commands = endpoint.commands.mid(first);
      const int pause = commands.indexOf("!R 0");
      const int stop = commands.indexOf("!EX", pause + 1);
      const int faults = commands.indexOf("?FF", stop + 1);
      const int flags = commands.indexOf("?FS", faults + 1);
      const int write = commands.indexOf("^STO 1", flags + 1);
      const int read = commands.indexOf("~STO", write + 1);
      QVERIFY(pause >= 0 && stop > pause && faults > stop && flags > faults &&
              write > flags && read > write);
      QCOMPARE(endpoint.params["STO"], qint64(1));
    } else {
      QTRY_VERIFY(!session->writesEnabled());
      QCOMPARE(endpoint.params["STO"], qint64(0));
    }
    for (const auto &cmd : endpoint.commands.mid(first)) {
      if (cmd.startsWith('^'))
        QVERIFY(success && cmd == "^STO 1");
      QVERIFY(!cmd.startsWith('%')); // No implicit EEPROM persistence.
      QVERIFY(cmd != "!MG" && !cmd.startsWith("!S ") && !cmd.startsWith("!G "));
    }
  }

  void configurationChoicesUnlockAfterReadOnly() {
    FakeController endpoint;
    Window window;
    window.show();
    window.findChild<QComboBox *>("sourceSelector")->setCurrentIndex(1);
    window.findChild<QComboBox *>("portSelector")->setEditText(endpoint.path);
    window.findChild<QPushButton *>("connectButton")->click();
    auto session = window.findChild<Session *>();
    QTRY_VERIFY(!session->model().isEmpty());
    auto edit = window.findChild<QCheckBox *>("enableWrites");
    auto choice = window.findChild<QComboBox *>("choice_STO_0");
    auto draft = window.findChild<QDoubleSpinBox *>("draft_STO_0");
    auto unknown = window.findChild<QComboBox *>("choice_PMOD_1");
    QVERIFY(choice && draft && unknown);
    QVERIFY(!choice->isEnabled());
    QVERIFY(session->enqueue("~STO"));
    auto row = parameter(window.findChild<QTreeWidget *>("parametersTree"), "STO");
    QTRY_COMPARE(row->text(1), QString("0"));
    for (int i = 0; i < 2; ++i) {
      edit->setChecked(true);
      QVERIFY(choice->isEnabled());
      QVERIFY(!unknown->isEnabled()); // Unread fields must stay blocked.
      choice->setCurrentIndex(choice->findData(1));
      QCOMPARE(draft->value(), 1.0);
      edit->setChecked(false);
      QVERIFY(!choice->isEnabled());
      QCOMPARE(draft->value(), 1.0); // Toggling permission preserves the draft.
    }
    QVERIFY(session->enqueue("~STO"));
    QTRY_COMPARE(endpoint.commands.count("~STO"), 2);
    QCOMPARE(draft->value(), 1.0); // A read also preserves the pending draft.
    edit->setChecked(true);
    QVERIFY(choice->isEnabled());
    choice->setCurrentIndex(choice->findData(0));
    QCOMPARE(draft->value(), 0.0);
    for (const auto &cmd : endpoint.commands)
      QVERIFY2(!cmd.startsWith('^') && !cmd.startsWith('!') &&
                   !cmd.startsWith('%'), qPrintable(cmd));
  }

  void sliderStaysLatchedAfterReleaseAndZeroIsNotEmergency_data() {
    QTest::addColumn<int>("modeIndex");
    QTest::addColumn<QString>("key");
    QTest::newRow("raw-command") << 0 << QString("G");
    QTest::newRow("rpm-command") << 1 << QString("S");
  }
  void sliderStaysLatchedAfterReleaseAndZeroIsNotEmergency() {
    QFETCH(int, modeIndex);
    QFETCH(QString, key);
    FakeController endpoint;
    endpoint.faults = 16;
    Window window;
    window.show();
    window.selectPage(1);
    window.findChild<QComboBox *>("sourceSelector")->setCurrentIndex(1);
    window.findChild<QComboBox *>("portSelector")->setEditText(endpoint.path);
    window.findChild<QPushButton *>("connectButton")->click();
    auto session = window.findChild<Session *>();
    QTRY_VERIFY(!session->model().isEmpty());
    QVERIFY(session->enqueue("~RWD"));
    QTRY_COMPARE(session->watchdogMs(), 1000);
    auto arm = window.findChild<QCheckBox *>("enableMotion");
    auto slider = window.findChild<QSlider *>("motorCommand1");
    window.findChild<QComboBox *>("motorMode1")->setCurrentIndex(modeIndex);
    auto send = window.findChild<QPushButton *>("motorSend1");
    auto accept = [] {
      QTimer::singleShot(30, [] {
        if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
          box->button(QMessageBox::Yes)->click();
      });
    };
    accept(); arm->click();
    QTRY_VERIFY(window.findChild<QLabel *>("manualControlHint")->text().contains("PARADA ATIVA"));
    QVERIFY(slider->isEnabled());
    QVERIFY(!send->isEnabled());
    slider->setValue(100);
    slider->setSliderDown(true);
    QTest::qWait(220);
    slider->setSliderDown(false);
    QVERIFY(!endpoint.commands.contains(QString("!%1 1 100").arg(key)));
    accept(); window.findChild<QPushButton *>("releaseEmergency")->click();
    QTRY_VERIFY(send->isEnabled());
    QVERIFY(endpoint.commands.contains("!MG"));
    emit session->streamFrame("0:0:0:0:483:36:0:0:0:0:16:8");
    QVERIFY(!send->isEnabled());
    emit session->streamFrame("0:0:0:0:483:36:0:0:0:0:0:1");
    QVERIFY(send->isEnabled());
    QVector<qint64> transmissions;
    QElapsedTimer clock; clock.start();
    connect(session, &Session::traffic, &window, [&](QString direction, QString message) {
      if (direction == "TX" && message == QString("!%1 1 110").arg(key)) transmissions.append(clock.elapsed());
    });
    slider->setSliderDown(true);
    slider->setValue(110);
    QTRY_VERIFY(endpoint.commands.count(QString("!%1 1 110").arg(key)) >= 3);
    slider->setSliderDown(false);
    const auto sentBeforeRelease = endpoint.commands.count(QString("!%1 1 110").arg(key));
    QTest::qWait(1500); // Longer than the configured controller watchdog.
    QVERIFY(endpoint.commands.count(QString("!%1 1 110").arg(key)) > sentBeforeRelease + 4);
    QVERIFY(!endpoint.commands.contains("!EX"));
    QVERIFY(session->motionEnabled());
    for (int i = 1; i < transmissions.size(); ++i)
      QVERIFY(transmissions[i] - transmissions[i - 1] < 800);
    QTRY_VERIFY(window.findChild<QLabel *>("motorCommandStatus1")->text().contains("ACK"));
    window.findChild<QPushButton *>("motorStop1")->click();
    QTRY_VERIFY(endpoint.commands.contains(QString("!%1 1 0").arg(key)));
    const int stopped = endpoint.commands.indexOf(QString("!%1 1 0").arg(key));
    QTest::qWait(300);
    QVERIFY(session->connected());
    QVERIFY(session->motionEnabled());
    QVERIFY(!endpoint.commands.contains("!EX"));
    for (int i = stopped + 1; i < endpoint.commands.size(); ++i)
      QVERIFY(endpoint.commands[i] != QString("!%1 1 110").arg(key));
    slider->setValue(-60);
    QTRY_VERIFY(endpoint.commands.contains(QString("!%1 1 -60").arg(key)));
    slider->setValue(0);
    QTRY_VERIFY(endpoint.commands.count(QString("!%1 1 0").arg(key)) >= 2);
    QVERIFY(!endpoint.commands.contains("!EX"));
    slider->setValue(80);
    QTRY_VERIFY(endpoint.commands.contains(QString("!%1 1 80").arg(key)));
    endpoint.faults = 2; // A real controller fault still stops the latched command.
    QTRY_VERIFY(endpoint.commands.contains("!EX"));
    QVERIFY(!session->motionEnabled());
  }

  void positionManualKeepsAbsoluteTargetAndStopsWithoutReturningHome() {
    FakeController endpoint;
    endpoint.params["MMOD 1"] = 3;
    endpoint.params["MVEL 1"] = 30;
    endpoint.params["MXRPM 1"] = 3000;
    Window window; window.show(); window.selectPage(1);
    window.findChild<QComboBox *>("sourceSelector")->setCurrentIndex(1);
    window.findChild<QComboBox *>("portSelector")->setEditText(endpoint.path);
    window.findChild<QPushButton *>("connectButton")->click();
    auto session = window.findChild<Session *>();
    QTRY_VERIFY(!session->model().isEmpty());
    auto mode = window.findChild<QComboBox *>("motorMode1");
    QTRY_COMPARE(mode->currentIndex(), 2); // Fresh connection detects MMOD3.
    QTRY_VERIFY(endpoint.commands.contains("~MXRPM 1"));
    QTest::qWait(100);
    QTRY_COMPARE(session->watchdogMs(), 1000);
    for (const auto &cmd : endpoint.commands)
      QVERIFY2(!cmd.startsWith('!') && !cmd.startsWith('^') && !cmd.startsWith('%'), qPrintable(cmd));
    QVERIFY(session->setWritesEnabled(true));
    QVERIFY(session->setMotionEnabled(true));
    auto slider = window.findChild<QSlider *>("motorCommand1");
    auto send = window.findChild<QPushButton *>("motorSend1");
    QTRY_VERIFY(send->isEnabled());
    mode->setCurrentIndex(0);
    slider->setValue(475); // ACK of G would conceal its ineffectiveness in MMOD3.
    QTest::qWait(150);
    QVERIFY(!endpoint.commands.contains("!G 1 475"));
    QVERIFY(window.findChild<QLabel *>("motorCommandStatus1")->text().contains("G não move"));
    mode->setCurrentIndex(2);
    slider->setSliderDown(true);
    slider->setValue(1000);
    QTest::qWait(150);
    QVERIFY(!endpoint.commands.contains("!G 1 0"));
    slider->setSliderDown(false); // Release alone applies a fixed absolute target.
    QTRY_VERIFY(endpoint.commands.count("!P 1 1000") >= 3);
    QVERIFY(endpoint.commands.indexOf("!G 1 0") < endpoint.commands.indexOf("!S 1 30"));
    QVERIFY(endpoint.commands.indexOf("!S 1 30") < endpoint.commands.indexOf("!P 1 1000"));
    slider->setSliderDown(true);
    slider->setValue(2000); // Dragging previews; the confirmed target stays 1000.
    QTest::qWait(500);
    QVERIFY(!endpoint.commands.contains("!P 1 2000"));
    slider->setSliderDown(false);
    QTRY_VERIFY(endpoint.commands.count("!P 1 2000") >= 2);
    window.findChild<QPushButton *>("motorStop1")->click();
    QTRY_VERIFY(endpoint.commands.contains("!MS 1"));
    QTest::qWait(400);
    QVERIFY(!endpoint.commands.contains("!P 1 0")); // Stop must not request the origin.
    QVERIFY(!endpoint.commands.contains("!EX"));
    const int stopped = endpoint.commands.lastIndexOf("!MS 1");
    for (int i = stopped + 1; i < endpoint.commands.size(); ++i)
      QVERIFY(!endpoint.commands[i].startsWith("!P "));
    slider->setValue(0); send->click(); // Stop already reset the slider to zero.
    QTRY_VERIFY(endpoint.commands.count("!P 1 0") >= 3);
    endpoint.overrides["!P 1 0"] = "-";
    QTRY_VERIFY(endpoint.commands.contains("!EX"));
    QVERIFY(!session->motionEnabled());
    for (const auto &cmd : endpoint.commands) {
      QVERIFY(!cmd.startsWith("!PR "));
      QVERIFY(!cmd.startsWith('^'));
      QVERIFY(!cmd.startsWith('%'));
    }
  }

  void positionManualRejectsUnreadPrerequisite() {
    FakeController endpoint;
    endpoint.params["MMOD 1"] = 3;
    endpoint.params["MVEL 1"] = 30;
    endpoint.params["MXRPM 1"] = 100;
    endpoint.overrides["~MVEL 1"] = "-";
    Window window; window.show(); window.selectPage(1);
    window.findChild<QComboBox *>("sourceSelector")->setCurrentIndex(1);
    window.findChild<QComboBox *>("portSelector")->setEditText(endpoint.path);
    window.findChild<QPushButton *>("connectButton")->click();
    auto session = window.findChild<Session *>();
    QTRY_VERIFY(endpoint.commands.contains("~MXRPM 1"));
    QTest::qWait(100);
    QVERIFY(session->setWritesEnabled(true));
    QVERIFY(session->setMotionEnabled(true));
    auto slider = window.findChild<QSlider *>("motorCommand1");
    auto send = window.findChild<QPushButton *>("motorSend1");
    QTRY_VERIFY(send->isEnabled());
    slider->setValue(1000);
    QTest::qWait(200);
    QVERIFY(!endpoint.commands.contains("!G 1 0"));
    QVERIFY(!endpoint.commands.contains("!P 1 1000"));
    endpoint.overrides.remove("~MVEL 1");
    QVERIFY(session->enqueue("~MVEL 1"));
    QTRY_COMPARE(parameter(window.findChild<QTreeWidget *>("parametersTree"), "MVEL 1")->text(1), QString("30"));
    QTRY_VERIFY(send->isEnabled());
    send->click();
    QTRY_VERIFY(endpoint.commands.contains("!P 1 1000"));
    window.findChild<QPushButton *>("motorStop1")->click();
    QTRY_VERIFY(endpoint.commands.contains("!MS 1"));
  }

  void manualControlsEnableFromMonitorWithoutMotion() {
    FakeController endpoint;
    Window window;
    window.show();
    window.findChild<QComboBox *>("sourceSelector")->setCurrentIndex(1);
    window.findChild<QComboBox *>("portSelector")->setEditText(endpoint.path);
    window.findChild<QPushButton *>("connectButton")->click();
    auto session = window.findChild<Session *>();
    auto arm = window.findChild<QCheckBox *>("enableMotion");
    auto slider = window.findChild<QSlider *>("motorCommand1");
    QTRY_VERIFY(arm->isEnabled());
    QVERIFY(!session->writesEnabled());
    QVERIFY(!slider->isEnabled());
    // Connection reads the watchdog, never changes it or implicitly arms.
    QTRY_COMPARE(session->watchdogMs(), 1000);
    QVERIFY(!session->motionEnabled());
    QVERIFY(!session->writesEnabled());
    QTimer::singleShot(30, [] {
      if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
        box->button(QMessageBox::Cancel)->click();
    });
    arm->click();
    QVERIFY(!session->motionEnabled());
    QTimer::singleShot(30, [] {
      if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
        box->button(QMessageBox::Yes)->click();
    });
    arm->click();
    QVERIFY(session->writesEnabled());
    QVERIFY(session->motionEnabled());
    QVERIFY(slider->isEnabled());
    QVERIFY(arm->isChecked());
    window.findChild<QDoubleSpinBox *>("motorValue1")->setValue(100);
    QTest::qWait(40);
    for (const auto &cmd : endpoint.commands) {
      QVERIFY(!cmd.startsWith('!'));
      QVERIFY(!cmd.startsWith('^'));
      QVERIFY(!cmd.startsWith('%'));
    }
    arm->click();
    QTRY_VERIFY(endpoint.commands.contains("!EX"));
    QVERIFY(!slider->isEnabled());
    // A disabled watchdog cannot be armed through the new shortcut.
    endpoint.params["RWD"] = 0;
    QVERIFY(session->enqueue("~RWD"));
    QTRY_COMPARE(session->watchdogMs(), 0);
    arm->click();
    QVERIFY(!session->motionEnabled());
    QVERIFY(!arm->isChecked());
  }
  void hallAndEncoderRpmAreSeparateAndMissingIsNotZero() {
    FakeController endpoint;
    endpoint.overrides["?S"] = "S=281:-90";
    endpoint.overrides["?BS 1"] = "BS=283";
    endpoint.overrides["?BS 2"] = "BS=-92";
    Window window; window.show();
    window.findChild<QComboBox *>("sourceSelector")->setCurrentIndex(1);
    window.findChild<QComboBox *>("portSelector")->setEditText(endpoint.path);
    window.findChild<QPushButton *>("connectButton")->click();
    QTRY_COMPARE(window.findChild<QLabel *>("metric3")->text(), QString("281"));
    QCOMPARE(window.findChild<QLabel *>("metric4")->text(), QString("-90"));
    QCOMPARE(window.findChild<QLabel *>("hallRpm1")->text(), QString("283"));
    QCOMPARE(window.findChild<QLabel *>("hallRpm2")->text(), QString("-92"));
    QTemporaryDir dir;
    QVERIFY(window.saveCsv(dir.filePath("rpm.csv")));
    QFile file(dir.filePath("rpm.csv")); QVERIFY(file.open(QIODevice::ReadOnly));
    const auto csv = file.readAll();
    QVERIFY(csv.contains("rpm1_hall,rpm2_hall"));
    QVERIFY(csv.contains(",283.000,-92.000"));
    endpoint.overrides["?BS 1"] = "-";
    QTRY_COMPARE(window.findChild<QLabel *>("hallRpm1")->text(), QString("—"));
    QCOMPARE(window.findChild<QLabel *>("metric3")->text(), QString("281"));
  }
  void cprImportStagesSupportedFieldsWithoutSending() {
    Window window;
    window.show();
    window.showCpr(false, QString(TEST_SOURCE_DIR) +
                              "/tests/fixtures/original-SBLG2360T.cpr");
    auto table = window.findChild<QTableWidget *>("cprTable");
    QVERIFY(table);
    QCOMPARE(table->rowCount(), 605);
    QVERIFY(window.findChild<QPushButton *>("importCprDrafts")
                ->text()
                .startsWith("Importar 600 rascunhos"));
    const auto capture = qEnvironmentVariable("MOTORLAB_CAPTURE_DIR");
    if (!capture.isEmpty())
      QVERIFY(window.findChild<QWidget *>("cprDialog")
                  ->grab()
                  .save(capture + "/cpr.png"));
    auto import = window.findChild<QPushButton *>("importCprDrafts");
    QVERIFY(import);
    import->click();
    QCOMPARE(window.findChild<QDoubleSpinBox *>("draft_ALIM_1")->value(), 30.0);
    QVERIFY(window.findChild<QPlainTextEdit *>("trafficConsole")
                ->toPlainText()
                .isEmpty());
  }
  void cprExportKeepsRawScaleAndDemoOrigin() {
    Window window;
    window.show();
    QTemporaryDir dir;
    window.findChild<QDoubleSpinBox *>("draft_ALIM_1")->setValue(12.3);
    auto path = dir.filePath("offline.cpr");
    QVERIFY(window.saveCpr(path));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    auto p = CprProfile::parse(file.readAll());
    QVERIFY(p.valid());
    QCOMPARE(p.firmware, QString("v3.0:SBLG2360T"));
    QCOMPARE(p.items.size(), 1);
    QCOMPARE(p.items[0].command, QString("ALIM"));
    QCOMPARE(p.items[0].multiplier, 10);
    QCOMPARE(p.items[0].value, QString("123"));
    window.startDemo();
    auto session = window.findChild<Session *>();
    QTRY_VERIFY(!session->model().isEmpty());
    QVERIFY(session->enqueue("~ALIM 1"));
    QTRY_COMPARE(
        parameter(window.findChild<QTreeWidget *>("parametersTree"), "ALIM 1")
            ->text(1),
        QString("20.0"));
    window.findChild<QPushButton *>("connectButton")->click();
    auto demoPath = dir.filePath("demo.cpr");
    QVERIFY(window.saveCpr(demoPath));
    QFile df(demoPath);
    QVERIFY(df.open(QIODevice::ReadOnly));
    const auto dp = CprProfile::parse(df.readAll());
    QVERIFY(dp.valid());
    QVERIFY(dp.roborun.contains("DEMO"));
    QCOMPARE(dp.firmware, QString("v3.0:SBLMG2360T"));
  }
  void originalHierarchyAndChoices() {
    Window window;
    window.show();
    auto tree = window.findChild<QTreeWidget *>("parametersTree");
    auto alim = parameter(tree, "ALIM 1");
    QVERIFY(alim);
    QVERIFY(!alim->parent()); // Lists now contain parameters only.
    QCOMPARE(alim->data(0, Qt::UserRole+1).toStringList(),
             QStringList({"Motor 1", "Motor Output", "Amps Limits"}));
    auto choices = window.findChild<QComboBox *>("choice_ISM_0");
    QVERIFY(choices);
    QCOMPARE(choices->count(), 3);
    choices->setCurrentIndex(choices->findData(4));
    QCOMPARE(window.findChild<QDoubleSpinBox *>("draft_ISM_0")->value(), 4.0);
    QVERIFY(window.findChild<QPlainTextEdit *>("trafficConsole")
                ->toPlainText()
                .isEmpty());
  }
  void configurationCardsKeepMotorChannelsAndFlatIndicesSeparate() {
    Window window;
    window.show();
    auto tree = window.findChild<QTreeWidget *>("parametersTree");
    auto browser = window.findChild<ConfigBrowser *>("powerConfigBrowser");
    QVERIFY(browser);
    const auto clickCard = [&](const QString &path) {
      auto card = window.findChild<QPushButton *>("powerConfigCard_"+path);
      QVERIFY2(card, qPrintable(path));
      QVERIFY(card->isVisible());
      QVERIFY(!card->toolTip().isEmpty());
      QTest::mouseClick(card, Qt::LeftButton);
      QCoreApplication::processEvents();
    };
    QVERIFY(window.findChild<QPushButton *>("powerConfigCard_Motor 1")->isVisible());
    QVERIFY(window.findChild<QPushButton *>("powerConfigCard_Motor 2")->isVisible());
    QVERIFY(!tree->isVisible());
    clickCard("Motor 1");
    auto output = window.findChild<QPushButton *>("powerConfigCard_Motor 1|Motor Output");
    QCOMPARE(output->text(), QString("Saída do motor"));
    clickCard("Motor 1|Motor Output");
    clickCard("Motor 1|Motor Output|Amps Limits");
    QVERIFY(tree->isVisible());
    QVERIFY(!parameter(tree,"ALIM 1")->isHidden());
    QVERIFY(parameter(tree,"ALIM 2")->isHidden());
    for (int i=0;i<tree->topLevelItemCount();++i) {
      QCOMPARE(tree->topLevelItem(i)->childCount(),0);
      QVERIFY(!tree->topLevelItem(i)->data(0,Qt::UserRole).toString().isEmpty());
    }
    window.findChild<QPushButton *>("powerConfigHome")->click();
    clickCard("Motor 2");
    clickCard("Motor 2|Motor Output");
    clickCard("Motor 2|Motor Output|Amps Limits");
    QVERIFY(parameter(tree,"ALIM 1")->isHidden());
    QVERIFY(!parameter(tree,"ALIM 2")->isHidden());
    QCOMPARE(parameter(tree,"HSAT 13")->data(0,Qt::UserRole+1).toStringList().first(),QString("Motor 2"));
    QCOMPARE(parameter(tree,"KPG 3")->data(0,Qt::UserRole+1).toStringList(),
             QStringList({"Motor 1","Motor Output","Closed Loop Parameters","Closed Loop Position"}));
    auto filter=window.findChild<QLineEdit *>("parameterFilter");
    filter->setText("aceleracao"); // Accent-independent global search.
    QVERIFY(!parameter(tree,"MAC 1")->isHidden());
    QVERIFY(!parameter(tree,"MAC 2")->isHidden());
    filter->setText("Amps Limit"); // Original RoboRun names remain searchable.
    QVERIFY(!parameter(tree,"ALIM 1")->isHidden());
    QVERIFY(!parameter(tree,"ALIM 2")->isHidden());
    filter->clear();
    QVERIFY(parameter(tree,"ALIM 1")->isHidden());
    QVERIFY(!parameter(tree,"ALIM 2")->isHidden());
    QVERIFY(window.findChild<QPlainTextEdit *>("trafficConsole")->toPlainText().isEmpty());
  }
  void everyConfigurationHasPortugueseTextAndOriginalHoverHelp() {
    Window window;
    window.show();
    const auto dict=ConfigText::dictionary();
    for (const auto &spec:Catalog::all()) {
      const auto translation=dict["parameters"].toObject()[spec.key].toObject();
      QVERIFY2(!translation["name"].toString().isEmpty(),qPrintable(spec.key));
      QVERIFY2(!translation["description"].toString().isEmpty(),qPrintable(spec.key));
      for(int index=spec.indexMax?1:0;index<=spec.indexMax;++index) {
        auto item=parameter(window.findChild<QTreeWidget *>("parametersTree"),Catalog::suffix(spec,index));
        if(!item) item=parameter(window.findChild<QTreeWidget *>("ioCategories"),Catalog::suffix(spec,index));
        QVERIFY(item);
        QVERIFY(item->text(0).startsWith(ConfigText::name(spec,index)));
        QVERIFY(item->toolTip(0).contains(ConfigText::description(spec,index).toHtmlEscaped()));
        for(const auto &group:item->data(0,Qt::UserRole+1).toStringList())
          QVERIFY2(dict["categories"].toObject().contains(group),qPrintable(group));
        if(auto editor=item->treeWidget()->itemWidget(item,2))
          QCOMPARE(editor->toolTip(),item->toolTip(0));
      }
    }
    auto sto=window.findChild<QComboBox *>("choice_STO_0");
    QCOMPARE(sto->itemText(sto->findData(1)),QString("Habilitado"));
    auto filter=window.findChild<QLineEdit *>("parameterFilter");
    filter->setText("ALIM 1");
    auto item=parameter(window.findChild<QTreeWidget *>("parametersTree"),"ALIM 1");
    window.findChild<QTreeWidget *>("parametersTree")->setCurrentItem(item);
    auto help=window.findChild<QLabel *>("powerConfigParameterHelp");
    QVERIFY(help->text().startsWith("<b>Amps Limit (A)</b>"));
    QVERIFY(help->text().contains("corrente máxima"));
  }
  void configurationCardsRenderWithoutClipping() {
    Window window;
    window.resize(1440,860);
    window.show();
    window.startDemo();
    auto session=window.findChild<Session *>();
    for(const auto &suffix:QStringList{"ALIM 1","MXRPM 1","MAC 1","MDEC 1","EDEC 1","MNRPM 1","BPR 1"})
      QVERIFY(session->enqueue("~"+suffix));
    auto tree=window.findChild<QTreeWidget *>("parametersTree");
    QTRY_COMPARE(parameter(tree,"MAC 1")->text(1),QString("0.0"));
    window.findChild<QCheckBox *>("enableWrites")->setChecked(true);
    auto browser=window.findChild<ConfigBrowser *>("powerConfigBrowser");
    const auto capture=[&](const QString &name) {
      QTest::qWait(60);
      const auto dir=qEnvironmentVariable("MOTORLAB_UI_CAPTURE_DIR");
      if(!dir.isEmpty()) QVERIFY(window.grab().save(dir+"/"+name+".png"));
    };
    capture("cards-home");
    browser->navigate({"Motor 1"});capture("cards-motor-1");
    browser->navigate({"Motor 1","Motor Output"});capture("cards-output");
    browser->navigate({"Motor 1","Motor Output","Speed &amp; Acceleration"});
    tree->setCurrentItem(parameter(tree,"MAC 1"));capture("cards-rampas");
    auto editor=window.findChild<QDoubleSpinBox *>("draft_MAC_1");
    QVERIFY(editor->isVisible());
    QVERIFY(editor->width()>=220);
    window.resize(1100,680);
    QTest::qWait(60);
    QCOMPARE(window.size(),QSize(1100,680));
    QVERIFY(tree->viewport()->height()>=120);
    capture("cards-rampas-1100");
    browser->navigate({"Motor 1","Motor Configuration","Sinusoidal Settings","Hall Sensor Angle Table","Negative Direction"});
    QTest::qWait(60);
    QCOMPARE(window.size(),QSize(1100,680));
    QVERIFY(!parameter(tree,"HSAT 1")->isHidden());
    capture("cards-hall-1100");
  }
  void chartRetainsTimeWindowDuringFastCapture() {
    Chart chart;
    for (int i = 0; i <= 7000; ++i) {
      Sample sample;
      sample.seconds = i / 100.0;
      sample.amp1 = i;
      chart.append(sample);
    }
    QCOMPARE(chart.data().size(), size_t(6001));
    QCOMPARE(chart.data().front().seconds, 10.0);
    QCOMPARE(chart.data().back().amp1, 7000.0);
    chart.clear();
    QVERIFY(chart.data().empty());
  }
  void continuousCaptureDoesNotKeepStaleAnalogInputs() {
    Window window;
    window.show();
    window.startDemo();
    auto inputs = window.findChild<QTableWidget *>("ioValues");
    QTRY_COMPARE_WITH_TIMEOUT(inputs->item(0, 2)->text(), QString("2500"),
                              6000);
    window.selectPage(3);
    auto stream = window.findChild<QPushButton *>("startStreaming");
    QVERIFY(stream->isEnabled());
    stream->click();
    QTRY_COMPARE_WITH_TIMEOUT(stream->text(), QString("Parar captura contínua"),
                              3000);
    QTRY_COMPARE(inputs->item(0, 2)->text(), QString("—"));
    QTRY_COMPARE(inputs->item(0, 3)->text(), QString("—"));
    stream->click();
    QTRY_COMPARE(inputs->item(0, 2)->text(), QString("2500"));
  }
  void invalidProfileDoesNotPartiallyStageOrSend() {
    Window window;
    window.show();
    QTemporaryDir dir;
    auto draft = window.findChild<QDoubleSpinBox *>("draft_ALIM_1");
    draft->setValue(12.3);
    QJsonObject obj{
        {"application", "MotorLab Linux"},
        {"schema", 1},
        {"scope", "catalog_configuration_profile"},
        {"model", "SBLG2360T"},
        {"source", "offline"},
        {"parameters",
         QJsonArray{QJsonObject{{"query", "~ALIM 1"}, {"raw", 150}},
                    QJsonObject{{"query", "~ALIM 2"}, {"raw", 999}}}}};
    QFile f(dir.filePath("bad.json"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(obj).toJson());
    f.close();
    QString error;
    QVERIFY(!window.loadProfile(f.fileName(), &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(draft->value(), 12.3);
    QVERIFY(window.findChild<QPlainTextEdit *>("trafficConsole")
                ->toPlainText()
                .isEmpty());
    obj["parameters"] =
        QJsonArray{QJsonObject{{"query", "~ALIM 1"}, {"raw", 150}}};
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(obj).toJson());
    f.close();
    QVERIFY2(window.loadProfile(f.fileName(), &error), qPrintable(error));
    QCOMPARE(draft->value(), 15.0);
    QVERIFY(window.findChild<QPlainTextEdit *>("trafficConsole")
                ->toPlainText()
                .isEmpty());
    QVERIFY(window.saveProfile(dir.filePath("draft.json")));
    QFile saved(dir.filePath("draft.json"));
    QVERIFY(saved.open(QIODevice::ReadOnly));
    auto profile = QJsonDocument::fromJson(saved.readAll()).object();
    QCOMPARE(profile["source"].toString(), QString("offline"));
    QVERIFY(!profile["complete_catalog_read"].toBool());
  }
  void disconnectedDemoProfileRetainsOriginAndCannotLoadOnReal() {
    Window demo;
    demo.show();
    demo.startDemo();
    auto session = demo.findChild<Session *>();
    QVERIFY(session);
    QTRY_VERIFY(!session->model().isEmpty());
    QVERIFY(session->enqueue("~ALIM 1"));
    QTRY_COMPARE(
        parameter(demo.findChild<QTreeWidget *>("parametersTree"), "ALIM 1")
            ->text(1),
        QString("20.0"));
    demo.findChild<QPushButton *>("connectButton")->click();
    QTemporaryDir dir;
    auto path = dir.filePath("demo.json");
    QVERIFY(demo.saveProfile(path));
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    auto profile = QJsonDocument::fromJson(f.readAll()).object();
    QCOMPARE(profile["source"].toString(), QString("demo"));
    QCOMPARE(profile["model"].toString(), QString("SBLMG2360T"));
    FakeController endpoint;
    Window real;
    real.show();
    real.findChild<QComboBox *>("sourceSelector")->setCurrentIndex(1);
    real.findChild<QComboBox *>("portSelector")->setEditText(endpoint.path);
    real.findChild<QPushButton *>("connectButton")->click();
    auto actual = real.findChild<Session *>();
    QTRY_VERIFY(!actual->model().isEmpty());
    QString error;
    QVERIFY(!real.loadProfile(path, &error));
    QVERIFY(error.contains("simulado"));
    for (auto cmd : endpoint.commands)
      QVERIFY(cmd.startsWith('?') || cmd.startsWith('~'));
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
    Window window;
    window.show();
    window.findChild<QComboBox *>("sourceSelector")->setCurrentIndex(1);
    window.findChild<QComboBox *>("portSelector")->setEditText(endpoint.path);
    window.findChild<QPushButton *>("connectButton")->click();
    auto session = window.findChild<Session *>();
    QTRY_VERIFY(!session->model().isEmpty());
    window.findChild<QCheckBox *>("enableWrites")->setChecked(true);
    endpoint.commands.clear();
    QTimer::singleShot(50, [] {
      auto dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
      if (dialog) dialog->button(QMessageBox::Yes)->click();
    });
    window.findChild<QPushButton *>("EESAV")->click();
    if (saved) {
      QTRY_VERIFY(endpoint.commands.contains("%EESAV"));
      QTRY_VERIFY(window.findChild<QPushButton *>("EESAV")->isEnabled());
    } else {
      QTRY_VERIFY(!session->writesEnabled());
    }
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
    QVERIFY(!session->motionEnabled());
  }
  void unconfirmedExPreventsConfigurationWrite_data() {
    QTest::addColumn<QString>("faultReply");
    QTest::newRow("no-estop") << QString("FF=0");
    QTest::newRow("negative-invalid") << QString("FF=-1");
  }
  void unconfirmedExPreventsConfigurationWrite() {
    QFETCH(QString, faultReply);
    FakeController endpoint;
    endpoint.overrides["?FF"] = faultReply;
    Window window;
    window.show();
    window.findChild<QComboBox *>("sourceSelector")->setCurrentIndex(1);
    window.findChild<QComboBox *>("portSelector")->setEditText(endpoint.path);
    window.findChild<QPushButton *>("connectButton")->click();
    auto session = window.findChild<Session *>();
    QTRY_VERIFY(!session->model().isEmpty());
    QVERIFY(session->enqueue("~ALIM 1"));
    QTRY_COMPARE(
        parameter(window.findChild<QTreeWidget *>("parametersTree"), "ALIM 1")
            ->text(1),
        QString("20.0"));
    window.findChild<QCheckBox *>("enableWrites")->setChecked(true);
    window.findChild<QDoubleSpinBox *>("draft_ALIM_1")->setValue(15);
    QTimer::singleShot(50, [] {
      auto dialog =
          qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
      if (dialog)
        dialog->button(QMessageBox::Yes)->click();
    });
    window.findChild<QPushButton *>("applyButton")->click();
    QTRY_VERIFY(!session->writesEnabled());
    QVERIFY(!endpoint.commands.contains("^ALIM 1 150"));
    QVERIFY(
        window.findChild<QLabel *>("notice")->text().contains("confirmada"));
  }
  void readbackMismatchRevokesWritePermission() {
    FakeController endpoint;
    endpoint.overrides["^ALIM 1 150"] = "+";
    Window window;
    window.show();
    window.findChild<QComboBox *>("sourceSelector")->setCurrentIndex(1);
    window.findChild<QComboBox *>("portSelector")->setEditText(endpoint.path);
    window.findChild<QPushButton *>("connectButton")->click();
    auto session = window.findChild<Session *>();
    QTRY_VERIFY(!session->model().isEmpty());
    QVERIFY(session->enqueue("~ALIM 1"));
    QTRY_COMPARE(
        parameter(window.findChild<QTreeWidget *>("parametersTree"), "ALIM 1")
            ->text(1),
        QString("20.0"));
    window.findChild<QCheckBox *>("enableWrites")->setChecked(true);
    window.findChild<QDoubleSpinBox *>("draft_ALIM_1")->setValue(15);
    QTimer::singleShot(50, [] {
      auto dialog =
          qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
      if (dialog)
        dialog->button(QMessageBox::Yes)->click();
    });
    window.findChild<QPushButton *>("applyButton")->click();
    QTRY_VERIFY(endpoint.commands.contains("^ALIM 1 150"));
    QTRY_VERIFY(!session->writesEnabled());
    QCOMPARE(endpoint.alim, 200);
    QVERIFY(window.findChild<QLabel *>("notice")->text().contains("releitura"));
  }
  void simulatorCannotSendToSerial() {
    FakeController endpoint;
    Window window;
    window.show();
    window.findChild<QComboBox *>("sourceSelector")->setCurrentIndex(1);
    window.findChild<QComboBox *>("portSelector")->setEditText(endpoint.path);
    window.findChild<QPushButton *>("connectButton")->click();
    auto session = window.findChild<Session *>();
    QTRY_VERIFY(!session->model().isEmpty());
    window.findChild<QPlainTextEdit *>("scriptEditor")
        ->setPlainText("SetCommand(_G,1,100)\nprint(7)");
    window.findChild<QPushButton *>("simulateBytecode")->click();
    QTRY_VERIFY(window.findChild<QPlainTextEdit *>("compilerResult"));
    QTRY_VERIFY_WITH_TIMEOUT(
        window.findChild<QPlainTextEdit *>("compilerResult")
            ->toPlainText()
            .contains("Simulação concluída"),
        10000);
    QVERIFY(window.findChild<QPlainTextEdit *>("compilerResult")
                ->toPlainText()
                .contains("SetCommand("));
    QVERIFY(window.findChild<QPushButton *>("saveCompiledHex")->isEnabled());
    const auto capture = qEnvironmentVariable("MOTORLAB_CAPTURE_DIR");
    if (!capture.isEmpty())
      QVERIFY(window.findChild<QWidget *>("compilerDialog")
                  ->grab()
                  .save(capture + "/compiler.png"));
    for (auto cmd : endpoint.commands)
      QVERIFY(cmd.startsWith('?') || cmd.startsWith('~'));
  }

  void demoShowsUnitsAndExportsHonestData() {
    Window window;
    window.show();
    window.startDemo();
    QTRY_VERIFY_WITH_TIMEOUT(window.sampleCount() >= 2, 2500);
    auto voltage = window.findChild<QLabel *>("metric2");
    QVERIFY(voltage);
    QVERIFY(voltage->text().toDouble() > 49 && voltage->text().toDouble() < 52);
    auto temp = window.findChild<QLabel *>("metric5");
    QVERIFY(temp);
    QVERIFY(temp->text().toDouble() < 40);
    QVERIFY(window.findChild<QLabel *>("status")->text().contains("SIMULADOS"));
    auto edit = window.findChild<QCheckBox *>("enableWrites");
    QVERIFY(edit);
    QVERIFY(!edit->isChecked());
    QTemporaryDir dir;
    QVERIFY(window.saveCsv(dir.filePath("samples.csv")));
    QFile csv(dir.filePath("samples.csv"));
    QVERIFY(csv.open(QIODevice::ReadOnly));
    const QByteArray data = csv.readAll();
    QVERIFY(data.contains("source,amp1_rms_A"));
    QVERIFY(data.contains(",demo,"));
    window.findChild<QPushButton *>("connectButton")->click();
    QVERIFY(window.saveCsv(dir.filePath("after-close.csv")));
    QFile closed(dir.filePath("after-close.csv"));
    QVERIFY(closed.open(QIODevice::ReadOnly));
    QVERIFY(closed.readAll().contains(",demo,"));
    QCOMPARE(voltage->text(), QString("—"));
  }
  void parameterEditHasConfirmationAndReadback() {
    Window window;
    window.show();
    window.startDemo();
    window.selectPage(0);
    QTRY_VERIFY_WITH_TIMEOUT(
        window.findChild<QCheckBox *>("enableWrites")->isEnabled(), 1000);
    window.findChild<QPushButton *>("readParameters")->click();
    auto table = window.findChild<QTreeWidget *>("parametersTree");
    QTRY_COMPARE_WITH_TIMEOUT(parameter(table, "ALIM 1")->text(1),
                              QString("20.0"), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(
        window.findChild<QPushButton *>("readParameters")->isEnabled(), 20000);
    auto edit = window.findChild<QCheckBox *>("enableWrites");
    edit->setChecked(true);
    auto draft = window.findChild<QDoubleSpinBox *>("draft_ALIM_1");
    QVERIFY(draft->isEnabled());
    draft->setValue(15.0);
    QTimer::singleShot(70, [] {
      auto dialog =
          qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
      if (dialog)
        dialog->button(QMessageBox::Yes)->click();
    });
    window.findChild<QPushButton *>("applyButton")->click();
    QTRY_COMPARE_WITH_TIMEOUT(parameter(table, "ALIM 1")->text(1),
                              QString("15.0"), 1500);
    QTRY_VERIFY_WITH_TIMEOUT(
        window.findChild<QPushButton *>("applyButton")->isEnabled(), 1000);
    const auto log =
        window.findChild<QPlainTextEdit *>("trafficConsole")->toPlainText();
    QVERIFY(log.contains("TX  ^ALIM 1 150"));
    QVERIFY(log.contains("RX  ALIM=150"));
    QVERIFY(!log.contains("EESAV"));
    QTemporaryDir dir;
    QVERIFY(window.saveSnapshot(dir.filePath("snapshot.json")));
    QFile file(dir.filePath("snapshot.json"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto obj = QJsonDocument::fromJson(file.readAll()).object();
    QCOMPARE(obj["source"].toString(), QString("demo"));
    QCOMPARE(obj["scope"].toString(), QString("catalog_configuration_profile"));
    QVERIFY(obj["parameters"].toArray().size() >= 600);
    QCOMPARE(obj["parameters"].toArray()[0].toObject()["raw"].toInt(), 150);
  }
  void consoleRejectsCommandsAndEditorIsLocal() {
    Window window;
    window.show();
    window.startDemo();
    window.selectPage(2);
    QTRY_VERIFY_WITH_TIMEOUT(
        window.findChild<QPushButton *>("sendQuery")->isEnabled(), 1000);
    auto query = window.findChild<QLineEdit *>("queryInput");
    query->setText("!G 1 1000");
    window.findChild<QPushButton *>("sendQuery")->click();
    QVERIFY(!window.findChild<QPlainTextEdit *>("trafficConsole")
                 ->toPlainText()
                 .contains("TX  !G"));
    window.selectPage(4);
    QVERIFY(window.findChild<QPlainTextEdit *>("scriptEditor")
                ->toPlainText()
                .contains("Não é enviado"));
    QVERIFY(window.findChild<QPushButton *>("uploadScript")->isEnabled());
    QVERIFY(window.findChild<QPushButton *>("runScript")->isEnabled());
    QVERIFY(window.findChild<QPushButton *>("motorStop1")->isEnabled());
  }
  void realSerialPathDisplaysScaledUnitsAndVerifiesRamEdit_data() {
    QTest::addColumn<QString>("model");
    QTest::addColumn<QString>("family");
    QTest::newRow("SBLMG2360T")
        << QString("SBLMG2360T") << QString("SBLMG2XXX");
    QTest::newRow("SBLG2360T") << QString("SBLG2360T") << QString("SBLG2XXX");
  }
  void realSerialPathDisplaysScaledUnitsAndVerifiesRamEdit() {
    QFETCH(QString, model);
    QFETCH(QString, family);
    FakeController endpoint;
    endpoint.overrides["?TRN"] = "TRN=RoboG4:" + model;
    endpoint.overrides["?FID"] = "FID=Roboteq v3.0f " + family;
    Window window;
    window.show();
    window.findChild<QComboBox *>("sourceSelector")->setCurrentIndex(1);
    window.findChild<QComboBox *>("portSelector")->setEditText(endpoint.path);
    window.findChild<QPushButton *>("connectButton")->click();
    QTRY_VERIFY_WITH_TIMEOUT(window.sampleCount() >= 1, 2000);
    QCOMPARE(window.findChild<QLabel *>("metric0")->text(), QString("12.5"));
    QCOMPARE(window.findChild<QLabel *>("metric1")->text(), QString("4.3"));
    QCOMPARE(window.findChild<QLabel *>("metric2")->text(), QString("51.2"));
    QCOMPARE(window.findChild<QLabel *>("metric4")->text(), QString("-210"));
    QCOMPARE(window.findChild<QLabel *>("metric5")->text(), QString("37"));
    QCOMPARE(window.findChild<QLabel *>("detectedModel")->text(),
             "Detectado: " + model);
    QVERIFY(window.findChild<QCheckBox *>("enableWrites")->isEnabled());
    QVERIFY(!window.findChild<QCheckBox *>("enableWrites")->isChecked());
    for (const auto &cmd : endpoint.commands) {
      QVERIFY(!cmd.startsWith('^'));
      QVERIFY(!cmd.startsWith('!'));
      QVERIFY(!cmd.startsWith('%'));
    }
    window.selectPage(0);
    window.findChild<QPushButton *>("readParameters")->click();
    QTRY_VERIFY_WITH_TIMEOUT(
        window.findChild<QPushButton *>("readParameters")->isEnabled(), 20000);
    window.findChild<QCheckBox *>("enableWrites")->setChecked(true);
    auto draft = window.findChild<QDoubleSpinBox *>("draft_ALIM_1");
    QVERIFY(draft->isEnabled());
    draft->setValue(10);
    QTimer::singleShot(70, [] {
      auto dialog =
          qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
      if (dialog)
        dialog->button(QMessageBox::Yes)->click();
    });
    window.findChild<QPushButton *>("applyButton")->click();
    QTRY_COMPARE_WITH_TIMEOUT(endpoint.alim, 100, 1500);
    QTRY_COMPARE_WITH_TIMEOUT(
        parameter(window.findChild<QTreeWidget *>("parametersTree"), "ALIM 1")
            ->text(1),
        QString("10.0"), 1500);
    QTemporaryDir dir;
    QVERIFY(window.saveCsv(dir.filePath("serial.csv")));
    QFile f(dir.filePath("serial.csv"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const auto data = f.readAll();
    QVERIFY(
        data.contains(",serial,12.500,4.300,850.000,-210.000,51.200,37.000,"));
    QVERIFY(!data.contains(",demo,"));
    QVERIFY(window.saveSnapshot(dir.filePath("snapshot.json")));
    QFile saved(dir.filePath("snapshot.json"));
    QVERIFY(saved.open(QIODevice::ReadOnly));
    const auto snapshot = QJsonDocument::fromJson(saved.readAll()).object();
    QCOMPARE(snapshot["model"].toString(), model);
    QCOMPARE(snapshot["firmware"].toString(), "Roboteq v3.0f " + family);
    QCOMPARE(snapshot["version"].toString(), QString("0.6.0"));
    endpoint.overrides["?A"] = "A=100:70";
    const int previous = window.sampleCount();
    QTRY_COMPARE_WITH_TIMEOUT(window.findChild<QLabel *>("metric0")->text(),
                              QString("10.0"), 1500);
    QVERIFY(window.sampleCount() > previous);
    for (const auto &name : {"chartFeed", "diagnosticFeed"}) {
      const auto feed = window.findChild<QTableWidget *>(name);
      QVERIFY(feed);
      QCOMPARE(feed->item(0, 1)->text(), QString("10.0"));
      QCOMPARE(feed->item(0, 2)->text(), QString("10.0"));
      QCOMPARE(feed->item(0, 3)->text(), QString("12.5"));
      QCOMPARE(feed->item(1, 2)->text(), QString("4.3"));
      QCOMPARE(feed->item(1, 3)->text(), QString("7.0"));
      QCOMPARE(feed->item(4, 1)->text(), QString("-210"));
    }
    window.findChild<QPushButton *>("connectButton")->click();
    QCOMPARE(window.findChild<QLabel *>("detectedModel")->text(),
             QString("Detectado: —"));
    QVERIFY(!window.findChild<QCheckBox *>("enableWrites")->isChecked());
  }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
