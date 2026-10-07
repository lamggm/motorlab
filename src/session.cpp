#include "session.h"
#include "catalog.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

Session::Session(QObject *parent) : QObject(parent) {
  timeout.setSingleShot(true);
  connect(&streamTimer, &QTimer::timeout, this, [this] {
    if (demo && stream)
      emit streamFrame("58:51:810:790:508:36:8400:8200:3:0:0:0");
  });
  connect(&timeout, &QTimer::timeout, this, [this] {
    fail("Tempo de resposta esgotado para " + Protocol::redacted(current.text) +
         ". Conexão encerrada; reconecte para consultar novamente.");
  });
}
Session::~Session() {
  blockSignals(true);
  close();
}

bool Session::openSerial(const QString &path, int baud) {
  close();
  speed_t speed = 0;
  switch (baud) {
  case 115200:
    speed = B115200;
    break;
  case 57600:
    speed = B57600;
    break;
  case 38400:
    speed = B38400;
    break;
  case 19200:
    speed = B19200;
    break;
  case 9600:
    speed = B9600;
    break;
  default:
    emit failed("Baud rate não suportado.");
    return false;
  }
  fd = ::open(QFile::encodeName(path).constData(),
              O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    emit failed("Não foi possível abrir " + path + ": " +
                QString::fromLocal8Bit(std::strerror(errno)));
    return false;
  }
  struct stat st{};
  if (fstat(fd, &st) != 0 || !S_ISCHR(st.st_mode)) {
    fail("Escolha um dispositivo serial, não um arquivo comum.");
    return false;
  }
  if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
    fail("A porta não pôde ser reservada para uso exclusivo.");
    return false;
  }
  ownLock = true;
  if (ioctl(fd, TIOCEXCL) != 0) {
    fail("A porta não pôde ser reservada para uso exclusivo.");
    return false;
  }
  ownExclusive = true;
  if (tcgetattr(fd, &original) != 0) {
    fail("Não foi possível consultar a configuração da porta.");
    return false;
  }
  restoreTermios = true;
  termios settings = original;
  cfmakeraw(&settings);
  settings.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS | HUPCL);
  settings.c_cflag |= CS8 | CLOCAL | CREAD;
  settings.c_cc[VMIN] = 0;
  settings.c_cc[VTIME] = 0;
  cfsetispeed(&settings, speed);
  cfsetospeed(&settings, speed);
  if (tcsetattr(fd, TCSANOW, &settings) != 0) {
    fail("Não foi possível configurar 8N1 na porta.");
    return false;
  }
  reader = new QSocketNotifier(fd, QSocketNotifier::Read, this);
  connect(reader, &QSocketNotifier::activated, this, [this] { readReady(); });
  serialPath = QFileInfo(path).canonicalFilePath();
  live = true;
  demo = false;
  emit changed();
  enqueue("?FID");
  enqueue("?TRN");
  return true;
}

void Session::openDemo() {
  close();
  demo = true;
  live = true;
  demoClock.start();
  for (const auto &spec : Catalog::all())
    for (int ch = spec.indexMax ? 1 : 0; ch <= spec.indexMax; ++ch)
      demoParams[Catalog::suffix(spec, ch)] = qMax<qint64>(spec.minimum, 0);
  for (int ch = 1; ch <= 2; ++ch) {
    demoParams[QString("ALIM %1").arg(ch)] = 200;
    demoParams[QString("MXRPM %1").arg(ch)] = 1500;
    demoParams[QString("MMOD %1").arg(ch)] = 1;
    demoParams[QString("KPG %1").arg(ch)] = 10;
    demoParams[QString("KIG %1").arg(ch)] = 2;
  }
  demoParams["RWD"] = 1000;
  demoDefaults = demoParams;
  demoSaved = demoParams;
  demoFaults = 0;
  emit changed();
  enqueue("?FID");
  enqueue("?TRN");
}

void Session::close() {
  // Last best-effort stop before relinquishing the port. No success is claimed
  // without a reply.
  if (motion && fd >= 0) {
    const QByteArray stop("!EX\r");
    ::write(fd, stop.constData(), size_t(stop.size()));
  }
  const bool interruptedUpload = upload;
  upload = false;
  uploadRecords.clear();
  uploadIndex = -1;
  stream = false;
  streamTimer.stop();
  motion = false;
  watchdog = -1;
  firmwareAuthorized = false;
  scriptPaused = false;
  confirmedFaults = 0;
  stopStamp.invalidate();
  ++generation;
  timeout.stop();
  pending.clear();
  active = false;
  completing = false;
  writes = false;
  if (reader) {
    reader->setEnabled(false);
    reader->deleteLater();
    reader = nullptr;
  }
  if (fd >= 0) {
    if (restoreTermios)
      tcsetattr(fd, TCSANOW, &original);
    if (ownExclusive)
      ioctl(fd, TIOCNXCL);
    if (ownLock)
      flock(fd, LOCK_UN);
    ::close(fd);
    fd = -1;
  }
  restoreTermios = false;
  ownExclusive = false;
  ownLock = false;
  live = false;
  demo = false;
  input.clear();
  fid.clear();
  trn.clear();
  emit changed();
  if (interruptedUpload)
    emit uploadFinished(false, "Transferência interrompida; releia o estado "
                               "antes de executar scripts.");
}

bool Session::setWritesEnabled(bool enabled) {
  if (enabled && (!live || (!demo && !Protocol::compatible(fid, trn))))
    return false;
  // Authorization revocation clears pending mutations.
  const bool wasMotion = motion;
  writes = enabled;
  if (!writes) {
    motion = false;
    QQueue<Protocol::Command> keep;
    for (const auto &c : pending)
      if (!c.write)
        keep.enqueue(c);
    pending = keep;
    if (wasMotion && live)
      enqueue("!EX");
  }
  emit changed();
  return true;
}

bool Session::enqueue(const QString &text) {
  const auto c = Protocol::command(text);
  const bool identified = demo || Protocol::compatible(fid, trn) ||
                          (firmwareAuthorized && c.emergency);
  if (!live || !c.valid || (c.write && !identified) ||
      (c.write && !c.emergency && !writes) || (c.motion && !motion) ||
      (c.hex && !upload) || (c.key == "SLD" && !upload) || pending.size() >= 64)
    return false;
  if (upload && !c.hex && c.key != "SLD" && !c.emergency)
    return false;
  if (c.emergency) {
    if (c.key == "EX" && active && !demo) {
      cancelMotion();
      const bool sent = sendRaw(c.text);
      close();
      emit traffic(
          "TX",
          c.text + " [parada imediata durante operação; sem ACK confirmado]");
      emit failed(
          sent
              ? "Parada enviada durante outra operação. Conexão encerrada para "
                "descartar respostas ambíguas; reconecte e confira ?FF."
              : "Não foi possível transmitir a parada; confira o controlador.");
      return sent;
    }
    if (upload) {
      upload = false;
      uploadRecords.clear();
      emit uploadFinished(false, "Transferência cancelada pela parada.");
    }
    cancelMotion();
    pending.prepend(c);
    startNext();
    return true;
  }
  pending.enqueue(c);
  startNext();
  return true;
}

void Session::startNext() {
  if (!live || active || completing || pending.isEmpty())
    return;
  current = pending.dequeue();
  if ((current.write && !current.emergency && !writes) ||
      (current.motion && !motion)) {
    startNext();
    return;
  }
  active = true;
  emit traffic("TX", Protocol::redacted(current.text));
  timeout.start(timeoutMs);
  if (demo) {
    const auto gen = generation;
    const auto cmd = current;
    QTimer::singleShot(18, this, [this, gen, cmd] {
      if (gen == generation && active)
        line(demoResponse(cmd).toLatin1());
    });
  } else {
    sendRaw(current.text);
  }
}

void Session::readReady() {
  char buffer[512];
  while (fd >= 0) {
    const auto n = ::read(fd, buffer, sizeof buffer);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;
      fail("A porta serial foi desconectada ou apresentou erro.");
      return;
    }
    if (n == 0) {
      pollfd descriptor{fd, POLLIN, 0};
      if (::poll(&descriptor, 1, 0) > 0 &&
          (descriptor.revents & (POLLHUP | POLLERR | POLLNVAL))) {
        fail("A porta serial foi desconectada.");
        return;
      }
      break;
    }
    for (int i = 0; i < n; ++i) {
      const char ch = buffer[i];
      if (ch == '\r' || ch == '\n') {
        if (!input.isEmpty()) {
          const auto value = input;
          input.clear();
          line(value);
        }
      } else {
        input.append(ch);
        if (input.size() > 1024) {
          fail("Resposta serial excedeu o limite de tamanho.");
          return;
        }
      }
    }
  }
}

void Session::line(const QByteArray &value) {
  const QString text = QString::fromLatin1(value);
  emit traffic("RX", Protocol::redacted(text));
  if (stream && text.startsWith("ML=")) {
    emit streamFrame(text.mid(3));
    return;
  }
  if (!active)
    return;
  if (current.key == "SLD" && text.trimmed() == "HLD") {
    finish("HLD", true);
    return;
  }
  const auto r = Protocol::reply(text);
  if (r.kind == Protocol::Reply::Error) {
    finish("Controlador rejeitou a consulta/operação.", false);
    return;
  }
  if (current.write && r.kind == Protocol::Reply::Ack) {
    finish("+", true);
    return;
  }
  if (!current.write && r.kind == Protocol::Reply::Value &&
      r.key == current.key) {
    if (r.key != "FID" && r.key != "TRN" && !r.numeric) {
      fail("Resposta numérica inválida para " + current.text + ".");
      return;
    }
    const bool identityChanged =
        (r.key == "FID" && !fid.isEmpty() && fid != r.payload) ||
        (r.key == "TRN" && !trn.isEmpty() && trn != r.payload);
    if (r.key == "FID")
      fid = r.payload;
    if (r.key == "TRN")
      trn = r.payload;
    // Authorization belongs to this identity, even when both models are
    // supported.
    if (identityChanged)
      firmwareAuthorized = false;
    if (writes && !demo && (identityChanged || !Protocol::compatible(fid, trn)))
      setWritesEnabled(false);
    finish(r.payload, true);
  }
}

void Session::finish(const QString &payload, bool ok) {
  const QString cmd = current.text;
  if (cmd == "!R 0")
    scriptPaused = ok;
  if (cmd == "?FF") {
    confirmedFaults = 0;
    stopStamp.invalidate();
    const auto r = Protocol::reply("FF=" + payload);
    if (ok && r.numeric && r.numbers.size() == 1 && r.numbers[0] >= 0 &&
        r.numbers[0] <= 65535) {
      confirmedFaults = quint64(r.numbers[0]);
      stopStamp.start();
    }
  }
  if (Protocol::command(cmd).motion) {
    confirmedFaults = 0;
    stopStamp.invalidate();
    scriptPaused = false;
  }
  if (cmd.startsWith("^RWD"))
    watchdog = -1;
  if (cmd == "~RWD" && ok) {
    const auto r = Protocol::reply("RWD=" + payload);
    if (r.numeric && r.numbers.size() == 1 && r.numbers[0] >= 0 &&
        r.numbers[0] <= 65535)
      watchdog = int(r.numbers[0]);
  }
  timeout.stop();
  active = false;
  completing = true;
  const auto gen = generation;
  if (upload && (current.key == "SLD" || current.hex)) {
    if (!ok) {
      upload = false;
      uploadRecords.clear();
      setWritesEnabled(false);
      emit uploadFinished(
          false,
          "Controlador rejeitou o HEX. Transferência encerrada sem executar.");
    } else {
      ++uploadIndex;
      emit uploadProgress(uploadIndex, uploadRecords.size());
    }
  }
  emit received(cmd, payload, ok);
  if (cmd == "?FID" || cmd == "?TRN")
    emit changed();
  // Wait one event-loop turn: don't assign residual unsolicited replies to the
  // next request.
  QTimer::singleShot(0, this, [this, gen] {
    if (gen == generation) {
      completing = false;
      if (upload)
        uploadNext();
      startNext();
    }
  });
}

void Session::fail(const QString &message) {
  close();
  emit failed(message);
}

QString Session::demoResponse(const Protocol::Command &c) {
  const double t = demoClock.elapsed() / 1000.0;
  if (c.key == "SLD")
    return "HLD";
  if (c.write) {
    if (c.text.startsWith('^'))
      demoParams[c.key + (c.channel ? " " + QString::number(c.channel)
                                    : QString())] = c.value;
    if (c.key == "EX")
      demoFaults |= 16;
    if (c.key == "MG")
      demoFaults &= ~quint64(16);
    if (c.key == "EESAV")
      demoSaved = demoParams;
    if (c.key == "EELD")
      demoParams = demoSaved;
    if (c.key == "EERST") {
      demoParams = demoDefaults;
      demoSaved = demoDefaults;
    }
    return "+";
  }
  if (c.text.startsWith('~'))
    return c.key + "=" +
           QString::number(demoParams.value(
               c.key +
               (c.channel ? " " + QString::number(c.channel) : QString())));
  if (c.key == "FID")
    return "FID=Roboteq v3.0f SBLMG2XXX 07/15/2026 (SIMULADO)";
  if (c.key == "TRN")
    return "TRN=RoboG4:SBLMG2360T";
  QStringList parts;
  if (c.key == "A")
    parts = {QString::number(int(58 + 22 * std::sin(t * .8))),
             QString::number(int(51 + 19 * std::sin(t * .8 + .6)))};
  if (c.key == "S")
    parts = {QString::number(int(810 + 140 * std::sin(t * .5))),
             QString::number(int(790 + 145 * std::sin(t * .5 + .35)))};
  if (c.key == "C")
    parts = {QString::number(qint64(t * 8400)),
             QString::number(qint64(t * 8200))};
  if (c.key == "V")
    parts = {"135", QString::number(int(508 + 3 * std::sin(t))), "4750"};
  if (c.key == "T")
    parts = {"43", QString::number(int(36 + 2 * std::sin(t / 10))), "37", "39",
             "40"};
  if (c.key == "FF")
    parts = {QString::number(demoFaults)};
  if (QSet<QString>{"FS", "DO", "FM", "LK"}.contains(c.key))
    parts = {"0"};
  if (c.key == "AI" || c.key == "PI")
    parts = QStringList(8, "2500");
  if (QSet<QString>{"MA", "MC", "P", "CB", "BS", "SNA", "ANG", "E", "M"}
          .contains(c.key))
    parts = {"0", "0", "0", "0"};
  if (c.key == "DI")
    parts = {"3"};
  if (parts.isEmpty())
    return "?";
  if (c.channel)
    return c.key + "=" + parts.value(c.channel - 1, "0");
  return c.key + "=" + parts.join(':');
}

bool Session::setMotionEnabled(bool enabled) {
  if (enabled && (!live || !writes || upload || (!demo && watchdog < 100) ||
                  (!demo && !Protocol::compatible(fid, trn))))
    return false;
  motion = enabled;
  if (!enabled)
    cancelMotion();
  emit changed();
  return true;
}
void Session::cancelMotion() {
  motion = false;
  QQueue<Protocol::Command> keep;
  for (const auto &c : pending)
    if (!c.write)
      keep.enqueue(c);
  pending = keep;
}
bool Session::sendRaw(const QString &text) {
  if (demo)
    return true;
  const QByteArray bytes = text.toLatin1() + '\r';
  if (fd < 0 ||
      ::write(fd, bytes.constData(), size_t(bytes.size())) != bytes.size()) {
    fail("Falha ao enviar quadro completo. Conexão encerrada.");
    return false;
  }
  return true;
}
bool Session::startStream(int intervalMs) {
  if (!live || busy() || upload || stream || intervalMs < 1 ||
      intervalMs > 2000 || (!demo && !Protocol::compatible(fid, trn)))
    return false;
  // Virtual USB CDC can stream at 1 ms; physical UART has a wire-rate limit.
  if (!demo && !serialPath.startsWith("/dev/ttyACM") && intervalMs < 20)
    return false;
  const QString packet = "/\"ML=\",\":\"?A_?S_?V 2_?T 2_?C_?DI_?DO_?FF_?FS_# " +
                         QString::number(intervalMs);
  emit traffic("TX", "#C");
  if (!sendRaw("#C"))
    return false;
  stream = true;
  emit traffic("TX", packet);
  if (!sendRaw(packet))
    return false;
  if (demo)
    streamTimer.start(intervalMs);
  emit changed();
  return true;
}
void Session::stopStream() {
  if (!stream)
    return;
  emit traffic("TX", "#");
  sendRaw("#");
  stream = false;
  streamTimer.stop();
  emit changed();
}
bool Session::uploadScript(const HexImage &image) {
  if (!image.valid() || !writes || !live || busy() || stream || upload ||
      motion || !scriptPaused || !(confirmedFaults & 16) ||
      !stopStamp.isValid() || stopStamp.elapsed() > 2000)
    return false;
  // Re-validate caller data; immutable sequence only after preflight.
  const auto checked = HexFile::parse(image.records.join('\n').toLatin1());
  if (!checked.valid() || checked.bytes != image.bytes)
    return false;
  confirmedFaults = 0;
  stopStamp.invalidate();
  scriptPaused = false;
  upload = true;
  uploadIndex = -1;
  uploadRecords = checked.records;
  if (!enqueue("%SLD")) {
    upload = false;
    uploadRecords.clear();
    return false;
  }
  return true;
}
void Session::uploadNext() {
  if (!upload)
    return;
  if (uploadIndex >= uploadRecords.size()) {
    upload = false;
    uploadRecords.clear();
    emit uploadFinished(
        true, "HEX transferido e confirmado; script permanece sem execução.");
    emit changed();
    return;
  }
  if (uploadIndex >= 0 && !enqueue(uploadRecords[uploadIndex])) {
    upload = false;
    uploadRecords.clear();
    setWritesEnabled(false);
    emit uploadFinished(false, "Transferência interrompida.");
  }
}

QString Session::usbPath() const {
  if (!live || demo || !serialPath.startsWith("/dev/ttyACM"))
    return {};
  QString path = QFileInfo("/sys/class/tty/" +
                           QFileInfo(serialPath).fileName() + "/device")
                     .canonicalFilePath();
  while (!path.isEmpty() && path != "/") {
    if (QFile::exists(path + "/idVendor") &&
        QFile::exists(path + "/idProduct")) {
      const auto name = QFileInfo(path).fileName();
      return QRegularExpression("^[0-9]+-[0-9]+(?:\\.[0-9]+)*$")
                     .match(name)
                     .hasMatch()
                 ? name
                 : QString();
    }
    path = QFileInfo(path).absolutePath();
  }
  return {};
}
bool Session::prepareFirmwareUpdate(const QString &model, const QString &path) {
  if (!live || demo || upload || Protocol::supportedModel(trn) != model ||
      path.isEmpty() || usbPath() != path ||
      !QRegularExpression("\\bv3\\.0[a-f]?(?=\\s|$)",
                          QRegularExpression::CaseInsensitiveOption)
           .match(fid)
           .hasMatch())
    return false;
  firmwareAuthorized = true;
  return true;
}
bool Session::enterDfu(const QString &model, const QString &path) {
  if (!live || demo || !firmwareAuthorized || motion || busy() || stream ||
      upload || !scriptPaused || !(confirmedFaults & 16) ||
      !stopStamp.isValid() || stopStamp.elapsed() > 2000 || model.isEmpty() || Protocol::supportedModel(trn) != model ||
      path.isEmpty() || usbPath() != path)
    return false;
  emit traffic("TX", "%DFU [modo atualização USB]");
  const bool ok = sendRaw("%DFU 321654987");
  close(); // Expected re-enumeration: never interpret the disappearing serial
           // port as an ACK.
  return ok;
}
