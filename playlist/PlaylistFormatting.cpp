#include "PlaylistFormatting.h"

#include <QChar>

QString formatEuroSize(qint64 bytes, bool gb)
{
    if (bytes <= 0) {
        return gb ? QStringLiteral("???,?? GB") : QStringLiteral("???,?? MB");
    }

    const double value = gb
        ? bytes / (1024.0 * 1024.0 * 1024.0)
        : bytes / (1024.0 * 1024.0);

    const qint64 cents = qint64(value * 100.0 + 0.5);
    const qint64 whole = cents / 100;
    const int fraction = int(cents % 100);

    QString wholeText = QString::number(whole);
    for (int pos = wholeText.size() - 3; pos > 0; pos -= 3) {
        wholeText.insert(pos, QChar('.'));
    }

    return QString("%1,%2 %3")
        .arg(wholeText)
        .arg(fraction, 2, 10, QChar('0'))
        .arg(gb ? QStringLiteral("GB") : QStringLiteral("MB"));
}
