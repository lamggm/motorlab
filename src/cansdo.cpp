#include "cansdo.h"
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtEndian>
#include <cerrno>
#include <cstring>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
QByteArray SdoTransfer::begin(quint16 object, quint8 subindex) {
  *this = SdoTransfer();
  index = object;
  sub = subindex;
  active = true;
  QByteArray b(8, 0);
  b[0] = 0x40;
  qToLittleEndian(index, b.data() + 1);
  b[3] = sub;
  return b;
}
QByteArray SdoTransfer::accept(const QByteArray &b) {
  if (!active)
    return {};
  auto bad = [&](const QString &e) {
    error = e;
    active = false;
    return QByteArray();
  };
  if (b.size() != 8)
    return bad("SDO com DLC diferente de 8.");
  quint8 command = b[0];
  if (command == 0x80) {
    if (qFromLittleEndian<quint16>(b.constData() + 1) != index ||
        quint8(b[3]) != sub)
      return {};
    return bad(QString("Abort SDO 0x%1")
                   .arg(qFromLittleEndian<quint32>(b.constData() + 4), 8, 16,
                        QChar('0')));
  }
  if (!segmented) {
    if (qFromLittleEndian<quint16>(b.constData() + 1) != index ||
        quint8(b[3]) != sub)
      return {};
    if ((command & 0xe0) != 0x40 || (command & 0x10) ||
        (!(command & 2) && (command & 0x0c)) ||
        (!(command & 1) && (command & 0x0c)))
      return bad("Resposta de upload SDO inválida.");
    if (command & 2) {
      int count = (command & 1) ? 4 - ((command >> 2) & 3) : 4;
      value = b.mid(4, count);
      active = false;
      return {};
    }
    if (command & 1) {
      auto size = qFromLittleEndian<quint32>(b.constData() + 4);
      if (size > 4096)
        return bad("SDO acima do limite de 4096 bytes.");
      expected = size;
    }
    segmented = true;
  } else {
    if ((command & 0xe0) != 0 || bool(command & 0x10) != toggle)
      return bad("Classe/toggle do segmento SDO inválido.");
    bool last = command & 1;
    int unused = (command >> 1) & 7;
    if (!last && unused)
      return bad("Segmento intermediário incompleto.");
    value += b.mid(1, 7 - unused);
    if (value.size() > 4096 || (expected >= 0 && value.size() > expected))
      return bad("Tamanho de SDO excedido.");
    if (last) {
      if (expected >= 0 && value.size() != expected)
        return bad("SDO truncado.");
      active = false;
      return {};
    }
    toggle = !toggle;
  }
  QByteArray request(8, 0);
  request[0] = 0x60 | (toggle ? 0x10 : 0);
  return request;
}
CanSdo::CanSdo(QObject *parent) : QObject(parent) {
  deadline.setSingleShot(true);
  deadline.setInterval(900);
  connect(&deadline, &QTimer::timeout, this, [this] {
    auto index = transfer.index;
    auto sub = transfer.sub;
    close();
    emit received(index, sub, {}, "Timeout SDO; conexão encerrada.");
  });
}
CanSdo::~CanSdo() { close(); }
bool CanSdo::open(const QString &interface, int selectedNode) {
  close();
  error.clear();
  if (!QRegularExpression("^[a-zA-Z0-9_.-]{1,15}$")
           .match(interface)
           .hasMatch() ||
      selectedNode < 1 || selectedNode > 127) {
    error = "Interface/nó inválido.";
    return false;
  }
  auto runtime =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
  if (runtime.isEmpty()) {
    error = "Diretório de execução não disponível.";
    return false;
  }
  lock =
      std::make_unique<QLockFile>(runtime + "/motorlab-can-" + interface + "-" +
                                  QString::number(selectedNode) + ".lock");
  lock->setStaleLockTime(0);
  if (!lock->tryLock()) {
    error = "Este nó já está reservado por outro MotorLab.";
    return false;
  }
  fd = ::socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, CAN_RAW);
  auto fail = [&](const QString &e) {
    error = e;
    close();
    return false;
  };
  if (fd < 0)
    return fail(QString::fromLocal8Bit(std::strerror(errno)));
  ifreq ifr{};
  auto name = interface.toLocal8Bit();
  std::memcpy(ifr.ifr_name, name.constData(), size_t(name.size()));
  if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0)
    return fail("Interface SocketCAN inexistente.");
  int index = ifr.ifr_ifindex;
  if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0 || !(ifr.ifr_flags & IFF_UP))
    return fail("Interface CAN não está ativa. Configure-a externamente antes "
                "de conectar.");
  can_filter filter{quint32(0x580 + selectedNode),
                    CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG};
  if (setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, &filter, sizeof filter) < 0)
    return fail("Falha no filtro CAN.");
  sockaddr_can address{};
  address.can_family = AF_CAN;
  address.can_ifindex = index;
  if (::bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof address) < 0)
    return fail("Não foi possível vincular a interface CAN.");
  node = selectedNode;
  reader = new QSocketNotifier(fd, QSocketNotifier::Read, this);
  connect(reader, &QSocketNotifier::activated, this, [this] { consume(); });
  return true;
}
void CanSdo::close() {
  deadline.stop();
  transfer.active = false;
  if (reader) {
    reader->setEnabled(false);
    reader->deleteLater();
    reader = nullptr;
  }
  if (fd >= 0) {
    ::close(fd);
    fd = -1;
  }
  if (lock) {
    lock->unlock();
    lock.reset();
  }
}
bool CanSdo::send(const QByteArray &data) {
  if (fd < 0 || data.size() != 8)
    return false;
  can_frame frame{};
  frame.can_id = 0x600 + node;
  frame.can_dlc = 8;
  std::memcpy(frame.data, data.constData(), 8);
  if (::write(fd, &frame, sizeof frame) != sizeof frame) {
    close();
    return false;
  }
  deadline.start();
  return true;
}
bool CanSdo::read(quint16 index, quint8 sub) {
  return fd >= 0 && !busy() && send(transfer.begin(index, sub));
}
void CanSdo::consume() {
  can_frame frame{};
  while (fd >= 0) {
    auto n = ::read(fd, &frame, sizeof frame);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      return;
    if (n != sizeof frame) {
      auto i = transfer.index;
      auto s = transfer.sub;
      close();
      emit received(i, s, {}, "Falha de leitura SocketCAN.");
      return;
    }
    if (!busy() || frame.can_id != quint32(0x580 + node))
      continue;
    const auto next = transfer.accept(
        frame.can_dlc == 8
            ? QByteArray(reinterpret_cast<const char *>(frame.data), 8)
            : QByteArray());
    if (!next.isEmpty()) {
      if (!send(next)) {
        emit received(transfer.index, transfer.sub, {},
                      "Falha ao solicitar segmento CAN.");
        return;
      }
    } else if (!transfer.active) {
      deadline.stop();
      emit received(transfer.index, transfer.sub, transfer.value,
                    transfer.error);
    }
  }
}
