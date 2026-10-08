#pragma once
#include "catalog.h"
#include <QJsonObject>

namespace ConfigText {
QJsonObject dictionary();
QString name(const ConfigSpec &spec, int index);
QString description(const ConfigSpec &spec, int index);
QString category(const QString &original);
QString categoryDescription(const QString &original);
QString choice(const QString &original);
QString tooltip(const ConfigSpec &spec, int index);
QString categoryTooltip(const QString &original);
} // namespace ConfigText
