#include "FamilyFilenameParser.h"

#include <QFileInfo>
#include <QRegularExpression>

bool FamilyFilenameParser::parse(const QString& fileName,
                                 FamilyFilenameMatch& match,
                                 QString* errorOut)
{
    if (errorOut)
        errorOut->clear();

    const QString baseName = QFileInfo(fileName).fileName();
    static const QRegularExpression pattern(
        QStringLiteral(R"(^(.+)\.(\d{4})\.(\d{2})\.(\d{2})\.(.+)$)"));

    const QRegularExpressionMatch result = pattern.match(baseName);
    if (!result.hasMatch()) {
        if (errorOut) {
            *errorOut = QStringLiteral(
                "Filename does not match FamilyDescriptor.yyyy.mm.dd.*:\n%1")
                            .arg(baseName);
        }
        return false;
    }

    const QString descriptor = result.captured(1);
    const int year = result.captured(2).toInt();
    const int month = result.captured(3).toInt();
    const int day = result.captured(4).toInt();
    const QDate date(year, month, day);

    if (descriptor.isEmpty() || !date.isValid()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Filename contains an empty descriptor or invalid date:\n%1")
                            .arg(baseName);
        }
        return false;
    }

    match.descriptor = descriptor;
    match.date = date;
    match.remainder = result.captured(5);
    return true;
}
