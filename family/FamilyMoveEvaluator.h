#pragma once

#include "FamilyDestinationRepository.h"
#include <QString>
#include <QVector>

struct FamilyMoveEvaluation {
    enum class FolderState { Missing, Offline, Online };
    enum class MoveState { Blocked, Offline, Ready };

    FolderState folderState = FolderState::Missing;
    MoveState moveState = MoveState::Blocked;
    QString descriptor;
    QString destinationFolder;
    QString destinationPath;
    QString reason;
    qint64 availableBytes = -1;
};

class FamilyMoveEvaluator {
public:
    static FamilyMoveEvaluation evaluate(
        const QString& sourcePath,
        const QVector<FamilyDestination>& destinations);
};
