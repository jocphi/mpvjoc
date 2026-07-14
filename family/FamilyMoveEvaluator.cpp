#include "FamilyMoveEvaluator.h"
#include "FamilyFilenameParser.h"

#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>

FamilyMoveEvaluation FamilyMoveEvaluator::evaluate(
    const QString& sourcePath,
    const QVector<FamilyDestination>& destinations)
{
    FamilyMoveEvaluation evaluation;
    const QFileInfo sourceInfo(sourcePath);

    FamilyFilenameMatch filenameMatch;
    QString parseError;
    if (!FamilyFilenameParser::parse(sourceInfo.fileName(), filenameMatch, &parseError)) {
        evaluation.reason = parseError;
        return evaluation;
    }
    evaluation.descriptor = filenameMatch.descriptor;

    const FamilyDestination* matched = nullptr;
    for (const FamilyDestination& candidate : destinations) {
        if (QString::compare(candidate.descriptor, filenameMatch.descriptor, Qt::CaseInsensitive) == 0) {
            matched = &candidate;
            break;
        }
    }
    if (!matched) {
        evaluation.reason = QStringLiteral("No family destination is defined for: %1")
                                .arg(filenameMatch.descriptor);
        return evaluation;
    }

    evaluation.descriptor = matched->descriptor;
    evaluation.destinationFolder = matched->folderPath;
    const QFileInfo folderInfo(matched->folderPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        evaluation.folderState = FamilyMoveEvaluation::FolderState::Offline;
        evaluation.moveState = FamilyMoveEvaluation::MoveState::Offline;
        evaluation.reason = QStringLiteral("Family destination is offline or unavailable: %1")
                                .arg(matched->folderPath);
        return evaluation;
    }

    evaluation.folderState = FamilyMoveEvaluation::FolderState::Online;
    evaluation.destinationPath = QDir(folderInfo.absoluteFilePath()).filePath(sourceInfo.fileName());

    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        evaluation.reason = QStringLiteral("Source is not an existing regular file: %1")
                                .arg(sourcePath);
        return evaluation;
    }
    if (!folderInfo.isWritable()) {
        evaluation.reason = QStringLiteral("Family destination is not writable: %1")
                                .arg(folderInfo.absoluteFilePath());
        return evaluation;
    }
    if (QFileInfo::exists(evaluation.destinationPath)) {
        evaluation.reason = QStringLiteral("Destination already exists: %1")
                                .arg(evaluation.destinationPath);
        return evaluation;
    }
    if (sourceInfo.absoluteFilePath() == QFileInfo(evaluation.destinationPath).absoluteFilePath()) {
        evaluation.reason = QStringLiteral("Source and destination are the same file.");
        return evaluation;
    }

    QStorageInfo storage(folderInfo.absoluteFilePath());
    storage.refresh();
    if (!storage.isValid() || !storage.isReady()) {
        evaluation.reason = QStringLiteral("Could not determine free space for: %1")
                                .arg(folderInfo.absoluteFilePath());
        return evaluation;
    }
    evaluation.availableBytes = storage.bytesAvailable();
    if (evaluation.availableBytes < sourceInfo.size()) {
        evaluation.reason = QStringLiteral(
            "Not enough free space in family destination. Required: %1 bytes, available: %2 bytes")
                                .arg(sourceInfo.size())
                                .arg(evaluation.availableBytes);
        return evaluation;
    }

    evaluation.moveState = FamilyMoveEvaluation::MoveState::Ready;
    return evaluation;
}
