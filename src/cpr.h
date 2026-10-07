#pragma once
#include <QByteArray>
#include <QString>
#include <QVector>
struct CprItem {
  QString tree, command, node, type, value;
  int index = 1, multiplier = 1;
  QString identity() const { return command + " " + QString::number(index); }
};
struct CprProfile {
  QString firmware, roborun, controlTree, powerTree, error;
  QVector<CprItem> items;
  bool valid() const { return error.isEmpty() && !items.isEmpty(); }
  static CprProfile parse(const QByteArray &data);
  QByteArray xml() const;
};
