#pragma once
#include <QString>

void forceCLocaleForMpv();
void check_mpv_error(int status);
QString formatHMS(double sec);
QString formatBytes(qint64 bytes);
