#include "protocol.h"
#include "catalog.h"
#include <QRegularExpression>
#include <QSet>
namespace Protocol {
QString redacted(const QString &text) {
  QString safe = text;
  safe.replace(QRegularExpression("%((?:LK)|(?:UK)) +[0-9]+"),
               "%\\1 [chave protegida]");
  return safe;
}
Command command(const QString &text) {
  Command c;
  if (text.isEmpty() || text.size() > 600)
    return c;
  for (auto ch : text)
    if (ch.unicode() < 32 || ch.unicode() > 126)
      return c;
  c.text = text.trimmed();
  if (c.text.startsWith(':')) {
    const QByteArray hex = c.text.mid(1).toLatin1();
    if (hex.size() < 10 || hex.size() % 2 ||
        !QRegularExpression("^[0-9a-fA-F]+$")
             .match(QString::fromLatin1(hex))
             .hasMatch())
      return c;
    const auto b = QByteArray::fromHex(hex);
    int sum = 0;
    for (auto ch : b)
      sum += quint8(ch);
    if (b.size() != quint8(b[0]) + 5 || (sum & 255))
      return c;
    c.valid = true;
    c.write = true;
    c.maintenance = true;
    c.hex = true;
    c.key = "HEX";
    return c;
  }
  static const QRegularExpression syntax(
      "^([?~^!%])([A-Z][A-Z0-9]*)(?: (-?[0-9]+))?(?: (-?[0-9]+))?$");
  const auto m = syntax.match(c.text);
  if (!m.hasMatch())
    return c;
  c.key = m.captured(2);
  const QString prefix = m.captured(1);
  bool aOk = true, bOk = true;
  const bool hasA = !m.captured(3).isEmpty(), hasB = !m.captured(4).isEmpty();
  const qint64 a = hasA ? m.captured(3).toLongLong(&aOk) : 0,
               b = hasB ? m.captured(4).toLongLong(&bOk) : 0;
  if (!aOk || !bOk)
    return c;
  if (a >= 0 && a <= 65535)
    c.channel = int(a);
  c.value = b;
  if (prefix == "~" || prefix == "^") {
    const auto s = Catalog::find(c.key);
    if (!s)
      return c;
    const int index = s->indexMax ? int(a) : 0;
    if (s->indexMax && (a < 1 || a > s->indexMax))
      return c;
    if (prefix == "~")
      c.valid = !hasB && (s->indexMax ? hasA : !hasA);
    else {
      c.write = true;
      c.value = s->indexMax ? b : a;
      c.channel = index;
      c.valid = (s->indexMax ? hasA && hasB : hasA && !hasB) &&
                Catalog::validValue(*s, c.value, index);
    }
  } else if (prefix == "?") {
    if (hasB)
      return c;
    if (QSet<QString>{"FID", "TRN", "FF", "FS", "DI", "DO", "LK"}.contains(
            c.key))
      c.valid = !hasA;
    else if (QSet<QString>{"A", "S", "C", "P", "F", "FM", "CB", "BS", "SNA",
                           "ANG", "E", "M", "RMP"}
                 .contains(c.key))
      c.valid = !hasA || (a >= 1 && a <= 2);
    else if (QSet<QString>{"MA", "MC", "K"}.contains(c.key))
      c.valid = hasA && a >= 1 && a <= 4;
    else if (c.key == "AI" || c.key == "PI")
      c.valid = hasA && a >= 1 && a <= 8;
    else if (c.key == "V")
      c.valid = hasA && a >= 1 && a <= 3;
    else if (c.key == "T")
      c.valid = hasA && a >= 1 && a <= 5;
    else if (c.key == "VAR")
      c.valid = hasA && a >= 1 && a <= 32;
  } else if (prefix == "!") {
    c.write = true;
    if (c.key == "EX") {
      c.valid = !hasA && !hasB;
      c.emergency = true;
    } else if (c.key == "MS" || c.key == "QST") {
      c.valid = hasA && !hasB && a >= 1 && a <= 2;
      c.emergency = true;
    } else if (c.key == "MG") {
      c.valid = !hasA && !hasB;
      c.motion = true;
    } else if (c.key == "R") {
      c.valid = !hasB && (!hasA || (a >= 0 && a <= 2));
      c.motion = !hasA || a != 0;
      c.emergency = hasA && a == 0;
    } else if (QSet<QString>{"G", "S", "P", "PR", "GIQ", "GID"}.contains(
                   c.key)) {
      c.valid = hasA && hasB && a >= 1 && a <= 2;
      c.motion = true;
      if (c.key == "G")
        c.valid &= b >= -1000 && b <= 1000;
      else if (c.key == "S")
        c.valid &= b >= -65535 && b <= 65535;
      else if (c.key == "GIQ" || c.key == "GID")
        c.valid &= b >= -450 && b <= 450;
      else
        c.valid &= b >= -2147483648LL && b <= 2147483647LL;
    } else if (c.key == "D0" || c.key == "D1") {
      c.valid = hasA && !hasB && a >= 1 && a <= 4;
      c.motion = true;
    } else if (c.key == "C" || c.key == "CB") {
      c.valid = hasA && hasB && a >= 1 && a <= 2 && b >= -2147483648LL &&
                b <= 2147483647LL;
    }
  } else if (prefix == "%") {
    c.write = true;
    c.maintenance = true;
    if (QSet<QString>{"EESAV", "EELD", "SLD"}.contains(c.key))
      c.valid = !hasA && !hasB;
    if (QSet<QString>{"EERST", "CLSAV"}.contains(c.key))
      c.valid = hasA && !hasB && a == 321654987;
    if (c.key == "LK" || c.key == "UK")
      c.valid = hasA && !hasB && a >= 1 && a <= 4294967295LL;
    if (c.key == "CLMOD") {
      c.valid = hasA && !hasB && (a == 0 || a == 2 || a == 3);
      c.motion = a != 0;
      c.emergency = a == 0;
    }
  }
  return c;
}
Reply reply(const QString &line) {
  Reply r;
  r.raw = line;
  if (line.size() > 4096)
    return r;
  const QString s = line.trimmed();
  if (s == "+") {
    r.kind = Reply::Ack;
    return r;
  }
  if (s == "-" || s == "?") {
    r.kind = Reply::Error;
    return r;
  }
  static const QRegularExpression syntax(
      "^([A-Z][A-Z0-9]+|[A-Z])\\s*=\\s*(.+)$");
  const auto m = syntax.match(s);
  if (!m.hasMatch())
    return r;
  r.kind = Reply::Value;
  r.key = m.captured(1);
  r.payload = m.captured(2).trimmed();
  r.numeric = true;
  for (const auto &part : r.payload.split(':')) {
    bool ok = false;
    const qint64 n = part.toLongLong(&ok);
    if (!QRegularExpression("^-?[0-9]+$").match(part).hasMatch() || !ok) {
      r.numeric = false;
      r.numbers.clear();
      break;
    }
    r.numbers.append(n);
  }
  return r;
}
QString supportedModel(const QString &identity) {
  const auto m = identity.section(':', -1).trimmed().toUpper();
  return m == "SBLG2360T" || m == "SBLMG2360T" ? m : QString();
}
bool compatible(const QString &firmware, const QString &model) {
  static const QRegularExpression v("\\bv3\\.0f(?=\\s|$)",
                                    QRegularExpression::CaseInsensitiveOption);
  const auto actual = supportedModel(model);
  const auto family =
      QRegularExpression("\\b(SBLMG2XXX|SBLG2XXX)\\b",
                         QRegularExpression::CaseInsensitiveOption)
          .match(firmware);
  if (family.hasMatch() &&
      actual != (family.captured(1).toUpper() == "SBLMG2XXX" ? "SBLMG2360T"
                                                             : "SBLG2360T"))
    return false;
  return !actual.isEmpty() && v.match(firmware).hasMatch();
}
QStringList telemetryCommands() {
  QStringList commands = {"?A",  "?S", "?BS 1", "?BS 2", "?V 2",  "?T 2", "?C",
                          "?DI", "?DO", "?FM 1", "?FM 2"};
  for (int i = 1; i <= 8; ++i) {
    commands << QString("?AI %1").arg(i) << QString("?PI %1").arg(i);
  }
  commands << "?FF" << "?FS";
  return commands;
}

static QStringList names(quint64 flags, const QStringList &labels) {
  QStringList out;
  for (int i = 0; i < labels.size(); ++i)
    if (flags & (quint64(1) << i))
      out.append(labels[i]);
  return out.isEmpty() ? QStringList{"Nenhum"} : out;
}
QStringList faultNames(quint64 f) {
  return names(f, {"Sobretemperatura", "Sobretensão", "Subtensão",
                   "Curto-circuito", "Parada de emergência", "Sensor/motor",
                   "MOSFET", "Configuração padrão", "STO"});
}
QStringList motorNames(quint64 f) {
  return names(f, {"AmpLim", "Stall", "LoopErr", "QuickStop", "FwdLim",
                   "RevLim", "AmpTrig", "FETs Off"});
}
QStringList statusNames(quint64 f) {
  return names(f, {"Serial", "Pulso", "Analógico", "FETs desligados", "Stall",
                   "Limite", "STO", "Script", "Setup"});
}
} // namespace Protocol
