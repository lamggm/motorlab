#include "firmware.h"
#include "catalog.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtEndian>
quint32 Firmware::crc(const QByteArray &bytes) {
  quint32 v = 0xffffffff;
  for (auto b : bytes) {
    v ^= quint8(b);
    for (int i = 0; i < 8; ++i)
      v = (v >> 1) ^ ((v & 1) ? 0xedb88320U : 0U);
  }
  return v;
}
FirmwareImage Firmware::parse(const QByteArray &b, const QString &model) {
  FirmwareImage f;
  auto bad = [&](const QString &e) {
    f.error = e;
    return f;
  };
  if (b.size() < 309 || b.size() > 1024 * 1024 || b.left(5) != "DfuSe" ||
      quint8(b[5]) != 1 || quint8(b[10]) != 1)
    return bad("Contêiner DfuSe inválido; é necessário um único alvo.");
  auto u32 = [&](int p) {
    return qFromLittleEndian<quint32>(b.constData() + p);
  };
  auto u16 = [&](int p) {
    return qFromLittleEndian<quint16>(b.constData() + p);
  };
  const int tail = b.size() - 16;
  if (u32(6) != quint32(tail) || b.mid(tail + 8, 3) != "UFD" ||
      quint8(b[tail + 11]) != 16 || u16(tail + 4) != 0x0483 ||
      u16(tail + 6) != 0x011a || u32(b.size() - 4) != crc(b.left(b.size() - 4)))
    return bad("Tamanho, identificação USB ou CRC do firmware inválido.");
  if (b.mid(11, 6) != "Target" || quint8(b[17]) != 0 || u32(281) != 1 ||
      u32(277) != quint32(tail - 285))
    return bad("Alvo/elementos de firmware não suportados.");
  f.address = u32(285);
  const quint32 length = u32(289);
  if (f.address != 0x08010000 || length < 1024 || length > 0x70000 ||
      length != quint32(tail - 293))
    return bad("Região de Flash não corresponde à aplicação RoboG4 dual.");
  // The cryptographic allowlist binds the original vendor image to the exact
  // model.
  Catalog::all();
  QFile manifest(":/firmware.json");
  if (!manifest.open(QIODevice::ReadOnly))
    return bad("Manifesto de firmware ausente.");
  f.sha256 = QString::fromLatin1(
      QCryptographicHash::hash(b, QCryptographicHash::Sha256).toHex());
  bool found = false;
  for (const auto &v : QJsonDocument::fromJson(manifest.readAll()).array()) {
    const auto o = v.toObject();
    if (o["model"].toString() == model && o["sha256"].toString() == f.sha256) {
      f.model = model;
      f.version = o["version"].toString();
      found = true;
      break;
    }
  }
  if (!found)
    return bad("Arquivo não reconhecido para este modelo. Use o DFU oficial "
               "incluído, sem alterações.");
  f.container = b;
  f.payload = b.mid(293, length);
  return f;
}
QStringList Firmware::dfuArguments(const QString &path) {
  if (!QRegularExpression("^[1-9][0-9]*-[1-9][0-9]*(?:\\.[1-9][0-9]*)*$")
           .match(path)
           .hasMatch())
    return {};
  // DFU-only matching, restricted to the USB physical path of the identified
  // controller.
  return {"-d", ",0483:df11", "-p", path, "-a", "0"};
}
