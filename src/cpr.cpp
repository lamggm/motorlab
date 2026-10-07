#include "cpr.h"
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
CprProfile CprProfile::parse(const QByteArray &data) {
  CprProfile p;
  auto bad = [&](QString e) {
    p.error = e;
    p.items.clear();
    return p;
  };
  if (data.isEmpty() || data.size() > 1024 * 1024)
    return bad("CPR vazio ou maior que 1 MiB.");
  QXmlStreamReader x(data);
  // Reject DTD/entities instead of expanding untrusted profiles.
  while (!x.atEnd() && !x.isStartElement()) {
    x.readNext();
    if (x.isDTD())
      return bad("DTD não permitido em CPR.");
  }
  if (x.name() != u"ControllerProfile" ||
      x.attributes().value("Version") != u"1.0")
    return bad("Formato CPR não suportado.");
  QSet<QString> seen, sections;
  QMap<QString, CprItem> canonical;
  while (x.readNextStartElement()) {
    QString name = x.name().toString();
    if (sections.contains(name))
      return bad("Seção CPR duplicada.");
    sections.insert(name);
    if (name == "Firmware")
      p.firmware = x.readElementText();
    else if (name == "Roborun")
      p.roborun = x.readElementText();
    else if (name == "ControlTree" || name == "PowerTree") {
      (name == "ControlTree" ? p.controlTree : p.powerTree) =
          x.attributes().value("Name").toString();
      while (x.readNextStartElement()) {
        if (x.name() != u"Item")
          return bad("Elemento CPR desconhecido.");
        CprItem item;
        item.tree = name;
        item.command = x.attributes().value("Command").toString();
        item.node = x.attributes().value("Node").toString();
        bool ok = false;
        item.index = x.attributes().value("Index").toInt(&ok);
        if (!ok || item.index < 0 || item.index > 65535 ||
            !QRegularExpression("^[A-Z][A-Z0-9]{0,15}$")
                 .match(item.command)
                 .hasMatch() ||
            seen.contains(item.tree + "|" + item.node + "|" + item.identity()))
          return bad("Comando/índice CPR inválido ou duplicado.");
        seen.insert(item.tree + "|" + item.node + "|" + item.identity());
        QSet<QString> fields;
        while (x.readNextStartElement()) {
          QString field = x.name().toString();
          if (fields.contains(field))
            return bad("Campo CPR duplicado.");
          fields.insert(field);
          QString text = x.readElementText();
          if (field == "ArgumentType")
            item.type = text;
          else if (field == "Value")
            item.value = text;
          else if (field == "Multiplier") {
            item.multiplier = text.toInt(&ok);
            if (!ok || item.multiplier < 1 || item.multiplier > 1000000000)
              return bad("Multiplicador CPR inválido.");
          } else
            return bad("Campo CPR desconhecido.");
        }
        if (fields.size() != 3 ||
            !QStringList{"String", "IntegerList", "IntegerValue"}.contains(
                item.type))
          return bad("Tipo CPR não suportado.");
        if (item.type != "String") {
          bool numeric;
          const auto n = item.value.toLongLong(&numeric);
          if (!numeric || n < -2147483648LL || n > 4294967295LL)
            return bad("Valor CPR fora de 32 bits.");
        }
        if (canonical.contains(item.identity())) {
          const auto before = canonical[item.identity()];
          if (before.value != item.value ||
              before.multiplier != item.multiplier || before.type != item.type)
            return bad("Aliases CPR com valores conflitantes.");
        } else
          canonical.insert(item.identity(), item);
        p.items.append(item);
        if (p.items.size() > 10000)
          return bad("Itens CPR em excesso.");
      }
    } else
      return bad("Seção CPR desconhecida.");
  }
  while (!x.atEnd()) {
    x.readNext();
    if (x.isDTD() || x.isStartElement())
      return bad("Conteúdo depois do perfil CPR.");
  }
  if (x.hasError() || p.firmware.isEmpty() || p.items.isEmpty())
    return bad("XML CPR inválido ou incompleto.");
  return p;
}
QByteArray CprProfile::xml() const {
  QByteArray out;
  QXmlStreamWriter w(&out);
  w.setAutoFormatting(true);
  w.writeStartDocument();
  w.writeStartElement("ControllerProfile");
  w.writeAttribute("Version", "1.0");
  w.writeTextElement("Firmware", firmware);
  w.writeTextElement("Roborun", roborun);
  for (const auto &tree : QStringList{"ControlTree", "PowerTree"}) {
    w.writeStartElement(tree);
    w.writeAttribute("Name", tree == "ControlTree" ? controlTree : powerTree);
    for (const auto &item : items)
      if (item.tree == tree) {
        w.writeStartElement("Item");
        w.writeAttribute("Command", item.command);
        w.writeAttribute("Index", QString::number(item.index));
        w.writeAttribute("Node", item.node);
        w.writeTextElement("ArgumentType", item.type);
        w.writeTextElement("Multiplier", QString::number(item.multiplier));
        w.writeTextElement("Value", item.value);
        w.writeEndElement();
      }
    w.writeEndElement();
  }
  w.writeEndElement();
  w.writeEndDocument();
  return out;
}
