#include "remote_session.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QThread>
#include <QUuid>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
RemoteSession::RemoteSession(const QString &socketPath, QObject *parent)
    : Session(parent), path(socketPath),
      client(QUuid::createUuid().toString(QUuid::WithoutBraces)) {
  poll.setInterval(100);
  connect(&poll, &QTimer::timeout, this, &RemoteSession::sync);
}
RemoteSession::~RemoteSession() { detach(); }
QJsonObject RemoteSession::backendCall(const QString &method,
                                       const QJsonObject &args) {
  QLocalSocket socket;
  socket.connectToServer(path);
  if (!socket.waitForConnected(500))
    return {{"ok", false},
            {"error", "motorlabd indisponível: " + socket.errorString()},
            {"transport_error", true},
            {"endpoint_absent",
             socket.error() == QLocalSocket::ServerNotFoundError ||
                 socket.error() == QLocalSocket::ConnectionRefusedError}};
  socket.write(
      QJsonDocument(QJsonObject{{"method", method}, {"arguments", args}})
          .toJson(QJsonDocument::Compact) +
      '\n');
  if (!socket.waitForBytesWritten(500))
    return {{"ok", false},
            {"error", "Falha ao enviar para motorlabd."},
            {"transport_error", true}};
  QByteArray data;
  QElapsedTimer time;
  time.start();
  while (!data.contains('\n') && time.elapsed() < 1500) {
    if (socket.bytesAvailable() ||
        socket.waitForReadyRead(qMax(1, 1500 - int(time.elapsed()))))
      data += socket.readAll();
    else
      break;
    if (data.size() > 2 * 1024 * 1024)
      break;
  }
  const auto reply =
      QJsonDocument::fromJson(data.left(data.indexOf('\n'))).object();
  return reply.contains("ok")
             ? reply
             : QJsonObject{{"ok", false},
                           {"error", "Resposta inválida do motorlabd."},
                           {"transport_error", true}};
}
bool RemoteSession::attach(bool launch) {
  auto result =
      backendCall("gui_session", {{"action", "attach"}, {"client", client}});
  if (!result["ok"].toBool() && launch && result["endpoint_absent"].toBool()) {
    // Share the Python launcher's flock. Never replace a live endpoint.
    const auto dir = QFileInfo(path).absolutePath();
    if (!QFileInfo::exists(dir)) {
      QDir().mkpath(dir);
      QFile::setPermissions(dir, QFile::ReadOwner | QFile::WriteOwner |
                                     QFile::ExeOwner);
    }
    struct stat info{};
    if (::lstat(QFile::encodeName(dir).constData(), &info) ||
        !S_ISDIR(info.st_mode) || info.st_uid != getuid() ||
        (info.st_mode & 0077)) {
      lastError =
          "Diretório do daemon deve pertencer ao usuário e ter modo 0700.";
      return false;
    }
    const int lock =
        ::open(QFile::encodeName(dir + "/startup.lock").constData(),
               O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (lock >= 0 && ::flock(lock, LOCK_EX | LOCK_NB) == 0) {
      const auto status = backendCall("motorlab_status", {});
      if (!status["ok"].toBool() && status["endpoint_absent"].toBool()) {
        struct stat endpoint{};
        if (::lstat(QFile::encodeName(path).constData(), &endpoint) == 0 &&
            (!S_ISSOCK(endpoint.st_mode) || endpoint.st_uid != getuid())) {
          ::flock(lock, LOCK_UN);
          ::close(lock);
          lastError = "Destino do socket não é um socket próprio.";
          return false;
        }
        QLocalServer::removeServer(path);
        const auto binary =
            QCoreApplication::applicationDirPath() + "/motorlabd";
        QProcess::startDetached(binary, {"--control-socket", path});
        for (int n = 0; n < 40; ++n) {
          QThread::msleep(25);
          result = backendCall("gui_session",
                               {{"action", "attach"}, {"client", client}});
          if (result["ok"].toBool())
            break;
        }
      }
      ::flock(lock, LOCK_UN);
    }
    if (lock >= 0)
      ::close(lock);
    for (int n = 0; n < 40 && !result["ok"].toBool(); ++n) {
      result = backendCall("gui_session",
                           {{"action", "attach"}, {"client", client}});
      if (!result["endpoint_absent"].toBool())
        break;
      QThread::msleep(25);
    }
  }
  if (!result["ok"].toBool()) {
    lastError =
        result["error"].toString() +
        " Atualize/reinicie o daemon desconectado antes de abrir a GUI.";
    return false;
  }
  if (result["result"].toObject()["gui_api_version"].toInt() != 1) {
    lastError = "Versão de motorlabd incompatível.";
    return false;
  }
  attached = true;
  replyAfter.clear();
  operation = false;
  accept(result["result"].toObject(), true);
  poll.start();
  return true;
}
void RemoteSession::accept(const QJsonObject &value, bool initial) {
  if (value["gui_api_version"].toInt() != 1) {
    lost("Versão de backend incompatível.");
    return;
  }
  QJsonObject oldState = snapshot, newState = value;
  for (const auto &key : {"parameters", "script", "events", "observations",
                          "telemetry", "serial"}) {
    oldState.remove(key);
    newState.remove(key);
  }
  const bool stateChanged =
      initial || oldState != newState || value.contains("parameters");
  snapshot = value;
  serial = quint64(value["serial"].toDouble());
  if (operation && !value["client_operation"].toBool()) {
    operation = false;
    emit failed(
        "Exclusividade revogada no daemon; operação da janela interrompida.");
  }
  emit backendSnapshot(value);
  if (initial) {
    const auto observations = value["observations"].toObject();
    for (auto it = observations.begin(); it != observations.end(); ++it)
      if (it.key().startsWith('~') || it.key() == "?FF") {
        const auto e = it.value().toObject();
        if (it.key() == "?FF" &&
            QDateTime::fromString(e["timestamp"].toString(), Qt::ISODateWithMs)
                    .msecsTo(QDateTime::currentDateTimeUtc()) >= 1000)
          continue;
        emit received(it.key(), e["payload"].toString(), e["ok"].toBool());
      }
  }
  for (const auto &v : value["events"].toArray()) {
    const auto e = v.toObject();
    const auto kind = e["kind"].toString();
    if (kind == "received") {
      const auto command = e["command"].toString();
      if (replyAfter.contains(command)) {
        if (quint64(e["serial"].toDouble()) <= replyAfter[command])
          continue;
        replyAfter.remove(command);
      }
      emit received(command, e["payload"].toString(), e["ok"].toBool());
    } else if (kind == "traffic")
      emit traffic(e["direction"].toString(), e["text"].toString());
    else if (kind == "failed")
      emit failed(e["message"].toString());
    else if (kind == "stream")
      emit streamFrame(e["payload"].toString());
    else if (kind == "upload_progress")
      emit uploadProgress(e["done"].toInt(), e["total"].toInt());
    else if (kind == "upload_finished")
      emit uploadFinished(e["ok"].toBool(), e["message"].toString());
  }
  if (stateChanged)
    emit changed();
}
void RemoteSession::lost(const QString &message) {
  poll.stop();
  attached = false;
  operation = false;
  snapshot = {};
  replyAfter.clear();
  emit changed();
  emit failed(message + " A janela não retoma comandos automaticamente.");
}
void RemoteSession::sync() {
  if (!attached || syncing)
    return;
  syncing = true;
  auto r = backendCall("gui_session", {{"action", "sync"},
                                       {"client", client},
                                       {"since", double(serial)},
                                       {"full", refreshDrafts}});
  refreshDrafts = false;
  if (r["ok"].toBool())
    accept(r["result"].toObject());
  else
    lost(r["error"].toString());
  syncing = false;
}
bool RemoteSession::action(const QString &name, QJsonObject args) {
  if (!attached) {
    emit failed("Janela sem conexão com motorlabd.");
    return false;
  }
  args["action"] = name;
  args["client"] = client;
  const auto r = backendCall("gui_session", args);
  if (!r["ok"].toBool()) {
    lastError = r["error"].toString();
    if (r["transport_error"].toBool())
      lost(lastError);
    else
      emit failed(lastError);
    return false;
  }
  lastReceipt = quint64(r["result"].toObject()["after_serial"].toDouble());
  return true;
}
bool RemoteSession::openSerial(const QString &port, int baud) {
  const auto ok =
      action("connect", {{"mode", "serial"}, {"port", port}, {"baud", baud}});
  if (ok)
    sync();
  return ok;
}
void RemoteSession::openDemo() {
  if (action("connect", {{"mode", "demo"}}))
    sync();
}
void RemoteSession::close() {
  if (action("disconnect"))
    sync();
}
bool RemoteSession::enqueue(const QString &command) {
  if (!action("enqueue", {{"command", command}}))
    return false;
  replyAfter[command] = lastReceipt;
  return true;
}
bool RemoteSession::setWritesEnabled(bool enabled) {
  const auto ok = action("writes", {{"enabled", enabled}});
  if (ok) {
    snapshot["writes_enabled"] = enabled;
    if (!enabled)
      snapshot["motion_enabled"] = false;
    emit changed();
  }
  return ok;
}
bool RemoteSession::setMotionEnabled(bool enabled) {
  const auto ok = action("motion", {{"enabled", enabled}});
  if (ok) {
    snapshot["motion_enabled"] = enabled;
    emit changed();
  }
  return ok;
}
void RemoteSession::cancelMotion() { action("cancel_motion"); }
bool RemoteSession::startStream(int interval) {
  const auto ok = action("stream", {{"enabled", true}, {"interval", interval}});
  if (ok)
    snapshot["streaming"] = true;
  return ok;
}
void RemoteSession::stopStream() {
  if (streaming() && action("stream", {{"enabled", false}}))
    snapshot["streaming"] = false;
}
bool RemoteSession::uploadScript(const HexImage &image) {
  return action("upload", {{"hex", image.records.join('\n') + '\n'}});
}
bool RemoteSession::prepareFirmwareUpdate(const QString &, const QString &) {
  emit failed("Atualização USB deve ser executada pelo job do motorlabd.");
  return false;
}
bool RemoteSession::enterDfu(const QString &, const QString &) { return false; }
void RemoteSession::detach() {
  poll.stop();
  if (attached)
    action("detach");
  attached = false;
  operation = false;
  snapshot = {};
}
bool RemoteSession::setOperationActive(bool enabled) {
  if (operation == enabled)
    return true;
  if (!action("operation", {{"enabled", enabled}}))
    return false;
  operation = enabled;
  return true;
}
bool RemoteSession::stageShared(const QString &suffix, qint64 raw) {
  const bool ok = action("stage", {{"suffix", suffix}, {"raw", double(raw)}});
  if (!ok)
    refreshDrafts = true;
  return ok;
}
void RemoteSession::setSharedScript(const QString &source) {
  if (attached)
    action("script", {{"source", source}});
}
