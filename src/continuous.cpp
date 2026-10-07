#include "control.h"
#include "window.h"
#include <QDateTime>
#include <QUuid>
#include <cmath>
#include <stdexcept>

QJsonObject ControlServer::continuousState() const {
  return {{"active", continuous.enabled}, {"running", continuous.running && !continuous.stopping},
          {"stopping", continuous.stopping}, {"channel", continuous.channel},
          {"command_value", continuous.value}, {"unit", continuous.encoderSpeed ? "encoder_speed_rpm" : "open_loop_command_not_rpm"},
          {"lease_remaining_ms", continuous.enabled ? qMax<qint64>(0, continuous.leaseMs - continuous.lease.elapsed()) : 0},
          {"cycles", double(continuous.cycles)}, {"stop_reason", continuous.reason},
          {"sample", continuous.sample}};
}

void ControlServer::continuousStop(const QString &reason) {
  if (!continuous.enabled || continuous.stopping)
    return;
  continuous.stopping = true;
  continuous.reason = reason;
  w.session.cancelMotion();
  tick.start(); // Drain an outstanding reply, then EX; never queue more motion.
}

void ControlServer::continuousObserve(const QString &command, const QString &payload, bool ok) {
  if (!ok) {
    continuousStop("Consulta/comando rejeitado: " + command);
    return;
  }
  if (command.startsWith("!G ") || command.startsWith("!S ")) {
    continuous.motorAck.restart();
    return;
  }
  if (command == "!MG")
    return;
  const auto key = command.mid(1).section(' ', 0, 0);
  const auto parsed = Protocol::reply(key + "=" + payload);
  if (!parsed.numeric || parsed.numbers.isEmpty()) {
    continuousStop("Resposta numérica inválida: " + command);
    return;
  }
  const auto n = parsed.numbers[0];
  if (key == "RWD" && (n < 1000 || n > 2000))
    continuousStop("Modo contínuo requer watchdog entre 1000 e 2000 ms; não foi alterado.");
  if (key == "MMOD" && n != (continuous.encoderSpeed ? 1 : 0))
    continuousStop("Modo do controlador diferente do modo solicitado.");
  if (continuous.encoderSpeed) {
    if (key == "BLFB" && n != 0) continuousStop("Realimentação externa não selecionada.");
    if (key == "EMOD" && n != 2 + 16 * continuous.channel) continuousStop("Encoder não associado como feedback do canal.");
    if (key == "EPPR" && n == 0) continuousStop("Resolução do encoder inválida.");
    if (key == "ALIM" && (n < 1 || n > continuous.maxAmps * 10)) continuousStop("Limite de corrente do driver excede o limite do ensaio.");
    if ((key == "MXPF" || key == "MXPR") && (n < 1 || n > continuous.maxPowerPercent)) continuousStop("Limite de potência do driver excede o limite explícito do ensaio.");
    if (key == "MXRPM" && (n < std::abs(continuous.value) || n > continuous.maxRpm)) continuousStop("Limite de velocidade do driver incompatível com o ensaio.");
    if (key == "F") continuous.sample["feedback_rpm"] = double(n);
    if (key == "RMP") continuous.sample["ramped_command"] = double(n);
    if (key == "E") continuous.sample["speed_error_rpm"] = double(n);
    if (key == "MA") {
      const int axis = command.section(' ', -1).toInt();
      continuous.sample[axis % 2 ? "flux_amps" : "torque_amps"] = double(n) / 10.0;
      if (std::abs(double(n) / 10.0) > continuous.maxAmps)
        continuousStop("Limite de corrente FOC medida excedido.");
    }
    if (key == "P" && command.section(' ', -1).toInt() == continuous.channel)
      continuous.sample["pwm_per_mille"] = double(n);
  }
  if (key == "MXMD")
    for (auto v : parsed.numbers)
      if (v != 0)
        continuousStop("Modo misto não permitido neste ensaio de um canal.");
  if (key == "SCRO" && n != 0)
    continuousStop("Script automático habilitado; execução contínua recusada.");
  if (key == "CPRI" && n != 0)
    continuousStop("Comando serial não é a primeira prioridade.");
  if (key == "FS" && (n & (128 | 256)))
    continuousStop("Script em execução; execução contínua recusada.");
  if (key == "FS")
    continuous.sample["flags"] = double(n);
  if (key == "FM") {
    continuous.sample["motor_flags" + command.section(' ', -1)] = double(n);
    if (continuous.running && continuous.fastCapture && (n & 4))
      continuousStop("Erro de acompanhamento da velocidade (LoopErr): " + command);
  }
  if (key == "FF") {
    continuous.sample["faults"] = double(n);
    if (n < 0 || n > 65535 || (continuous.running ? n != 0 : (n & ~16) != 0))
      continuousStop("Falha do controlador: FF=" + payload);
  }
  if ((key == "P" || key == "M") && command.section(' ', -1).toInt() != continuous.channel && n != 0)
    continuousStop("Outro canal tem potência aplicada.");
  if (key == "BS") {
    continuous.sample["rpm" + command.section(' ', -1)] = double(n);
    if (command == QString("?BS %1").arg(continuous.channel)) {
      continuous.sample["rpm"] = double(n);
      if (std::abs(double(n)) > continuous.maxRpm)
        continuousStop("Limite de velocidade medida excedido.");
    } else if (n != 0)
      continuousStop("Outro canal está em movimento.");
  }
  if (key == "CB") {
    if (continuous.haveCounter && n != continuous.counter)
      continuous.progress.restart();
    continuous.counter = n;
    continuous.haveCounter = true;
    continuous.sample["counter"] = double(n);
  }
  if (key == "S" || key == "C") {
    if (parsed.numbers.size() != 2) {
      continuousStop("Resposta dos dois encoders indisponível: " + command);
      return;
    }
    const auto prefix = key == "S" ? "encoder_rpm" : "encoder_counter";
    continuous.sample[QString(prefix) + "1"] = double(parsed.numbers[0]);
    continuous.sample[QString(prefix) + "2"] = double(parsed.numbers[1]);
    if (continuous.encoderSpeed) {
      const auto measured = parsed.numbers[continuous.channel - 1];
      if (key == "S" && (std::abs(double(measured)) > continuous.maxRpm ||
          (continuous.running && measured * continuous.value < -1000)))
        continuousStop("Encoder excedeu limite de RPM ou indicou sentido contrário.");
      if (key == "C") {
        if (continuous.haveEncoder && measured != continuous.encoderCounter) continuous.encoderProgress.restart();
        continuous.encoderCounter = measured;
        continuous.haveEncoder = true;
      }
    }
  }
  if (key == "A") {
    if (parsed.numbers.size() != 2) {
      continuousStop("Correntes dos dois canais indisponíveis.");
      return;
    }
    const auto amps = double(parsed.numbers[continuous.channel - 1]) / 10.0;
    continuous.sample["amps1"] = double(parsed.numbers[0]) / 10.0;
    continuous.sample["amps2"] = double(parsed.numbers[1]) / 10.0;
    continuous.sample["amps"] = amps;
    if (std::abs(amps) > continuous.maxAmps)
      continuousStop("Limite de corrente medida excedido.");
  }
  if (key == "V") {
    continuous.sample["voltage"] = double(n) / 10;
    if (n < 100 || n > 600)
      continuousStop("Barramento fora de 10 a 60 V.");
  }
  if (key == "T") {
    continuous.sample["temperature"] = double(n);
    if (n >= continuous.maxTemperature)
      continuousStop("Limite de temperatura atingido.");
  }
  continuous.sample["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

void ControlServer::continuousCycle() {
  if (continuous.stopping) {
    tick.start();
    return;
  }
  if (continuous.fastCapture && continuous.cycles > 0) {
    auto samples = jobs[active]["samples"].toArray();
    auto completed = continuous.sample;
    completed["cycle"] = double(continuous.cycles);
    samples.append(completed);
    if (samples.size() > 2000) samples.removeAt(0);
    jobs[active]["samples"] = samples;
  }
  ++continuous.cycles;
  jobs[active]["phase"] = "continuous";
  jobs[active]["continuous"] = continuousState();
  const auto &sample = continuous.sample;
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
      .arg(continuous.channel).arg(continuous.counter));
  w.continuousHistoryNote->setVisible(true);
  w.chart->setEnabled(false);
  w.ioValues->setEnabled(false);
  for (auto feed : {w.chartFeed, w.diagnosticFeed})
    for (int row = 0; row < feed->rowCount(); ++row)
      feed->item(row, 1)->setText("—");
  w.notify(QString("CONTÍNUO • M%1 • comando %2 • %3 RPM • %4 A — PARADA: !EX")
               .arg(continuous.channel).arg(continuous.value)
               .arg(continuous.sample["rpm"].toDouble()).arg(continuous.sample["amps"].toDouble()));
  const auto ch = QString::number(continuous.channel);
  steps = {{QString("!%1 %2 %3").arg(continuous.encoderSpeed ? "S" : "G").arg(ch).arg(continuous.value), {}},
           {"?FF", {}}, {"?BS " + ch, {}}, {"?CB " + ch, {}},
           {"?A", {}}, {"?V 2", {}}, {"?T 2", {}}, {"?FS", {}},
           {QString("?M %1").arg(3 - continuous.channel), {}},
           {QString("?BS %1").arg(3 - continuous.channel), {}},
           {"?FM 1", {}}, {"?FM 2", {}}, {"?S", {}}, {"?C", {}}};
  stepIndex = 0;
  if (continuous.encoderSpeed) {
    steps.append(Step{"?F " + ch, {}});
    steps.append(Step{"?RMP " + ch, {}});
    if (continuous.fastCapture) {
      steps.append(Step{"?E " + ch, {}});
      steps.append(Step{"?P " + ch, {}});
      steps.append(Step{QString("?MA %1").arg(2 * continuous.channel - 1), {}});
      steps.append(Step{QString("?MA %1").arg(2 * continuous.channel), {}});
    }
  }
  continuous.sample["cycle_started_utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
  waiting.clear();
  afterSteps = [this] { continuousCycle(); };
  tick.start();
}

QJsonObject ControlServer::continuousStart(const QJsonObject &args) {
  if (args["action"].toString() != "start" || !args.contains("channel") ||
      !args.contains("value") || args["value"].toInt() == 0)
    throw std::runtime_error("Informe action=start, channel e value não zero.");
  const bool encoderSpeed = args["mode"].toString() == "encoder_speed";
  if (!encoderSpeed && (std::abs(args["value"].toInt()) > 100 || args["max_speed_rpm"].toInt(600) > 1000 || args["max_current_a"].toInt(10) > 11))
    throw std::runtime_error("Malha aberta limitada a G +/-100, guarda de 1000 RPM e 11 A.");
  if (encoderSpeed && std::abs(args["value"].toInt()) > args["max_speed_rpm"].toInt(600))
    throw std::runtime_error("Referência RPM excede a guarda do ensaio.");
  authorize(args);
  continuous = Continuous{};
  continuous.enabled = true;
  continuous.encoderSpeed = args["mode"].toString() == "encoder_speed";
  continuous.fastCapture = args["capture_fast"].toBool();
  continuous.channel = args["channel"].toInt();
  continuous.value = args["value"].toInt();
  continuous.leaseMs = args["lease_ms"].toInt(3000);
  continuous.maxAmps = args["max_current_a"].toInt(10);
  continuous.maxRpm = args["max_speed_rpm"].toInt(600);
  continuous.maxTemperature = args["max_temperature_c"].toInt(60);
  continuous.maxPowerPercent = args["max_power_percent"].toInt(25);
  continuous.noMotionMs = args["no_motion_ms"].toInt(2000);
  continuous.token = QUuid::createUuid().toString(QUuid::WithoutBraces);
  continuous.lease.start();
  continuous.progress.start();
  continuous.encoderProgress.start();
  supervision.start();
  const auto ch = QString::number(continuous.channel);
  const auto other = QString::number(3 - continuous.channel);
  const bool release = args["release"].toBool();
  QList<Step> preflight;
  if (continuous.encoderSpeed)
    for (const auto &key : {"BLFB", "EMOD", "EPPR", "ALIM", "MXPF", "MXPR", "MXRPM"})
      preflight.append(Step{"~" + QString(key) + " " + ch, {}});
  preflight.append({{"?S", {}}, {"?C", {}}});
  preflight.append({{"~RWD", {}}, {"~MMOD " + ch, {}},
      {"~MXMD", {}}, {"~SCRO", {}}, {"~CPRI 1", {}}, {"?FS", {}}, {"?FF", {}},
      {"?M " + other, {}}, {"?P " + other, {}}, {"?BS " + other, {}}, {"?BS " + ch, {}},
      {"?CB " + ch, {}}, {"?A", {}}, {"?V 2", {}}, {"?T 2", {}}});
  auto result = sequence("continuous", preflight, false,
      [this, release] {
        if (continuous.sample["faults"].toInt() == 16 && !release) {
          continuousStop("EStop ativo; liberação explícita necessária.");
          return;
        }
        if (!w.session.setMotionEnabled(true)) {
          continuousStop("Permissão de movimento recusada.");
          return;
        }
        if (release) {
          steps = {{"!MG", {}}, {"?FF", "0"}};
          stepIndex = 0;
          afterSteps = [this] {
            continuous.running = true;
            continuous.progress.restart();
            continuous.encoderProgress.restart();
            continuousCycle();
          };
          tick.start();
        } else {
          continuous.running = true;
          continuous.progress.restart();
          continuous.encoderProgress.restart();
          continuousCycle();
        }
      });
  // Continuous operation is bounded by a renewable monotonic lease, never by
  // disabling the controller watchdog or by reusing the bounded execute API.
  deadline.stop();
  if (continuous.fastCapture) tick.setInterval(1);
  result["lease_token"] = continuous.token;
  result["lease_ms"] = continuous.leaseMs;
  return result;
}
