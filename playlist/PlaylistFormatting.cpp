#include "PlaylistFormatting.h"

#include <QtGlobal>

static QString groupedWholeNumber(qint64 whole)
{
    QString text = QString::number(whole);
    for (int pos = text.size() - 3; pos > 0; pos -= 3)
        text.insert(pos, QChar('.'));
    return text;
}

static QString groupedFraction(QString fraction)
{
    for (int pos = 3; pos < fraction.size(); pos += 4)
        fraction.insert(pos, QChar('.'));
    return fraction;
}

static QString formatDecimalWithGroupedFraction(qint64 scaled, int decimals, const QString& suffix, bool lessThan)
{
    qint64 divisor = 1;
    for (int i = 0; i < decimals; ++i)
        divisor *= 10;

    const qint64 whole = scaled / divisor;
    const qint64 frac = scaled % divisor;
    QString fracText = QString::number(frac).rightJustified(decimals, QChar('0'));

    const QString number = QStringLiteral("%1,%2").arg(groupedWholeNumber(whole), groupedFraction(fracText));
    return QStringLiteral("%1%2 %3").arg(lessThan ? QStringLiteral("<") : QString(), number, suffix);
}

static double divisorForUnit(PlaylistSizeUnit unit)
{
    switch (unit) {
    case PlaylistSizeUnit::Bytes:
        return 1.0;
    case PlaylistSizeUnit::Kilobytes:
        return 1024.0;
    case PlaylistSizeUnit::Megabytes:
        return 1024.0 * 1024.0;
    case PlaylistSizeUnit::Gigabytes:
        return 1024.0 * 1024.0 * 1024.0;
    case PlaylistSizeUnit::Terabytes:
        return 1024.0 * 1024.0 * 1024.0 * 1024.0;
    }
    return 1024.0 * 1024.0;
}

QString playlistSizeUnitSuffix(PlaylistSizeUnit unit)
{
    switch (unit) {
    case PlaylistSizeUnit::Bytes:
        return QStringLiteral("B");
    case PlaylistSizeUnit::Kilobytes:
        return QStringLiteral("KB");
    case PlaylistSizeUnit::Megabytes:
        return QStringLiteral("MB");
    case PlaylistSizeUnit::Gigabytes:
        return QStringLiteral("GB");
    case PlaylistSizeUnit::Terabytes:
        return QStringLiteral("TB");
    }
    return QStringLiteral("MB");
}

PlaylistSizeUnit playlistSizeUnitFromInt(int value)
{
    switch (value) {
    case 0:
        return PlaylistSizeUnit::Bytes;
    case 1:
        return PlaylistSizeUnit::Kilobytes;
    case 2:
        return PlaylistSizeUnit::Megabytes;
    case 3:
        return PlaylistSizeUnit::Gigabytes;
    case 4:
        return PlaylistSizeUnit::Terabytes;
    default:
        return PlaylistSizeUnit::Megabytes;
    }
}

int playlistSizeUnitToInt(PlaylistSizeUnit unit)
{
    return static_cast<int>(unit);
}

PlaylistSizeUnit nextPlaylistSizeUnit(PlaylistSizeUnit unit)
{
    return playlistSizeUnitFromInt((playlistSizeUnitToInt(unit) + 1) % 5);
}

QString playlistSizeWidthSample(PlaylistSizeUnit unit)
{
    switch (unit) {
    case PlaylistSizeUnit::Bytes:
        return QStringLiteral("000.000.000.000 B");
    case PlaylistSizeUnit::Kilobytes:
        return QStringLiteral("000.000.000,00 KB");
    case PlaylistSizeUnit::Megabytes:
        return QStringLiteral("000.000,00 MB");
    case PlaylistSizeUnit::Gigabytes:
        return QStringLiteral("000.000,00 GB");
    case PlaylistSizeUnit::Terabytes:
        return QStringLiteral("<0,000.000.001 TB");
    }
    return QStringLiteral("000.000,00 MB");
}

QString formatEuroSize(qint64 bytes, PlaylistSizeUnit unit)
{
    const QString suffix = playlistSizeUnitSuffix(unit);

    if (bytes < 0)
        return QStringLiteral("? %1").arg(suffix);

    if (unit == PlaylistSizeUnit::Bytes)
        return QStringLiteral("%1 %2").arg(groupedWholeNumber(bytes), suffix);

    const double value = bytes / divisorForUnit(unit);

    if (bytes == 0) {
        return QStringLiteral("0,00 %1").arg(suffix);
    }

    // Normal display: two decimals, preserving the historical playlist look.
    qint64 cents = qint64(value * 100.0 + 0.5);
    if (cents > 0) {
        const qint64 whole = cents / 100;
        const int frac = int(cents % 100);
        return QStringLiteral("%1,%2 %3")
            .arg(groupedWholeNumber(whole))
            .arg(frac, 2, 10, QChar('0'))
            .arg(suffix);
    }

    // Tiny non-zero value: expand decimal precision up to 9 fractional digits.
    // Counting the leading 0 before the comma plus 9 fractional digits gives a
    // maximum of 10 displayed digits, e.g. 0,000.000.001.
    for (int decimals = 3; decimals <= 9; ++decimals) {
        qint64 scale = 1;
        for (int i = 0; i < decimals; ++i)
            scale *= 10;
        const qint64 scaled = qint64(value * double(scale) + 0.5);
        if (scaled > 0)
            return formatDecimalWithGroupedFraction(scaled, decimals, suffix, false);
    }

    return formatDecimalWithGroupedFraction(1, 9, suffix, true);
}
