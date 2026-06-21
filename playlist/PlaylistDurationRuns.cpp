#include "PlaylistDurationRuns.h"

#include "PlaylistModel.h"

static int roundedDurationSeconds(const QModelIndex &index)
{
    return int(index.data(PlaylistModel::DurationRole).toDouble() + 0.5);
}

DuplicateDurationRunInfo duplicateDurationRunInfo(const QModelIndex &index, bool durationKnown, double durationSeconds)
{
    if (!durationKnown) {
        return {};
    }

    const int thisDuration = int(durationSeconds + 0.5);
    if (thisDuration < 0) {
        return {};
    }

    int runStart = index.row();
    int runEnd = index.row();

    while (runStart > 0) {
        const auto previousIndex = index.sibling(runStart - 1, 0);
        if (!previousIndex.isValid()
            || !previousIndex.data(PlaylistModel::DurationKnownRole).toBool()
            || roundedDurationSeconds(previousIndex) != thisDuration) {
            break;
        }
        --runStart;
    }

    while (true) {
        const auto nextIndex = index.sibling(runEnd + 1, 0);
        if (!nextIndex.isValid()
            || !nextIndex.data(PlaylistModel::DurationKnownRole).toBool()
            || roundedDurationSeconds(nextIndex) != thisDuration) {
            break;
        }
        ++runEnd;
    }

    const int runLength = runEnd - runStart + 1;
    if (runLength < 2) {
        return {};
    }

    int duplicateRunIndex = 0;
    int scan = 0;

    while (scan < runStart) {
        const auto scanIndex = index.sibling(scan, 0);
        if (!scanIndex.isValid()) {
            ++scan;
            continue;
        }

        const bool scanKnown = scanIndex.data(PlaylistModel::DurationKnownRole).toBool();
        const int scanDuration = scanKnown ? roundedDurationSeconds(scanIndex) : -1;
        int scanEnd = scan;

        if (scanKnown) {
            while (true) {
                const auto nextIndex = index.sibling(scanEnd + 1, 0);
                if (!nextIndex.isValid()
                    || !nextIndex.data(PlaylistModel::DurationKnownRole).toBool()
                    || roundedDurationSeconds(nextIndex) != scanDuration) {
                    break;
                }
                ++scanEnd;
            }
        }

        if (scanKnown && scanEnd > scan) {
            ++duplicateRunIndex;
        }

        scan = scanEnd + 1;
    }

    DuplicateDurationRunInfo info;
    info.isDuplicate = true;
    info.color = (duplicateRunIndex % 2) == 0
        ? QColor(18, 45, 82)
        : QColor(58, 42, 86);
    return info;
}
