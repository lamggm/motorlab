#include "headless_host.h"
#include "cpr.h"
#include "firmware.h"
#include "firmware_job.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <cmath>
#include <stdexcept>

namespace {
QString utc() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
bool save(const QString &path, const QByteArray &bytes) {
  QSaveFile f(path); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size() && f.commit();
}
QVector<qint64> numbers(const QString &key, const QString &text) {
  const auto r = Protocol::reply(key + "=" + text); return r.numeric ? r.numbers : QVector<qint64>{};
}
}
HeadlessHost::HeadlessHost(Session &s) : ControlHost(s) {
  QVector<const ConfigSpec *> specs;
  for (const auto &key : {"ALIM", "MXRPM"}) if (auto spec = Catalog::find(key)) specs.append(spec);
  for (const auto &spec : Catalog::all()) if (spec.key != "ALIM" && spec.key != "MXRPM") specs.append(&spec);
  for (const auto spec : specs)
    for (int ch = spec->indexMax ? 1 : 0; ch <= spec->indexMax; ++ch)
      params.append({spec->key, spec->name, spec->indexUnits.value(QString::number(ch)).toString(spec->unit),
                     Catalog::suffix(*spec, ch), spec, ch, spec->scale, 0, 0, false, spec->editable, false});
  connect(&session, &Session::received, this, &HeadlessHost::onReply);
  connect(&session, &Session::traffic, this, &HeadlessHost::writeLog);
  connect(&session, &Session::failed, this, [this](const QString &text) { notify(text); });
  connect(&session, &Session::streamFrame, this, [this](const QString &payload) {
    const auto v = numbers("ML", payload);
    if (v.size() != 12) { session.stopStream(); notify("Frame de streaming inválido; captura encerrada."); return; }
    readings.clear();
    readings["?A"] = QString("%1:%2").arg(v[0]).arg(v[1]);
    readings["?S"] = QString("%1:%2").arg(v[2]).arg(v[3]);
    readings["?V 2"] = QString::number(v[4]); readings["?T 2"] = QString::number(v[5]);
    readings["?C"] = QString("%1:%2").arg(v[6]).arg(v[7]);
    readings["?DI"] = QString::number(v[8]); readings["?DO"] = QString::number(v[9]);
    readings["?FF"] = QString::number(v[10]); readings["?FS"] = QString::number(v[11]); recordFrame();
  });
  poll.setInterval(400);
  connect(&poll, &QTimer::timeout, this, [this] {
    if (!session.connected() || applying || session.busy() || session.streaming() || session.uploading()) return;
    readings.clear(); scan = true;
    for (const auto &command : Protocol::telemetryCommands())
      if (!session.enqueue(command)) { scan = false; break; }
  });
  poll.start();
}
void HeadlessHost::stageParameter(int row, qint64 raw) {
  auto &p = params[row]; p.draftRaw = raw; p.dirty = !p.known || p.raw != raw; updateUnits();
}
void HeadlessHost::clearParameter(int row) { params[row].known = false; params[row].dirty = false; }
void HeadlessHost::writeLog(const QString &direction, const QString &text) {
  log.append(utc() + " " + direction + " " + text); while (log.size() > 2000) log.removeFirst();
}
void HeadlessHost::setProfileMetadata(const QString &m, const QString &f, const QString &s) {
  profileModel = m; firmware = f; source = s;
}
bool HeadlessHost::connectDevice(const QString &mode, const QString &port, int baud) {
  samples = {}; readings.clear(); log.clear(); scan = false; elapsed.start();
  source = captureSource = mode == "demo" ? "demo" : "serial";
  profileModel.clear(); firmware.clear(); captureModel.clear(); captureFirmware.clear();
  for (int row = 0; row < params.size(); ++row) clearParameter(row);
  if (mode == "demo") session.openDemo(); else if (!session.openSerial(port, baud)) return false;
  notify(mode == "demo" ? "Demonstração: dados simulados." : "Consultando modelo/firmware; permissões desabilitadas.");
  return session.connected();
}
QJsonObject HeadlessHost::view(const QJsonObject &a) {
  if (a.contains("screenshot_path")) throw std::runtime_error("CLI não tem tela para capturar. Use telemetry/profile para exportar dados.");
  if (a["visible"].toBool()) throw std::runtime_error("Backend CLI não possui janela.");
  if (a.contains("model")) model = a["model"].toString();
  return {{"visible", false}, {"page", a["page"].toInt(0)}, {"backend", "cli"}, {"model", model}};
}
void HeadlessHost::onReply(const QString &command, const QString &payload, bool ok) {
  if (command == "?FID" && ok) firmware = captureFirmware = payload;
  if (command == "?TRN" && ok) {
    profileModel = Protocol::supportedModel(payload); captureModel = payload;
    if(ok) {
      session.enqueue("~RWD");
      for(int ch=1;ch<=2;++ch) for(const auto &key:{"MMOD","MVEL","MXRPM"}) session.enqueue(QString("~%1 %2").arg(key).arg(ch));
    }
    if (!profileModel.isEmpty()) model = profileModel;
  }
  if (command.startsWith('~')) {
    const auto key = command.mid(1).section(' ', 0, 0); const auto v = numbers(key, payload);
    for (auto &p : params) if (command == "~" + p.suffix) {
      p.known = ok && v.size() == 1 && v[0] >= p.spec->minimum && v[0] <= p.spec->maximum;
      if (p.known) { p.raw = p.draftRaw = v[0]; p.dirty = false; }
      break;
    }
    updateUnits();
  }
  if (scan && !applying && Protocol::telemetryCommands().contains(command)) {
    if (ok) readings[command] = payload; else readings.remove(command);
    if (command == "?FS") { scan = false; recordFrame(); }
  }
}
void HeadlessHost::updateUnits() {
  auto mode = [this](const QString &key, int channel) {
    for (const auto &p : params) if (p.key == key && p.channel == channel && p.known && !p.dirty) return int(p.raw);
    return -1;
  };
  for (auto &p : params) {
    p.unit = p.spec->indexUnits.value(QString::number(p.channel)).toString(p.spec->unit);
    if (p.key == "FEW") {
      const auto m = mode("MMOD", p.channel);
      if (m == 1) p.unit = "RPM"; else if (m == 3 || m == 4) p.unit = "contagens";
    } else if ((p.key == "MAC" || p.key == "MDEC" || p.key == "EDEC") && mode("MMOD", p.channel) == 5) p.unit = "mA/s";
    else if (p.key == "PCTR" || p.key == "PMIN" || p.key == "PMAX") {
      switch (mode("PMOD", p.channel)) { case 1: p.unit = "µs"; break; case 2: p.unit = "Hz"; break; case 3: p.unit = "1/4095 ciclo"; break; case 6: p.unit = "pulsos"; break; }
    }
  }
}
void HeadlessHost::recordFrame() {
  QJsonObject sample{{"timestamp", utc()}, {"seconds", elapsed.elapsed()/1000.0}};
  auto pair = [&](const QString &command, const QString &one, const QString &two, double scale) {
    auto v = numbers(command.mid(1).section(' ',0,0), readings.value(command));
    sample[one] = v.size() == 2 ? QJsonValue(v[0]/scale) : QJsonValue();
    sample[two] = v.size() == 2 ? QJsonValue(v[1]/scale) : QJsonValue();
  };
  auto scalar = [&](const QString &command, const QString &key, double scale) {
    auto v = numbers(command.mid(1).section(' ',0,0), readings.value(command));
    sample[key] = v.size() == 1 ? QJsonValue(v[0]/scale) : QJsonValue();
  };
  pair("?A", "amps1", "amps2", 10); pair("?S", "rpm1", "rpm2", 1); pair("?C", "encoder1", "encoder2", 1);
  scalar("?V 2","voltage",10); scalar("?T 2","temperature",1);
  scalar("?BS 1","rpm1_hall",1); scalar("?BS 2","rpm2_hall",1);
  scalar("?FF","faults",1); scalar("?FS","flags",1);
  sample["rpm1_encoder"] = sample["rpm1"]; sample["rpm2_encoder"] = sample["rpm2"];
  QJsonObject fields; for(auto it=readings.begin();it!=readings.end();++it) fields[it.key()]=it.value();
  sample["readings"]=fields;
  samples.append(sample); if (samples.size() > 36000) samples.removeFirst();
}
QJsonObject HeadlessHost::telemetry(int limit) const {
  QJsonArray last; for (int i = qMax(0,int(samples.size())-limit); i < samples.size(); ++i) last.append(samples[i]);
  return {{"samples",last},{"connected",session.connected()},{"source",captureSource},{"model",captureModel},{"firmware",captureFirmware}};
}
void HeadlessHost::continuousUpdate(const QJsonObject &s, int, qint64) {
  QJsonObject sample = s;
  sample["seconds"] = elapsed.elapsed()/1000.0;
  for (int ch = 1; ch <= 2; ++ch) {
    const auto n = QString::number(ch);
    sample["rpm"+n+"_hall"] = s["rpm"+n]; sample["rpm"+n] = s["encoder_rpm"+n];
    sample["rpm"+n+"_encoder"] = s["encoder_rpm"+n]; sample["encoder"+n] = s["encoder_counter"+n];
  }
  samples.append(sample); if (samples.size() > 36000) samples.removeFirst();
}
bool HeadlessHost::saveProfile(const QString &path) {
  if (applying) return false;
  QJsonArray values; int known = 0;
  for (const auto &p : params) {
    if (p.known) ++known;
    if (p.known || p.dirty) values.append(QJsonObject{{"query","~"+p.suffix},{"raw",double(p.dirty?p.draftRaw:p.raw)},
      {"unit",p.unit},{"scale",p.scale},{"draft",p.dirty},{"readable",p.known}});
  }
  if (values.isEmpty()) return false;
  const auto m = session.connected() ? Protocol::supportedModel(session.model()) : profileModel.isEmpty()?model:profileModel;
  if (m.isEmpty()) return false;
  QJsonObject obj{{"application","MotorLab Linux"},{"version",QCoreApplication::applicationVersion()},{"schema",1},
    {"scope","catalog_configuration_profile"},{"source",source},{"model",m},{"firmware",session.connected()?session.firmware():firmware},
    {"session_connected",session.connected()},{"timestamp_utc",utc()},{"catalog_fields",params.size()},
    {"known_fields",known},{"complete_catalog_read",known==params.size()},{"parameters",values}};
  return save(path,QJsonDocument(obj).toJson());
}
bool HeadlessHost::saveCsv(const QString &path) {
  if (samples.isEmpty()) return false;
  QStringList keys{"timestamp","seconds","amps1","amps2","rpm1_encoder","rpm2_encoder","voltage","temperature","encoder1","encoder2","faults","flags","rpm1_hall","rpm2_hall"};
  QByteArray data = "timestamp_utc,seconds,source,amp1_rms_A,amp2_rms_A,rpm1_encoder,rpm2_encoder,battery_V,heatsink1_C,encoder1,encoder2,faults_FF,status_FS,rpm1_hall,rpm2_hall\n";
  for (auto v : samples) {
    auto s = v.toObject(); QStringList row;
    for (const auto &key : keys) {
      row.append(s[key].isString()?s[key].toString():s[key].isDouble()?QString::number(s[key].toDouble(),'f',3):QString());
      if (key == "seconds") row.append(captureSource);
    }
    data += row.join(',').toUtf8() + '\n';
  }
  return save(path,data);
}
void HeadlessHost::startFirmware(const QString &path, const QString &backup) {
  auto job = new FirmwareJob(session, this);
  poll.stop(); applying = true; firmwareActive = true;
  connect(job, &FirmwareJob::finished, this, [this,job](bool ok,const QString &text) {
    applying = false; firmwareActive = false; notify(text); writeLog("DFU",text); poll.start(); emit firmwareUpdateFinished(ok,text); job->deleteLater();
  });
  const auto snapshot = parameters();
  bool dirty = false, known = false;
  for (const auto &p : snapshot) { dirty |= p.dirty; known |= p.known; }
  applying = false;
  if (dirty || !known || !saveProfile(backup)) {
    notify("Leia a configuração sem rascunhos e salve o backup antes do DFU.");
    firmwareActive = false; emit firmwareUpdateFinished(false,notice); job->deleteLater(); poll.start(); return;
  }
  applying = true; job->start(path);
}

bool HeadlessHost::loadProfile(const QString &path, QString *error) {
  auto bad = [&](const QString &message) {
    if (error)
      *error = message;
    return false;
  };
  if (applying || session.streaming())
    return bad("Encerre a operação atual antes de importar.");
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
    return bad("Perfil inacessível ou maior que 1 MiB.");
  QJsonParseError parse;
  const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
  if (parse.error != QJsonParseError::NoError || !doc.isObject())
    return bad("JSON inválido.");
  const auto obj = doc.object();
  const auto scope = obj["scope"].toString();
  if (obj["application"].toString() != "MotorLab Linux" ||
      (scope != "catalog_configuration_profile" &&
       scope != "partial_parameter_snapshot"))
    return bad("Formato de perfil não reconhecido.");
  if (scope == "catalog_configuration_profile" && obj["schema"].toInt() != 1)
    return bad("Versão de esquema não suportada.");
  const auto model = Protocol::supportedModel(obj["model"].toString());
  if (model.isEmpty())
    return bad("Modelo do perfil não suportado.");
  const QString expected = session.connected()
                               ? Protocol::supportedModel(session.model())
                               : this->model;
  if (expected != model)
    return bad("O perfil pertence a outro modelo. Selecione o modelo correto "
               "offline ou conecte a unidade correspondente.");
  if (session.connected() && !session.isDemo() &&
      obj["source"].toString() == "demo")
    return bad("Perfil simulado não pode ser aplicado a um controlador real.");
  if (session.connected() && !session.isDemo() &&
      !obj["firmware"].toString().isEmpty() &&
      obj["firmware"].toString() != session.firmware())
    return bad("Firmware do perfil diferente do firmware conectado.");
  if (!obj["parameters"].isArray() || obj["parameters"].toArray().isEmpty())
    return bad("Perfil sem parâmetros.");
  QVector<QPair<int, qint64>> staged;
  QSet<QString> seen;
  for (const auto &v : obj["parameters"].toArray()) {
    if (!v.isObject())
      return bad("Entrada de parâmetro inválida.");
    const auto entry = v.toObject();
    const auto query = entry["query"].toString();
    const auto cmd = Protocol::command(query);
    if (!cmd.valid || !query.startsWith('~') || seen.contains(query) ||
        !entry["raw"].isDouble())
      return bad("Consulta inválida ou duplicada no perfil.");
    seen.insert(query);
    const double d = entry["raw"].toDouble();
    if (!std::isfinite(d) || std::floor(d) != d || d < -2147483648.0 ||
        d > 4294967295.0)
      return bad("Valor bruto deve ser um inteiro de 32 bits.");
    int row = -1;
    for (int i = 0; i < params.size(); ++i)
      if ("~" + params[i].suffix == query) {
        row = i;
        break;
      }
    if (row < 0)
      return bad("Parâmetro desconhecido: " + query);
    const auto &p = params[row];
    const qint64 raw = qint64(d);
    if (raw < p.spec->minimum || raw > p.spec->maximum)
      return bad("Valor fora do limite para " + query);
    if (p.spec->editable && !Catalog::validValue(*p.spec, raw, p.channel))
      return bad("Combinação de índice e valor inválida.");
    staged.append({row, raw});
  }
  // Commit locally only after every entry passes. Import never sends a command.
  for (const auto &value : staged) {
    auto &p = params[value.first];
    if (!p.editable) continue;
    stageParameter(value.first,value.second);
  }
  profileModel = model;
  firmware = obj["firmware"].toString();
  source = obj["source"].toString();

  return true;
}


bool HeadlessHost::saveCpr(const QString &path) {
  if (applying)
    return false;
  const QString model = session.connected()
                            ? Protocol::supportedModel(session.model())
                            : this->model;
  const QString firmwareVersion =
      session.connected() ? session.firmware() : firmware;
  if (!firmwareVersion.isEmpty() &&
      !QRegularExpression("v3\\.0[a-z]?\\b").match(firmwareVersion).hasMatch())
    return false;
  QFile metadata(":/original-trees.json");
  if (!metadata.open(QIODevice::ReadOnly))
    return false;
  auto tree =
      QJsonDocument::fromJson(metadata.readAll()).object()[model].toObject();
  CprProfile profile;
  profile.firmware = "v3.0:" + model;
  profile.roborun = "MotorLab CLI " + QCoreApplication::applicationVersion() + " · perfil parcial";
  if (session.isDemo() || source == "demo")
    profile.roborun += " · DEMO";
  profile.controlTree = tree["controlTree"].toString();
  profile.powerTree = tree["powerTree"].toString();
  QSet<QString> seen;
  for (auto value : tree["entries"].toArray()) {
    auto entry = value.toObject();
    const auto key = entry["command"].toString();
    int index = entry["index"].toInt();
    for (const auto &p : params) {
      if (p.key != key || (p.channel ? p.channel : 1) != index ||
          (!p.known && !p.dirty))
        continue;
      CprItem item;
      item.command = key;
      item.index = index;
      if (seen.contains(item.identity()))
        break;
      if (entry["type"].toString() == "String")
        break;
      item.tree = entry["tree"].toString();
      item.type = entry["type"].toString();
      QStringList nodes;
      for (auto n : entry["path"].toArray())
        nodes.append(n.toString());
      item.node = nodes.join('|');
      item.multiplier = qRound(p.scale);
      item.value = QString::number(
          p.dirty ? p.draftRaw : p.raw);
      profile.items.append(item);
      seen.insert(item.identity());
      break;
    }
  }
  if (profile.items.isEmpty())
    return false;
  const auto data = profile.xml();
  QSaveFile f(path);
  return f.open(QIODevice::WriteOnly) && f.write(data) == data.size() &&
         f.commit();
}
