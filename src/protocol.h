#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
namespace Protocol {
struct Command {
  QString text, key;
  bool valid = false, write = false, motion = false, emergency = false,
       maintenance = false, hex = false;
  int channel = 0;
  qint64 value = 0;
};
struct Reply {
  enum Kind { Value, Ack, Error, Other } kind = Other;
  QString key, payload, raw;
  QVector<qint64> numbers;
  bool numeric = false;
};
QString redacted(const QString &text);
Command command(const QString &text);
Reply reply(const QString &line);
QString supportedModel(const QString &identity);
bool compatible(const QString &firmware, const QString &model);
QStringList telemetryCommands();
QStringList faultNames(quint64 flags);
QStringList statusNames(quint64 flags);
QStringList motorNames(quint64 flags);
} // namespace Protocol
