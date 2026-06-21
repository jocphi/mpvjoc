#include "ThumbnailManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QFile>
#include <QProcess>
#include <QThread>
#include <QtGlobal>

ThumbnailManager::ThumbnailManager(QObject*p):QObject(p){ QString base=QStandardPaths::writableLocation(QStandardPaths::CacheLocation); if(base.isEmpty())base=QDir::homePath()+"/.cache/mpvjoc"; cache=QDir(base).filePath("thumbnails"); QDir().mkpath(cache); }
void ThumbnailManager::enqueue(const QString&p){ if(p.isEmpty())return; QString out=thumbPath(p); if(QFileInfo::exists(out)){if(thumbnailReady)thumbnailReady(p,out);return;} if(queued.contains(p)||running.contains(p))return; queued.insert(p); q.enqueue(p); pump(); }
QString ThumbnailManager::thumbPath(const QString&p)const{ return QDir(cache).filePath(QString::fromLatin1(QCryptographicHash::hash(p.toUtf8(),QCryptographicHash::Sha1).toHex())+".jpg"); }
void ThumbnailManager::pump(){ int maxJobs=qMax(2,QThread::idealThreadCount()/2); while(active<maxJobs&&!q.isEmpty()){ QString p=q.dequeue(); queued.remove(p); running.insert(p); active++; QString out=thumbPath(p); auto*pr=new QProcess(this); pr->setProgram("ffmpeg"); pr->setArguments({"-hide_banner","-loglevel","error","-y","-ss","10","-i",p,"-frames:v","1","-an","-vf","scale=128:72:force_original_aspect_ratio=decrease,pad=128:72:(ow-iw)/2:(oh-ih)/2:black",out}); connect(pr,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,pr,p,out](int code,QProcess::ExitStatus st){ pr->deleteLater(); running.remove(p); active--; if(st==QProcess::NormalExit&&code==0&&QFileInfo::exists(out)){if(thumbnailReady)thumbnailReady(p,out);} else {QFile::remove(out); if(thumbnailFailed)thumbnailFailed(p);} pump();}); pr->start(); }}
