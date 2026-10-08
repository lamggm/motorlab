#include "control.h"
#include "control_host.h"
#include <QDateTime>
#include <QUuid>
#include <cmath>
#include <stdexcept>

QJsonObject ControlServer::continuousState() const {
  return {{"active", continuous.enabled}, {"running", continuous.running && !continuous.stopping},
          {"stopping", continuous.stopping}, {"channel", continuous.channel},
          {"command_value", continuous.value}, {"unit", continuous.position ? "encoder_position_counts" : continuous.encoderSpeed ? "encoder_speed_rpm" : "open_loop_command_not_rpm"},
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
  if (command.startsWith("!G ") || command.startsWith("!S ") || command.startsWith("!P ")) {
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
  if (key == "MMOD" && n != (continuous.position ? 3 : continuous.encoderSpeed ? 1 : 0))
    continuousStop("Modo do controlador diferente do modo solicitado.");
  if (continuous.encoderSpeed || continuous.position) {
    if (key == "BLFB" && n != 0) continuousStop("Realimentação externa não selecionada.");
    if (key == "EMOD" && n != 2 + 16 * continuous.channel) continuousStop("Encoder não associado como feedback do canal.");
    if (key == "EPPR" && n == 0) continuousStop("Resolução do encoder inválida.");
    if (key == "ALIM" && (n < 1 || n > continuous.maxAmps * 10)) continuousStop("Limite de corrente do driver excede o limite do ensaio.");
    if ((key == "MXPF" || key == "MXPR") && (n < 1 || n > continuous.maxPowerPercent)) continuousStop("Limite de potência do driver excede o limite explícito do ensaio.");
    if (key == "MXRPM") {
      continuous.driverMaxRpm = int(n);
      if ((!continuous.position && n < std::abs(continuous.value)) || n < 1 || n > continuous.maxRpm)
        continuousStop("Limite de velocidade do driver incompatível com a guarda; não foi alterado.");
    }
    if (key == "MVEL" && continuous.position) {
      if (n < 1 || n > continuous.driverMaxRpm || n > continuous.maxRpm)
        continuousStop("Velocidade da trajetória excede MXRPM ou a guarda, ou não é positiva.");
      continuous.positionVelocity = int(n);
    }
    if (key == "F") continuous.sample[continuous.position ? "feedback_position_counts" : "feedback_rpm"] = double(n);
    if (key == "TR") continuous.sample["track_position_counts"] = double(n);
    if (key == "VDV") continuous.sample["velocity_demand_rpm"] = double(n);
    if (key == "RMP") continuous.sample["ramped_command"] = double(n);
    if (key == "E") continuous.sample[continuous.position ? "trajectory_error_counts" : "speed_error_rpm"] = double(n);
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
    if (continuous.encoderSpeed || continuous.position) {
      const auto measured = parsed.numbers[continuous.channel - 1];
      if (key == "S" && (std::abs(double(measured)) > continuous.maxRpm ||
          (continuous.running && !continuous.position && measured * continuous.value < -1000)))
        continuousStop("Encoder excedeu limite de RPM ou indicou sentido contrário.");
      if (key == "S" && continuous.position && !continuous.running &&
          (parsed.numbers[0] != 0 || parsed.numbers[1] != 0))
        continuousStop("Posicionamento requer ambos os motores inicialmente parados.");
      if (key == "C") {
        if (continuous.position) {
          if (!continuous.haveOrigin) {
            continuous.positionOrigin = measured;
            continuous.haveOrigin = true;
            if (std::abs(measured - continuous.value) > 4000)
              continuousStop("Deslocamento de posição excede 4000 contagens.");
          }
          const auto lo = qMin(continuous.positionOrigin, qint64(continuous.value)) - 100;
          const auto hi = qMax(continuous.positionOrigin, qint64(continuous.value)) + 100;
          continuous.sample["target_error_counts"] = double(continuous.value - measured);
          if (continuous.running && (measured < lo || measured > hi))
            continuousStop("Posição excedeu a janela do ensaio (margem de 100 contagens).");
        }
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
  w.continuousUpdate(sample, continuous.channel, continuous.counter);
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
  if (continuous.position) {
    // Refresh the same absolute P target; never repeat relative PR commands.
    // G0 selects the serial source without a position setpoint in MMOD3.
    // S sets trajectory velocity; neither command changes the absolute target.
    steps[0] = Step{QString("!G %1 0").arg(ch), {}};
    steps.insert(1, Step{QString("!S %1 %2").arg(ch).arg(continuous.positionVelocity), {}});
    steps.insert(2, Step{QString("!P %1 %2").arg(ch).arg(continuous.value), {}});
  }
  stepIndex = 0;
  if (continuous.encoderSpeed || continuous.position) {
    steps.append(Step{"?F " + ch, {}});
    steps.append(Step{"?RMP " + ch, {}});
    if (continuous.position) {
      steps.append(Step{"?TR " + ch, {}});
      steps.append(Step{"?VDV " + ch, {}});
    }
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
      !args.contains("value") || (args["value"].toInt() == 0 && args["mode"].toString() != "encoder_position"))
    throw std::runtime_error("Informe action=start, channel e value não zero.");
  const bool position = args["mode"].toString() == "encoder_position";
  const bool encoderSpeed = args["mode"].toString() == "encoder_speed";
  if (!encoderSpeed && !position && std::abs(args["value"].toInt()) > 1000)
    throw std::runtime_error("Comando G em malha aberta deve ficar entre -1000 e 1000; não é RPM.");
  if (encoderSpeed && std::abs(args["value"].toInt()) > args["max_speed_rpm"].toInt(3000))
    throw std::runtime_error("Referência RPM excede a guarda do ensaio.");
  authorize(args);
  continuous = Continuous{};
  continuous.enabled = true;
  continuous.position = position;
  continuous.encoderSpeed = args["mode"].toString() == "encoder_speed";
  continuous.fastCapture = position || args["capture_fast"].toBool();
  continuous.channel = args["channel"].toInt();
  continuous.value = args["value"].toInt();
  continuous.leaseMs = args["lease_ms"].toInt(3000);
  continuous.maxAmps = args["max_current_a"].toDouble(11.5);
  continuous.maxRpm = args["max_speed_rpm"].toInt(3000);
  continuous.maxTemperature = args["max_temperature_c"].toInt(85);
  continuous.maxPowerPercent = args["max_power_percent"].toInt(100);
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
  if (continuous.encoderSpeed || continuous.position)
    for (const auto &key : {"BLFB", "EMOD", "EPPR", "ALIM", "MXPF", "MXPR", "MXRPM"})
      preflight.append(Step{"~" + QString(key) + " " + ch, {}});
  if (position) preflight.append(Step{"~MVEL " + ch, {}});
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
