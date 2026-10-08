#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

// Profiles use Motor 1 as a canonical index space. Mapping uses the recovered
// RoboRun paths, not parity guesses: PID, Hall, SinCos and OTL differ.
namespace MotorProfiles {
struct Binding {
  QString key, canonicalQuery;
  int first=0, second=0;
  bool calibration=false, encoder=false;
};
const QVector<Binding> &bindings();
QJsonObject capture(const QJsonArray &values, int channel, const QString &name,
                    const QString &model, const QString &firmware,
                    const QString &source, QString *error);
QJsonArray plan(const QJsonObject &profile, int channel, bool calibration,
                const QString &model, const QString &firmware,
                bool real, QString *error);
QJsonArray library(const QString &directory=QString());
QString directory();
bool save(QJsonObject profile, QString *error, const QString &directory=QString());
bool remove(const QString &id, QString *error, const QString &directory=QString());
}
