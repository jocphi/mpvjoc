#pragma once

#include <QColor>
#include <QString>

QColor codecBadgeColor(const QString &codec);
QString resolutionBadgeText(int videoHeight);
QColor resolutionBadgeColor(int videoHeight);
