#pragma once
#include <QJsonObject>
#include <QString>
#include <QVector>
struct ConfigSpec {
  QString key, name, group, unit, unitNote;
  QJsonObject indexUnits;
  int indexMax = 0, page = 0, canIndex = 0;
  qint64 minimum = 0, maximum = 0;
  double scale = 1;
  bool editable = false, disruptive = false;
};
namespace Catalog {
const QVector<ConfigSpec> &all();
QJsonObject original(const QString &model, const QString &key, int index);
QJsonObject commonOriginal(const QString &key, int index);
const ConfigSpec *find(const QString &key);
bool validIndex(const ConfigSpec &spec, int index);
bool validValue(const ConfigSpec &spec, qint64 value, int index);
QString suffix(const ConfigSpec &spec, int index);
} // namespace Catalog
