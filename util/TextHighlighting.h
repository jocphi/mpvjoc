#pragma once

#include <QColor>
#include <QPainter>
#include <QRect>
#include <QString>

void drawHighlightedTitle(QPainter *painter, const QRect &rect, const QString &title, const QColor &defaultColor = QColor(Qt::white));
