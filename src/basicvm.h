#pragma once
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
class BasicVm {
public:
  bool compile(const QString &source);
  bool step();
  void run(int maximumSteps = 10000);
  bool done() const { return pc >= code.size() || !error.isEmpty(); }
  int line() const { return pc < code.size() ? code[pc].line : 0; }
  QString error, output;
  qint64 milliseconds = 0;
  QMap<QString, qint32> variables, values, config, commands;
  void setInput(const QString &key, int index, qint32 value);

private:
  struct Instruction {
    QString kind, arg, arg2, arg3;
    int line = 0, jump = -1;
  };
  QVector<Instruction> code;
  QMap<QString, int> arrays, labels, symbols;
  QSet<QString> booleanVariables;
  bool expressionBoolean = false;
  QMap<int, QString> symbolNames;
  QVector<int> returns;
  QMap<int, qint32> timers;
  QMap<int, QPair<qint32, qint32>> forBounds;
  QSet<QString> constants;
  QMap<int, bool> timerRunning;
  QMap<int, qint64> timerStamp;
  int pc = 0, steps = 0;
  bool explicitVariables = false, checking = false;
  qint32 expression(const QString &text);
  QStringList arguments(const QString &text);
  void assign(const QString &target, qint32 value);
  qint32 function(const QString &name, const QVector<qint32> &args);
};
