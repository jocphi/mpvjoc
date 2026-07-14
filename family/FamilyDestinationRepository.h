#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>

struct FamilyDestination {
    QString descriptor;
    QString folderPath;
    bool indexed = false;
    bool available = false;
    qint64 createdAt = 0;
    qint64 updatedAt = 0;
};

class FamilyDestinationRepository {
public:
    // Strictly read-only. On failure, `destinations` is left unchanged.
    static bool loadReadOnly(const QString& databasePath,
                             QVector<FamilyDestination>& destinations,
                             QString* errorOut = nullptr);
};
