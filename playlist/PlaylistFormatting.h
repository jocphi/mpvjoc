#pragma once
#include <QString>
#include <QtGlobal>

enum class PlaylistSizeUnit {
    Bytes = 0,
    Kilobytes = 1,
    Megabytes = 2,
    Gigabytes = 3,
    Terabytes = 4,
};

QString formatEuroSize(qint64 bytes, PlaylistSizeUnit unit);
QString playlistSizeUnitSuffix(PlaylistSizeUnit unit);
QString playlistSizeWidthSample(PlaylistSizeUnit unit);
PlaylistSizeUnit playlistSizeUnitFromInt(int value);
int playlistSizeUnitToInt(PlaylistSizeUnit unit);
PlaylistSizeUnit nextPlaylistSizeUnit(PlaylistSizeUnit unit);
