#include "FamilyDestinationRepository.h"

#include <QFile>
#include <QFileInfo>
#include <QString>
#include <sqlite3.h>

namespace {
QString sqliteError(sqlite3* database, const QString& prefix)
{
    const char* message = database ? sqlite3_errmsg(database) : nullptr;
    return message
        ? QStringLiteral("%1: %2").arg(prefix, QString::fromUtf8(message))
        : prefix;
}

QString columnText(sqlite3_stmt* statement, int column)
{
    const auto* text = sqlite3_column_text(statement, column);
    return text ? QString::fromUtf8(reinterpret_cast<const char*>(text)) : QString();
}
}

bool FamilyDestinationRepository::loadReadOnly(const QString& databasePath,
                                               QVector<FamilyDestination>& destinations,
                                               QString* errorOut)
{
    if (errorOut)
        errorOut->clear();

    const QFileInfo databaseInfo(databasePath);
    if (!databaseInfo.exists() || !databaseInfo.isFile() || !databaseInfo.isReadable()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Everything-rust database is not a readable file: %1")
                            .arg(QFileInfo(databasePath).absoluteFilePath());
        }
        return false;
    }

    sqlite3* database = nullptr;
    const QByteArray encodedPath = QFile::encodeName(databaseInfo.absoluteFilePath());
    const int openResult = sqlite3_open_v2(encodedPath.constData(),
                                           &database,
                                           SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX,
                                           nullptr);
    if (openResult != SQLITE_OK) {
        if (errorOut)
            *errorOut = sqliteError(database, QStringLiteral("Could not open everything-rust database read-only"));
        if (database)
            sqlite3_close(database);
        return false;
    }

    sqlite3_busy_timeout(database, 5000);

    static constexpr const char* query = R"SQL(
SELECT
    descriptors.family_descriptor,
    descriptors.folder_path,
    CASE
        WHEN files.id IS NULL THEN 0
        ELSE 1
    END AS indexed,
    descriptors.created_at,
    descriptors.updated_at
FROM folder_family_descriptors AS descriptors
LEFT JOIN files
    ON files.path = descriptors.folder_path
   AND files.is_dir != 0
ORDER BY descriptors.family_descriptor COLLATE NOCASE,
         descriptors.folder_path
)SQL";

    sqlite3_stmt* statement = nullptr;
    const int prepareResult = sqlite3_prepare_v2(database, query, -1, &statement, nullptr);
    if (prepareResult != SQLITE_OK) {
        if (errorOut)
            *errorOut = sqliteError(database, QStringLiteral("Could not prepare family-destination query"));
        sqlite3_close(database);
        return false;
    }

    QVector<FamilyDestination> loaded;
    int stepResult = SQLITE_ROW;
    while ((stepResult = sqlite3_step(statement)) == SQLITE_ROW) {
        FamilyDestination destination;
        destination.descriptor = columnText(statement, 0);
        destination.folderPath = columnText(statement, 1);
        destination.indexed = sqlite3_column_int(statement, 2) != 0;
        destination.createdAt = sqlite3_column_int64(statement, 3);
        destination.updatedAt = sqlite3_column_int64(statement, 4);
        destination.available = QFileInfo(destination.folderPath).isDir();

        if (!destination.descriptor.isEmpty() && !destination.folderPath.isEmpty())
            loaded.push_back(destination);
    }

    if (stepResult != SQLITE_DONE) {
        if (errorOut)
            *errorOut = sqliteError(database, QStringLiteral("Could not read family-destination rows"));
        sqlite3_finalize(statement);
        sqlite3_close(database);
        return false;
    }

    sqlite3_finalize(statement);
    sqlite3_close(database);

    destinations = std::move(loaded);
    return true;
}
