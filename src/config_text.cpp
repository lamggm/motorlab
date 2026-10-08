#include "config_text.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

QJsonObject ConfigText::dictionary() {
  static const QJsonObject data = [] {
    Catalog::all(); // Register the embedded resources before loading text.
    QFile file(":/config-pt.json");
    if (!file.open(QIODevice::ReadOnly))
      qFatal("Traduções de configuração ausentes");
    return QJsonDocument::fromJson(file.readAll()).object();
  }();
  return data;
}
QString ConfigText::name(const ConfigSpec &s, int index) {
  const auto base = dictionary()["parameters"]
                        .toObject()[s.key]
                        .toObject()["name"]
                        .toString();
  if (s.key == "OTL")
    return index == 1 ? "Temperatura máxima do controlador"
                      : "Temperatura máxima do motor";
  if (s.key == "CGT")
    return index == 1 ? "Tempo de guarda do nó" : "Fator de vida do nó";
  if (s.key == "ZSMC")
    return QStringList{"Zero do seno", "Zero do cosseno",
                       "Relação seno/cosseno"}
        .value((index - 1) % 3);
  if (s.key == "HSAT")
    return base + QString(" %1").arg((index - 1) % 6 + 1);
  if (s.key == "KPG" || s.key == "KIG" || s.key == "KDG")
    return base + (index <= 2 ? " de velocidade" : " de posição");
  if (s.key == "KPF" || s.key == "KIF")
    return base + (index <= 2 ? " de fluxo" : " de torque");
  return base.isEmpty() ? s.name : base;
}
QString ConfigText::description(const ConfigSpec &s, int index) {
  if (s.key == "CGT")
    return index == 1 ? "Define o tempo de guarda do nó CANopen em "
                        "milissegundos. Zero desabilita a supervisão de vida."
                      : "Multiplica o tempo de guarda para determinar o tempo "
                        "de vida permitido do nó CANopen.";
  if (s.key == "ZSMC")
    return QStringList{
        "Define o ponto zero do sinal de seno na calibração Sin/Cos.",
        "Define o ponto zero do sinal de cosseno na calibração Sin/Cos.",
        "Define a relação entre os sinais de seno e cosseno na calibração "
        "Sin/Cos."}
        .value((index - 1) % 3);
  return dictionary()["parameters"]
      .toObject()[s.key]
      .toObject()["description"]
      .toString();
}
QString ConfigText::category(const QString &original) {
  return dictionary()["categories"]
      .toObject()[original]
      .toObject()["name"]
      .toString(original);
}
QString ConfigText::categoryDescription(const QString &original) {
  return dictionary()["categories"]
      .toObject()[original]
      .toObject()["description"]
      .toString();
}
QString ConfigText::choice(const QString &original) {
  return dictionary()["choices"].toObject()[original].toString(original);
}
QString ConfigText::tooltip(const ConfigSpec &s, int index) {
  auto meta = Catalog::commonOriginal(s.key, index);
  const auto path = meta["path"].toArray();
  auto original = path.isEmpty() ? s.name : path.last().toString();
  bool numeric = false;
  original.toInt(&numeric);
  if (numeric)
    original = s.name + " · " + original;
  QStringList parts;
  for (auto part : path)
    parts.append(part.toString().replace("&amp;", "&"));
  return QString("<b>%1</b><br/><br/>%2<br/><br/><small>%3 · %4<br/>Manual "
                 "v3.0c, página %5.%6</small>")
      .arg(original.toHtmlEscaped(), description(s, index).toHtmlEscaped(),
           Catalog::suffix(s, index).toHtmlEscaped(),
           parts.join(" / ").toHtmlEscaped())
      .arg(s.page)
      .arg(s.unitNote.isEmpty() ? QString()
                                : "<br/>" + s.unitNote.toHtmlEscaped());
}
QString ConfigText::categoryTooltip(const QString &original) {
  return "<b>" + QString(original).replace("&amp;", "&").toHtmlEscaped() +
         "</b><br/><br/>" + categoryDescription(original).toHtmlEscaped();
}
