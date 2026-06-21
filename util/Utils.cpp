#include "Utils.h"
#include <QChar>
#include <QtGlobal>
#include <clocale>
#include <mpv/client.h>

void forceCLocaleForMpv(){ std::setlocale(LC_NUMERIC,"C"); }
void check_mpv_error(int s){ if(s<0) qFatal("mpv error: %s", mpv_error_string(s)); }
QString formatHMS(double sec){ if(sec<0) sec=0; int t=int(sec+0.5); return QString("%1:%2:%3").arg(t/3600,2,10,QChar('0')).arg((t%3600)/60,2,10,QChar('0')).arg(t%60,2,10,QChar('0')); }
QString formatBytes(qint64 b){ return b>0 ? QString::number(b/(1024.0*1024.0),'f',2)+" MB" : QStringLiteral("?.?? MB"); }
