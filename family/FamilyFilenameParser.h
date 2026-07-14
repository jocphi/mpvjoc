#pragma once

#include <QDate>
#include <QString>

struct FamilyFilenameMatch {
    QString descriptor;
    QDate date;
    QString remainder;
};

class FamilyFilenameParser {
public:
    // Matches: FamilyDescriptor.yyyy.mm.dd.*
    // The date must be a valid calendar date.
    static bool parse(const QString& fileName,
                      FamilyFilenameMatch& match,
                      QString* errorOut = nullptr);
};
