#include "PlaylistBadges.h"

#include <QtGlobal>

QColor codecBadgeColor(const QString &codec)
{
    const QString key = codec.toUpper();

    if (key == "H.264" || key == "AVC") {
        return QColor(38, 96, 132);
    }
    if (key == "HEVC" || key == "H.265") {
        return QColor(84, 70, 150);
    }
    if (key == "AV1") {
        return QColor(142, 64, 112);
    }
    if (key == "VP9") {
        return QColor(46, 116, 86);
    }
    if (key == "VP8") {
        return QColor(82, 124, 58);
    }
    if (key == "MPEG4" || key == "MPEG-4") {
        return QColor(138, 92, 42);
    }
    if (key == "AAC") {
        return QColor(118, 88, 38);
    }
    if (key == "MP3") {
        return QColor(116, 64, 38);
    }
    if (key == "FLAC") {
        return QColor(38, 112, 120);
    }
    if (key == "OPUS") {
        return QColor(98, 76, 130);
    }

    return QColor(72, 72, 72);
}

QString resolutionBadgeText(int videoHeight)
{
    if (videoHeight <= 0) {
        return {};
    }

    const int tier = videoHeight >= 2160
        ? 2160
        : (videoHeight >= 1440 ? 1440 : (videoHeight >= 1080 ? 1080 : (videoHeight >= 720 ? 720 : 480)));

    return QString::number(tier) + "p";
}

QColor resolutionBadgeColor(int videoHeight)
{
    auto lerp = [](int a, int b, double t) {
        return int(a + (b - a) * qBound(0.0, t, 1.0) + 0.5);
    };

    auto mix = [&](QColor a, QColor b, double t) {
        return QColor(lerp(a.red(), b.red(), t), lerp(a.green(), b.green(), t), lerp(a.blue(), b.blue(), t));
    };

    if (videoHeight < 700) {
        const double t = (qBound(80, videoHeight, 699) - 80) / double(699 - 80);
        const double peak = (480 - 80) / double(699 - 80);
        return t <= peak ? mix(QColor(70, 0, 0), QColor(255, 0, 0), t / peak)
                         : mix(QColor(255, 0, 0), QColor(255, 150, 150), (t - peak) / (1.0 - peak));
    }

    if (videoHeight < 1000) {
        const double t = (videoHeight - 700) / double(999 - 700);
        const double peak = (720 - 700) / double(999 - 700);
        return t <= peak ? mix(QColor(0, 70, 0), QColor(0, 255, 0), t / peak)
                         : mix(QColor(0, 255, 0), QColor(170, 255, 170), (t - peak) / (1.0 - peak));
    }

    if (videoHeight < 2000) {
        const double t = (videoHeight - 1000) / double(1999 - 1000);
        const double peak = (1080 - 1000) / double(1999 - 1000);
        return t <= peak ? mix(QColor(0, 30, 90), QColor(0, 170, 255), t / peak)
                         : mix(QColor(0, 170, 255), QColor(170, 220, 255), (t - peak) / (1.0 - peak));
    }

    const int capped = qMin(videoHeight, 4320);
    const double t = (capped - 2000) / double(4320 - 2000);
    const double peak = (2160 - 2000) / double(4320 - 2000);

    return t <= peak ? mix(QColor(65, 0, 90), QColor(210, 90, 255), t / peak)
                     : mix(QColor(210, 90, 255), QColor(235, 190, 255), (t - peak) / (1.0 - peak));
}
