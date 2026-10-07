#pragma once
#include <QString>
#include <QStringList>
struct HexImage {
  QStringList records;
  QByteArray bytes;
  QString error;
  bool valid() const { return error.isEmpty() && !records.isEmpty(); }
};
namespace HexFile {
HexImage parse(const QByteArray &text);
}
