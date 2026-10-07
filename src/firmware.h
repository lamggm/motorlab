#pragma once
#include <QByteArray>
#include <QString>
struct FirmwareImage {
  QByteArray container, payload;
  QString model, version, sha256, error;
  quint32 address = 0;
  bool valid() const { return error.isEmpty() && !payload.isEmpty(); }
};
namespace Firmware {
FirmwareImage parse(const QByteArray &bytes, const QString &model);
quint32 crc(const QByteArray &bytes);
QStringList dfuArguments(const QString &usbPath);
} // namespace Firmware
