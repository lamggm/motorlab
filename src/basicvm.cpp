#include "basicvm.h"
#include "catalog.h"
#include "protocol.h"
#include <QRegularExpression>
#include <functional>
#include <limits>
namespace {
struct Token {
  QString text;
  bool number = false;
};
QVector<Token> lex(const QString &s) {
  QVector<Token> t;
  int p = 0;
  static QRegularExpression rx("\\G\\s*(0x[0-9a-f]+|[0-9]+|[a-z_][a-z_0-9]*|<>|"
                               "<=|>=|<<|>>|[+*/%<>=(),\\[\\]\\-])",
                               QRegularExpression::CaseInsensitiveOption);
  while (p < s.size()) {
    if (s.mid(p).trimmed().isEmpty())
      break;
    auto m = rx.match(s, p);
    if (!m.hasMatch())
      throw QString("Expressão inválida perto de: ") + s.mid(p);
    QString v = m.captured(1).toLower();
    t.append({v, v[0].isDigit()});
    p = m.capturedEnd();
    if (t.size() > 256)
      throw QString("Expressão muito longa.");
  }
  return t;
}
int precedence(const QString &s) {
  if (s == "or")
    return 1;
  if (s == "xor")
    return 2;
  if (s == "and")
    return 3;
  if (QStringList{"=", "<>", "<", ">", "<=", ">="}.contains(s))
    return 4;
  if (s == "<<" || s == ">>")
    return 5;
  if (s == "+" || s == "-")
    return 6;
  if (s == "*" || s == "/" || s == "mod" || s == "%")
    return 7;
  return 0;
}
qint32 wrap(qint64 n) { return qint32(quint32(quint64(n))); }
QString literal(const QString &a) {
  if (!QRegularExpression(R"rx(^"(?:[^"\\]|\\.)*"$)rx").match(a).hasMatch())
    throw QString("Literal Print inválido.");
  auto value = a.mid(1, a.size() - 2);
  QString out;
  for (int p = 0; p < value.size(); ++p) {
    auto c = value[p];
    if (c != '\\') {
      out += c;
      continue;
    }
    if (++p >= value.size())
      throw QString("Escape incompleto.");
    auto e = value[p];
    if (e == 'n')
      out += '\n';
    else if (e == 'r')
      out += '\r';
    else if (e == 't')
      out += '\t';
    else if (e == 'a')
      out += QChar(7);
    else if (e == 'b')
      out += QChar(8);
    else if (e == 'f')
      out += QChar(12);
    else if (e == 'v')
      out += QChar(11);
    else if (e == '"' || e == '\\' || e == '\'')
      out += e;
    else
      throw QString("Escape ainda não suportado no simulador.");
  }
  return out;
}
QString stripComment(const QString &s) {
  bool quoted = false, escape = false;
  for (int i = 0; i < s.size(); ++i) {
    auto ch = s[i];
    if (escape) {
      escape = false;
      continue;
    }
    if (ch == '\\' && quoted) {
      escape = true;
      continue;
    }
    if (ch == '"')
      quoted = !quoted;
    if (ch == '\'' && !quoted)
      return s.left(i).trimmed();
  }
  if (quoted)
    throw QString("Texto sem aspas finais.");
  return s.trimmed();
}
} // namespace
QStringList BasicVm::arguments(const QString &s) {
  QStringList out;
  int start = 0, depth = 0;
  bool quoted = false, escape = false;
  for (int i = 0; i < s.size(); ++i) {
    auto c = s[i];
    if (escape) {
      escape = false;
      continue;
    }
    if (c == '\\' && quoted) {
      escape = true;
      continue;
    }
    if (c == '"')
      quoted = !quoted;
    if (!quoted) {
      if (c == '(' || c == '[')
        ++depth;
      if (c == ')' || c == ']')
        --depth;
      if (depth < 0)
        throw QString("Parênteses inválidos.");
      if (c == ',' && !depth) {
        out << s.mid(start, i - start).trimmed();
        start = i + 1;
      }
    }
  }
  if (quoted || depth)
    throw QString("Argumentos sem fechamento.");
  out << s.mid(start).trimmed();
  return out;
}
qint32 BasicVm::expression(const QString &s) {
  struct Value {
    qint32 n = 0;
    bool boolean = false, symbol = false;
  };
  auto tokens = lex(s);
  int pos = 0, depth = 0;
  bool allowSymbol = false;
  std::function<Value(int)> parse;
  std::function<Value()> atom = [&]() -> Value {
    if (++depth > 64)
      throw QString("Expressão aninhada demais.");
    if (pos >= tokens.size())
      throw QString("Expressão incompleta.");
    auto tok = tokens[pos++];
    auto name = tok.text;
    Value v;
    if (name == "-" || name == "+" || name == "not") {
      v = parse(8);
      if (v.symbol)
        throw QString("Macro só pode identificar Get/Set.");
      if (name == "not")
        v.n = v.boolean ? !v.n : ~v.n;
      else {
        v.n = name == "-" ? wrap(-qint64(v.n)) : v.n;
        v.boolean = false;
      }
    } else if (name == "(") {
      v = parse(1);
      if (pos >= tokens.size() || tokens[pos++].text != ")")
        throw QString("Falta ')'.");
    } else if (tok.number) {
      bool ok = false;
      auto n = name.toULongLong(&ok, name.startsWith("0x") ? 16 : 10);
      if (!ok || n > 4294967295ULL)
        throw QString("Inteiro fora de 32 bits.");
      v.n = wrap(n);
    } else if (name == "true" || name == "false") {
      v.n = name == "true";
      v.boolean = true;
    } else if (name.startsWith('_')) {
      if (!allowSymbol)
        throw QString("Macro só pode identificar Get/Set.");
      if (!symbols.contains(name)) {
        int id = symbols.size() + 1;
        symbols[name] = id;
        symbolNames[id] = name.mid(1).toUpper();
      }
      v.n = symbols[name];
      v.symbol = true;
    } else if (pos < tokens.size() && tokens[pos].text == "(") {
      ++pos;
      QVector<qint32> a;
      bool previous = allowSymbol;
      if (pos < tokens.size() && tokens[pos].text != ")") {
        do {
          allowSymbol = a.isEmpty() && QStringList{"getvalue", "getconfig",
                                                   "setconfig", "setcommand"}
                                           .contains(name);
          auto item = parse(1);
          if (item.symbol && !allowSymbol)
            throw QString("Macro inválida.");
          a << item.n;
          if (pos < tokens.size() && tokens[pos].text == ",") {
            ++pos;
            continue;
          }
          break;
        } while (true);
      }
      allowSymbol = previous;
      if (pos >= tokens.size() || tokens[pos++].text != ")")
        throw QString("Falta ')' na função.");
      v.n = function(name, a);
      v.boolean = name == "tobool";
    } else {
      QString key = name;
      if (pos < tokens.size() && tokens[pos].text == "[") {
        ++pos;
        auto index = parse(1).n;
        if (pos >= tokens.size() || tokens[pos++].text != "]")
          throw QString("Falta ']'.");
        if (!arrays.contains(name) ||
            (!checking && (index < 0 || index >= arrays[name])))
          throw QString("Índice de array inválido.");
        key += QString("[%1]").arg(index);
      }
      if (explicitVariables && !variables.contains(key) && !checking)
        throw QString("Variável não declarada: ") + name;
      if (explicitVariables && !variables.contains(name) &&
          !arrays.contains(name) && !variables.contains(key))
        throw QString("Variável não declarada: ") + name;
      v.n = variables.value(key);
      v.boolean = booleanVariables.contains(name);
    }
    --depth;
    return v;
  };
  parse = [&](int min) -> Value {
    Value left = atom();
    while (pos < tokens.size() && precedence(tokens[pos].text) >= min) {
      auto op = tokens[pos++].text;
      auto right = parse(precedence(op) + 1);
      if (left.symbol || right.symbol)
        throw QString("Macro não pode entrar em cálculo.");
      bool booleans = left.boolean && right.boolean;
      if (op == "+")
        left.n = wrap(qint64(left.n) + right.n);
      else if (op == "-")
        left.n = wrap(qint64(left.n) - right.n);
      else if (op == "*")
        left.n = wrap(qint64(left.n) * right.n);
      else if (op == "/" || op == "mod" || op == "%") {
        if (!right.n) {
          if (checking) {
            left = {0, false, false};
            continue;
          }
          throw QString("Divisão por zero.");
        }
        left.n = wrap(op == "/" ? qint64(left.n) / right.n
                                : qint64(left.n) % right.n);
      } else if (op == "and")
        left.n =
            booleans ? (bool(left.n) && bool(right.n)) : (left.n & right.n);
      else if (op == "or")
        left.n =
            booleans ? (bool(left.n) || bool(right.n)) : (left.n | right.n);
      else if (op == "xor")
        left.n =
            booleans ? (bool(left.n) != bool(right.n)) : (left.n ^ right.n);
      else if (op == "<<" || op == ">>") {
        if (right.n < 0 || right.n > 31)
          throw QString("Deslocamento fora de 0..31.");
        left.n = op == "<<" ? wrap(quint64(quint32(left.n)) << right.n)
                            : left.n >> right.n;
      } else if (op == "=")
        left.n = left.n == right.n;
      else if (op == "<>")
        left.n = left.n != right.n;
      else if (op == "<")
        left.n = left.n < right.n;
      else if (op == ">")
        left.n = left.n > right.n;
      else if (op == "<=")
        left.n = left.n <= right.n;
      else if (op == ">=")
        left.n = left.n >= right.n;
      left.boolean = (precedence(op) == 4) ||
                     (booleans && QStringList{"and", "or", "xor"}.contains(op));
    }
    return left;
  };
  auto result = parse(1);
  if (pos != tokens.size())
    throw QString("Operador inesperado: ") + tokens[pos].text;
  expressionBoolean = result.boolean;
  return result.n;
}

void BasicVm::assign(const QString &target, qint32 value) {
  auto m = QRegularExpression("^([a-z][a-z_0-9]*)(?:\\[(.+)\\])?$",
                              QRegularExpression::CaseInsensitiveOption)
               .match(target.trimmed());
  if (!m.hasMatch())
    throw QString("Destino inválido: ") + target;
  QString key = m.captured(1).toLower();
  if (constants.contains(key))
    throw QString("Constante não pode receber atribuição.");
  if (!m.captured(2).isEmpty()) {
    auto i = expression(m.captured(2));
    if (!arrays.contains(key) || (!checking && (i < 0 || i >= arrays[key])))
      throw QString("Índice de array inválido.");
    key += QString("[%1]").arg(i);
  }
  if (explicitVariables && !variables.contains(key) &&
      !arrays.contains(m.captured(1).toLower()))
    throw QString("Variável não declarada: ") + key;
  if (!checking)
    variables[key] = booleanVariables.contains(m.captured(1).toLower())
                         ? bool(value)
                         : value;
}
qint32 BasicVm::function(const QString &name, const QVector<qint32> &a) {
  const int n = a.size();
  auto require = [&](bool b) {
    if (!b)
      throw QString("Argumentos inválidos: ") + name;
  };
  if (name == "tobool") {
    require(n == 1);
    return bool(a[0]);
  }
  if (name == "abs") {
    require(n == 1);
    return a[0] < 0 ? wrap(-qint64(a[0])) : a[0];
  }
  if (name == "min" || name == "max") {
    require(n == 2);
    return name == "min" ? qMin(a[0], a[1]) : qMax(a[0], a[1]);
  }
  if (name == "getvalue" || name == "getconfig" || name == "setconfig" ||
      name == "setcommand") {
    const bool write = name.startsWith("set");
    require(n == (write ? 3 : 2) && symbolNames.contains(a[0]));
    const auto key = symbolNames[a[0]];
    const auto suffix = key + (a[1] ? QString(" %1").arg(a[1]) : QString());
    const auto cmd = Protocol::command(
        (name == "getvalue"    ? "?"
         : name == "getconfig" ? "~"
         : name == "setconfig" ? "^"
                               : "!") +
        suffix + (write ? " " + QString::number(a[2]) : QString()));
    if (checking) {
      if (name == "getconfig" || name == "setconfig")
        require(Catalog::find(key) &&
                (name == "getconfig" || Catalog::find(key)->editable));
      else
        require(Protocol::command(
                    (name == "getvalue" ? "?" : "!") + key +
                    ((key == "EX" || key == "MG" || key == "FF" ||
                      key == "FS" || key == "DI" || key == "DO" || key == "LK")
                         ? QString()
                         : QString(" 1")) +
                    (write && key != "EX" && key != "MG" ? QString(" 0")
                                                         : QString()))
                    .valid);
      return 0;
    }
    require(cmd.valid);
    if (name == "getvalue")
      return values.value(suffix);
    if (name == "getconfig")
      return config.value(suffix);
    if (name == "setconfig")
      config[suffix] = a[2];
    else
      commands[suffix] = a[2];
    return 0;
  }
  if (QStringList{"settimercount", "gettimercount", "settimerstate",
                  "gettimerstate"}
          .contains(name)) {
    bool set = name.startsWith("set");
    require(n == (set ? 2 : 1) && a[0] >= 0 && a[0] <= 7);
    if (checking)
      return 0;
    auto index = a[0];
    qint64 current =
        qMax<qint64>(0, qint64(timers.value(index)) -
                            (timerRunning.value(index)
                                 ? milliseconds - timerStamp.value(index)
                                 : 0));
    if (name == "gettimercount")
      return wrap(current);
    if (name == "gettimerstate")
      return current == 0 || !timerRunning.value(index) ? 1 : 0;
    timers[index] = name == "settimercount" ? a[1] : wrap(current);
    timerStamp[index] = milliseconds;
    if (name == "settimerstate") {
      require(a[1] == 0 || a[1] == 1);
      timerRunning[index] = a[1] == 0;
    } else {
      require(a[1] >= 0);
      timerRunning[index] = true;
    }
    return 0;
  }
  throw QString("Função ainda não suportada no simulador: ") + name;
}
void BasicVm::setInput(const QString &key, int index, qint32 value) {
  values[key.toUpper() + (index ? QString(" %1").arg(index) : QString())] =
      value;
}
bool BasicVm::compile(const QString &source) {
  *this = BasicVm();
  checking = true;
  struct Block {
    QString kind;
    int start;
    QVector<int> exits;
  };
  QVector<Block> blocks;
  auto append = [&](QString kind, QString arg, QString arg2, QString arg3,
                    int line, int jump = -1) {
    code.append({kind, arg, arg2, arg3, line, jump});
  };
  auto lines = source.split('\n');
  int line = 0;
  try {
    if (source.size() > 1024 * 1024 || lines.size() > 10000)
      throw QString("Script excede o limite local.");
    for (auto raw : lines) {
      ++line;
      auto s = stripComment(raw);
      if (s.isEmpty())
        continue;
      auto lower = s.toLower();
      if (lower == "option explicit") {
        explicitVariables = true;
        continue;
      }
      if (lower.startsWith("dim ")) {
        auto m =
            QRegularExpression("^dim +([a-z][a-z_0-9]*)(?:\\[([0-9]+)\\])? +as "
                               "+(integer|boolean)$",
                               QRegularExpression::CaseInsensitiveOption)
                .match(s);
        if (!m.hasMatch())
          throw QString("Declaração Dim inválida.");
        auto key = m.captured(1).toLower();
        if (m.captured(3).toLower() == "boolean")
          booleanVariables.insert(key);
        if (variables.contains(key) || arrays.contains(key))
          throw QString("Declaração duplicada.");
        if (m.captured(2).isEmpty())
          variables[key] = 0;
        else {
          int size = m.captured(2).toInt();
          if (size < 1 || size > 1024)
            throw QString("Array fora de 1..1024.");
          arrays[key] = size;
          for (int i = 0; i < size; ++i)
            variables[QString("%1[%2]").arg(key).arg(i)] = 0;
        }
        if (variables.size() > 4096)
          throw QString("Muitas variáveis.");
        continue;
      }
      if (lower.startsWith("#define ")) {
        auto m = QRegularExpression("^#define +([a-z][a-z_0-9]*) +(.+)$",
                                    QRegularExpression::CaseInsensitiveOption)
                     .match(s);
        if (!m.hasMatch())
          throw QString("#define inválido.");
        for (const auto &t : lex(m.captured(2)))
          if (!t.number &&
              QRegularExpression("^[a-z_]").match(t.text).hasMatch() &&
              !QStringList{"true", "false", "and", "or", "xor", "not", "mod"}
                   .contains(t.text) &&
              !constants.contains(t.text))
            throw QString("#define local aceita apenas expressões constantes.");
        auto key = m.captured(1).toLower();
        if (variables.contains(key))
          throw QString("Constante duplicada.");
        variables[key] = expression(m.captured(2));
        constants.insert(key);
        if (expressionBoolean)
          booleanVariables.insert(key);
        continue;
      }
      if (s.endsWith(':')) {
        auto name = s.left(s.size() - 1).toLower();
        if (!QRegularExpression("^[a-z][a-z_0-9]*$").match(name).hasMatch() ||
            labels.contains(name))
          throw QString("Rótulo inválido/duplicado.");
        labels[name] = code.size();
        continue;
      }
      if (lower.startsWith("if ")) {
        auto m = QRegularExpression("^if +(.+?) +then(.*)$",
                                    QRegularExpression::CaseInsensitiveOption)
                     .match(s);
        if (!m.hasMatch() || !m.captured(2).trimmed().isEmpty())
          throw QString(
              "Use If ... Then em bloco; If em linha única não é suportado.");
        expression(m.captured(1));
        blocks.append({"if", int(code.size()), {}});
        append("if", m.captured(1), {}, {}, line);
        continue;
      }
      if (lower == "else" || lower.startsWith("elseif ") ||
          lower.startsWith("else if ")) {
        if (blocks.isEmpty() || blocks.last().kind != "if")
          throw QString("Else sem If.");
        auto &b = blocks.last();
        int previous = b.start;
        if (previous < 0)
          throw QString("Else duplicado.");
        b.exits.append(code.size());
        append("jump", {}, {}, {}, line);
        code[previous].jump = code.size();
        if (lower == "else") {
          b.start = -1;
        } else {
          auto m = QRegularExpression("^else ?if +(.+?) +then$",
                                      QRegularExpression::CaseInsensitiveOption)
                       .match(s);
          if (!m.hasMatch())
            throw QString("ElseIf inválido.");
          expression(m.captured(1));
          b.start = code.size();
          append("if", m.captured(1), {}, {}, line);
        }
        continue;
      }
      if (lower == "end if" || lower == "endif") {
        if (blocks.isEmpty() || blocks.last().kind != "if")
          throw QString("End If sem If.");
        auto b = blocks.takeLast();
        if (b.start >= 0)
          code[b.start].jump = code.size();
        for (auto i : b.exits)
          code[i].jump = code.size();
        continue;
      }
      if (lower.startsWith("while ")) {
        expression(s.mid(6));
        blocks.append({"while", int(code.size()), {}});
        append("if", s.mid(6), {}, {}, line);
        continue;
      }
      if (lower == "end while" || lower == "endwhile") {
        if (blocks.isEmpty() || blocks.last().kind != "while")
          throw QString("End While sem While.");
        auto b = blocks.takeLast();
        append("jump", {}, {}, {}, line, b.start);
        code[b.start].jump = code.size();
        continue;
      }
      if (lower.startsWith("for ")) {
        auto m = QRegularExpression("^for +([a-z][a-z_0-9]*) *= *(.+?) +to "
                                    "+(.+?)(?: +step +(.+))?$",
                                    QRegularExpression::CaseInsensitiveOption)
                     .match(s);
        if (!m.hasMatch())
          throw QString("For inválido.");
        assign(m.captured(1), 0);
        expression(m.captured(2));
        expression(m.captured(3));
        auto step = m.captured(4).isEmpty() ? "1" : m.captured(4);
        expression(step);
        blocks.append({"for", int(code.size()), {}});
        append("forinit", m.captured(1), m.captured(2), {}, line);
        append("for", m.captured(1), m.captured(3), step, line);
        continue;
      }
      if (lower == "next" || lower.startsWith("next ")) {
        if (blocks.isEmpty() || blocks.last().kind != "for")
          throw QString("Next sem For.");
        auto b = blocks.takeLast();
        const auto loop = code[b.start + 1];
        if (lower != "next" &&
            s.mid(5).trimmed().toLower() != loop.arg.toLower())
          throw QString("Next de variável diferente.");
        append("fornext", loop.arg, {}, {}, line, b.start + 1);
        append("jump", {}, {}, {}, line, b.start + 1);
        code[b.start + 1].jump = code.size();
        continue;
      }
      if (lower.startsWith("goto ") || lower.startsWith("gosub ")) {
        append(lower.startsWith("goto") ? "goto" : "gosub",
               s.section(' ', 1).trimmed().toLower(), {}, {}, line);
        continue;
      }
      if (lower == "return" || lower == "terminate") {
        append(lower, {}, {}, {}, line);
        continue;
      }
      auto call = QRegularExpression("^([a-z][a-z_0-9]*)\\((.*)\\)$",
                                     QRegularExpression::CaseInsensitiveOption)
                      .match(s);
      if (call.hasMatch()) {
        auto name = call.captured(1).toLower();
        auto args = arguments(call.captured(2));
        if (name == "print") {
          for (auto a : args) {
            if (a.startsWith('"'))
              literal(a);
            else
              expression(a);
          }
          append("print", call.captured(2), {}, {}, line);
        } else if (name == "wait") {
          if (args.size() != 1)
            throw QString("Wait exige um argumento.");
          expression(args[0]);
          append("wait", args[0], {}, {}, line);
        } else {
          expression(s);
          append("call", s, {}, {}, line);
        }
        continue;
      }
      auto assignment = QRegularExpression("^(.+?) *= *(.+)$").match(s);
      if (assignment.hasMatch()) {
        assign(assignment.captured(1), 0);
        expression(assignment.captured(2));
        append("assign", assignment.captured(1), assignment.captured(2), {},
               line);
        continue;
      }
      throw QString("Instrução ainda não suportada: ") + s;
    }
    if (!blocks.isEmpty())
      throw QString("Bloco sem fechamento.");
    for (auto &i : code)
      if (i.kind == "goto" || i.kind == "gosub") {
        if (!labels.contains(i.arg))
          throw QString("Rótulo não encontrado: ") + i.arg;
        i.jump = labels[i.arg];
      }
    checking = false;
    values = {{"A 1", 58}, {"A 2", 51}, {"V 2", 508}};
    config = {{"ALIM 1", 200}, {"ALIM 2", 200}};
    return true;
  } catch (const QString &e) {
    checking = false;
    error = QString("Linha %1: %2").arg(line).arg(e);
    return false;
  }
}
bool BasicVm::step() {
  if (done())
    return false;
  const auto i = code[pc];
  try {
    if (++steps > 100000)
      throw QString("Limite local de 100.000 passos atingido.");
    int next = pc + 1;
    if (i.kind == "assign")
      assign(i.arg, expression(i.arg2));
    else if (i.kind == "if") {
      if (!expression(i.arg))
        next = i.jump;
    } else if (i.kind == "forinit") {
      assign(i.arg, expression(i.arg2));
      const auto guard = code[next];
      forBounds[next] = {expression(guard.arg2), expression(guard.arg3)};
    } else if (i.kind == "fornext") {
      assign(i.arg, wrap(qint64(variables.value(i.arg.toLower())) +
                         forBounds[i.jump].second));
    } else if (i.kind == "for") {
      auto value = variables.value(i.arg.toLower());
      auto limit = forBounds[pc].first, delta = forBounds[pc].second;
      if (!delta)
        throw QString("Step não pode ser zero.");
      if (delta > 0 ? value > limit : value < limit)
        next = i.jump;
    } else if (i.kind == "jump" || i.kind == "goto")
      next = i.jump;
    else if (i.kind == "gosub") {
      if (returns.size() >= 64)
        throw QString("GoSub aninhado demais.");
      returns << next;
      next = i.jump;
    } else if (i.kind == "return") {
      if (returns.isEmpty())
        throw QString("Return sem GoSub.");
      next = returns.takeLast();
    } else if (i.kind == "terminate")
      next = code.size();
    else if (i.kind == "call")
      expression(i.arg);
    else if (i.kind == "wait") {
      auto n = expression(i.arg);
      if (n < 0 || n > 3600000)
        throw QString("Wait fora do limite local.");
      milliseconds += n;
    } else if (i.kind == "print") {
      for (auto a : arguments(i.arg)) {
        if (a.startsWith('"')) {
          output += literal(a);
        } else {
          auto value = expression(a);
          output += expressionBoolean ? (value ? "True" : "False")
                                      : QString::number(value);
        }
      }
      if (output.size() > 65536)
        throw QString("Limite local de saída atingido.");
    }
    if (variables.size() > 4096)
      throw QString("Limite de variáveis atingido.");
    pc = next;
    return true;
  } catch (const QString &e) {
    error = QString("Linha %1: %2").arg(i.line).arg(e);
    return false;
  }
}
void BasicVm::run(int maximum) {
  for (int i = 0; i < qBound(0, maximum, 100000) && !done(); ++i)
    step();
}
