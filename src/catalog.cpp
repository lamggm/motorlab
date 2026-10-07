#include "catalog.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
static void resources() { Q_INIT_RESOURCE(resources); }
const QVector<ConfigSpec> &Catalog::all() {
  static const QVector<ConfigSpec> specs = [] {
    resources();
    QFile file(":/catalog.json");
    if (!file.open(QIODevice::ReadOnly))
      qFatal("Catálogo de configuração ausente");
    QVector<ConfigSpec> result;
    for (const auto &v : QJsonDocument::fromJson(file.readAll())
                             .object()["parameters"]
                             .toArray()) {
      const auto o = v.toObject();
      ConfigSpec s;
      s.key = o["key"].toString();
      s.name = o["name"].toString();
      s.group = o["group"].toString();
      s.unit = o["unit"].toString();
      s.unitNote = o["unit_note"].toString();
      s.indexUnits = o["index_units"].toObject();
      s.indexMax = o["index_max"].toInt();
      s.page = o["page"].toInt();
      s.canIndex = o["can_index"].toInt();
      s.minimum = qint64(o["min"].toDouble());
      s.maximum = qint64(o["max"].toDouble());
      s.scale = o["scale"].toDouble(1);
      s.editable = o["editable"].toBool();
      s.disruptive = o["disruptive"].toBool();
      if (s.key == "TELS")
        continue; // String transport is separate from the numeric catalog.
      result.append(s);
    }
    return result;
  }();
  return specs;
}
const ConfigSpec *Catalog::find(const QString &key) {
  for (const auto &s : all())
    if (s.key == key)
      return &s;
  return nullptr;
}
bool Catalog::validIndex(const ConfigSpec &s, int i) {
  return s.indexMax ? i >= 1 && i <= s.indexMax : i == 0;
}
QJsonObject Catalog::original(const QString &model, const QString &key,
                              int index) {
  static const QJsonObject trees = [] {
    resources();
    QFile file(":/original-trees.json");
    if (!file.open(QIODevice::ReadOnly))
      qFatal("Original trees missing");
    return QJsonDocument::fromJson(file.readAll()).object();
  }();
  for (auto value : trees[model].toObject()["entries"].toArray()) {
    auto e = value.toObject();
    if (e["command"].toString() == key &&
        e["index"].toInt() == (index ? index : 1))
      return e;
  }
  return {};
}
QJsonObject Catalog::commonOriginal(const QString &key, int index) {
  auto a = original("SBLG2360T", key, index),
       b = original("SBLMG2360T", key, index);
  return !a.isEmpty() && a["path"] == b["path"] &&
                 a["choices"] == b["choices"] && a["type"] == b["type"]
             ? a
             : QJsonObject();
}
bool Catalog::validValue(const ConfigSpec &s, qint64 v, int i) {
  const auto meta = commonOriginal(s.key, i);
  const auto choices = meta["control"].toString() == "ComboBox"
                           ? meta["choices"].toArray()
                           : QJsonArray();
  if (!choices.isEmpty()) {
    bool found = false;
    for (auto c : choices) {
      auto e = c.toObject();
      if (e["value"].toInt() == v &&
          !e["label"].toString().trimmed().isEmpty() &&
          !e["label"].toString().trimmed().startsWith('~'))
        found = true;
    }
    if (!found)
      return false;
  }
  return s.editable && validIndex(s, i) && v >= s.minimum && v <= s.maximum &&
         !(s.key == "CGT" && i == 2 && v > 255) &&
         !(s.key == "OTL" && i == 1 && v > 85);
}
QString Catalog::suffix(const ConfigSpec &s, int i) {
  return s.key + (i ? QString(" %1").arg(i) : QString());
}
