#pragma once

#include <QColor>
#include <QModelIndex>

struct DuplicateDurationRunInfo {
    bool isDuplicate = false;
    QColor color;
};

DuplicateDurationRunInfo duplicateDurationRunInfo(const QModelIndex &index, bool durationKnown, double durationSeconds);
