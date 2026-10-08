#include "control.h"
#include "control_host.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QUuid>
#include <cmath>
#include <stdexcept>
namespace {
void check(bool ok, const QString &message) {
  if (!ok)
    throw std::runtime_error(message.toStdString());
}
} // namespace
void ControlServer::guiEvent(const QString &kind, QJsonObject data) {
  data["kind"] = kind;
  data["serial"] = double(++guiSerial);
  guiEvents.append(data);
  while (guiEvents.size() > 4096)
    guiEvents.removeFirst();
}
QJsonObject ControlServer::guiSnapshot(const QString &client, quint64 since,
                                       bool initial) {
  QJsonObject result = state();
  result["gui_api_version"] = 1;
  result["usb_path"] = w.session.usbPath();
  result["uploading"] = w.session.uploading();
  result["session_busy"] = w.session.busy();
  result["operation_owner"] = guiOperation;
  result["client_operation"] = guiOperation == client;
  result["other_operation"] =
      !active.isEmpty() || (!guiOperation.isEmpty() && guiOperation != client);
  result["serial"] = double(guiSerial);
  QJsonArray events;
  if (!initial) {
    check(since <= guiSerial, "Cursor de eventos inválido.");
    check(guiEvents.isEmpty() ||
              since + 1 >=
                  quint64(guiEvents.first().toObject()["serial"].toDouble()),
          "Eventos expiraram; reconecte a janela sem retomar comandos.");
    for (const auto &event : guiEvents)
      if (event.toObject()["serial"].toDouble() > double(since))
        events.append(event);
  }
  result["events"] = events;
  QJsonObject observations;
  for (auto it = guiReadings.begin(); it != guiReadings.end(); ++it)
    observations[it.key()] = it.value();
  result["observations"] = observations;
  QJsonArray params;
  for (const auto &p : w.parameters())
    params.append(QJsonObject{{"suffix", p.suffix},
                              {"raw", double(p.raw)},
                              {"draft_raw", double(p.draftRaw)},
                              {"known", p.known},
                              {"dirty", p.dirty}});
  const auto hash =
      QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(params).toJson(
                                                       QJsonDocument::Compact),
                                                   QCryptographicHash::Sha256)
                              .toHex());
  auto &c = guiClients[client];
  if (initial || hash != c.parametersHash) {
    result["parameters"] = params;
    c.parametersHash = hash;
  }
  if (initial || c.script != w.scriptText()) {
    result["script"] = w.scriptText();
    c.script = w.scriptText();
  }
  result["telemetry"] = w.telemetry(1);
  result["selected_model"] = w.selectedModel();
  return result;
}
void ControlServer::releaseGui(const QString &client, bool lost) {
  const auto owned = guiMotors.keys(client);
  if (!owned.isEmpty() && w.session.connected()) {
    if (lost)
      w.session.enqueue("!EX");
    else
      for (auto ch : owned) {
        const auto key = guiMotorKeys.value(ch);
        if (ch == 0) {
          w.session.enqueue(key == "R" ? "!R 0" : "%CLMOD 0");
          w.session.enqueue("!EX");
          continue;
        }
        w.session.enqueue(key == "P" ? QString("!MS %1").arg(ch)
                                     : QString("!%1 %2 0").arg(key).arg(ch));
      }
  }
  for (auto ch : owned) {
    guiMotors.remove(ch);
    guiMotorKeys.remove(ch);
  }
  if (guiOperation == client) {
    if (lost && w.session.connected() && guiClients[client].operationWrites)
      w.session.enqueue("!EX");
    guiOperation.clear();
    w.endOperation();
  }
  guiClients.remove(client);
}
QJsonObject ControlServer::guiDispatch(const QJsonObject &a) {
  check(w.headless(), "O cliente gráfico precisa do motorlabd.");
  const auto client = a["client"].toString(), action = a["action"].toString();
  check(!QUuid(client).isNull(), "Cliente gráfico inválido.");
  if (action == "attach") {
    check(guiClients.size() < 16 || guiClients.contains(client),
          "Muitas janelas conectadas.");
    guiClients[client].heartbeat.start();
    return guiSnapshot(client, guiSerial, true);
  }
  check(guiClients.contains(client),
        "Cliente gráfico expirou; abra novamente a janela.");
  guiClients[client].heartbeat.restart();
  if (action == "sync") {
    if (a["full"].toBool())
      guiClients[client].parametersHash.clear();
    return guiSnapshot(client, quint64(a["since"].toDouble()));
  }
  if (action == "detach") {
    releaseGui(client, false);
    return {};
  }
  auto &s = w.session;
  if (action == "operation") {
    if (!a["enabled"].toBool()) {
      if (guiOperation == client) {
        guiOperation.clear();
        w.endOperation();
      }
      return {};
    }
    if (guiOperation == client)
      return {};
    requireIdle();
    guiClients[client].operationWrites = false;
    guiOperation = client;
    w.beginOperation();
    return {};
  }
  if (action == "enqueue" &&
      Protocol::command(a["command"].toString()).key == "EX" &&
      Protocol::command(a["command"].toString()).valid) {
    check(active.isEmpty() || jobs[active]["kind"] != "firmware",
          "DFU em andamento; não interrompa a gravação.");
    if (continuous.enabled)
      continuousStop("Parada de emergência solicitada pela GUI.");
    if (!active.isEmpty() && jobs[active]["kind"] != "firmware")
      finish(false, "Interrompido pela GUI.");
    guiMotors.clear();
    guiMotorKeys.clear();
    const quint64 receipt = guiSerial;
    check(s.enqueue(a["command"].toString()), "Parada recusada pela sessão.");
    return {{"after_serial", double(receipt)}};
  }
  check(active.isEmpty() && (guiOperation.isEmpty() || guiOperation == client),
        "Outra operação usa o controlador.");
  if (action == "stage") {
    const auto suffix = a["suffix"].toString();
    const auto value = a["raw"].toDouble();
    check(a["raw"].isDouble() && std::isfinite(value) &&
              value == std::floor(value),
          "Valor inválido.");
    const auto params = w.parameters();
    for (int row = 0; row < params.size(); ++row)
      if (params[row].suffix == suffix) {
        const auto &p = params[row];
        check(p.editable && value >= p.spec->minimum &&
                  value <= p.spec->maximum &&
                  Catalog::validValue(*p.spec, qint64(value), p.channel),
              "Parâmetro fora dos limites.");
        w.stageParameter(row, qint64(value));
        return {};
      }
    check(false, "Parâmetro desconhecido.");
  }
  if (action == "script") {
    check(a["source"].isString() && a["source"].toString().size() <= 262144,
          "Script inválido.");
    w.setScript(a["source"].toString());
    return {};
  }
  if (action == "connect") {
    requireIdle();
    check(!s.connected(),
          "Já conectado; desconecte explicitamente para trocar a fonte.");
    const auto mode = a["mode"].toString();
    check(mode == "demo" || mode == "serial", "Fonte inválida.");
    check(w.connectDevice(mode, a["port"].toString(), a["baud"].toInt(115200)),
          w.noticeText());
    guiReadings.clear();
    return {};
  }
  if (action == "disconnect") {
    requireIdle();
    s.close();
    guiReadings.clear();
    return {};
  }
  if (action == "writes" || action == "motion") {
    check(guiMotors.isEmpty() ||
              (guiMotors.size() == guiMotors.keys(client).size()),
          "Outra janela mantém movimento.");
    check(action == "writes" ? s.setWritesEnabled(a["enabled"].toBool())
                             : s.setMotionEnabled(a["enabled"].toBool()),
          "Permissão recusada pelo estado do controlador.");
    return {};
  }
  if (action == "cancel_motion") {
    check(guiMotors.isEmpty() ||
              guiMotors.size() == guiMotors.keys(client).size(),
          "Outra janela mantém movimento.");
    s.cancelMotion();
    return {};
  }
  if (action == "stream") {
    check(guiMotors.isEmpty(), "Pare os sliders antes de mudar a captura.");
    if (a["enabled"].toBool())
      check(s.startStream(a["interval"].toInt()), "Captura recusada.");
    else
      s.stopStream();
    return {};
  }
  if (action == "upload") {
    check(guiOperation == client, "Transferência requer operação exclusiva.");
    const auto image = HexFile::parse(a["hex"].toString().toUtf8());
    check(image.valid() && s.uploadScript(image),
          "HEX inválido ou transferência recusada.");
    return {};
  }
  if (action == "enqueue") {
    const auto command = a["command"].toString();
    const auto c = Protocol::command(command);
    check(c.valid && !c.hex, "Comando inválido.");
    const bool autonomous =
        (c.key == "R" && c.motion) || (c.key == "CLMOD" && c.motion);
    if (autonomous)
      check(guiMotors.isEmpty() ||
                (guiMotors.size() == 1 && guiMotors.value(0) == client),
            "Pare os sliders antes de executar script/ajuste.");
    if (c.motion && !autonomous && c.key != "MG")
      check(!guiMotors.contains(0) || guiMotors.value(0) == client,
            "Outra janela executa script/ajuste.");
    if (c.channel > 0 && (c.motion || c.key == "MS"))
      check(!guiMotors.contains(c.channel) || guiMotors[c.channel] == client,
            "Canal mantido por outra janela.");
    if ((c.key == "R" || c.key == "CLMOD") && guiMotors.contains(0))
      check(guiMotors.value(0) == client,
            "Script/ajuste pertence a outra janela; use emergência para "
            "interromper.");
    if (c.write && !c.motion && c.key != "MS")
      check(guiOperation == client || guiMotors.isEmpty() ||
                ((c.key == "R" || c.key == "CLMOD") &&
                 guiMotors.value(0) == client),
            "Pare os sliders antes de alterar o controlador.");
    if (guiOperation == client && c.write && !c.emergency)
      guiClients[client].operationWrites = true;
    const quint64 receipt = guiSerial;
    check(s.enqueue(command), "Comando recusado pelo estado da sessão: " +
                                  Protocol::redacted(command));
    if (autonomous) {
      guiAutonomousReady = false;
      guiMotors[0] = client;
      guiMotorKeys[0] = c.key;
    }
    if ((c.key == "R" || c.key == "CLMOD") && c.emergency &&
        guiMotors.value(0) == client) {
      guiMotors.remove(0);
      guiMotorKeys.remove(0);
    }
    if (c.channel > 0 && c.motion &&
        (c.key == "G" || c.key == "S" || c.key == "GIQ" || c.key == "GID" ||
         c.key == "P" || c.key == "PR")) {
      if (c.key == "P" || c.key == "PR" || c.value != 0) {
        guiMotors[c.channel] = client;
        guiMotorKeys[c.channel] = c.key == "PR" ? "P" : c.key;
      } else {
        guiMotors.remove(c.channel);
        guiMotorKeys.remove(c.channel);
      }
    }
    if (c.key == "MS") {
      guiMotors.remove(c.channel);
      guiMotorKeys.remove(c.channel);
    }
    return {{"after_serial", double(receipt)}};
  }
  check(false, "Ação gráfica desconhecida.");
  return {};
}
