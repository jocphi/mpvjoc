#include "MetadataProbeManager.h"
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <QtGlobal>

MetadataProbeManager::MetadataProbeManager(QObject*p):QObject(p){}

void MetadataProbeManager::enqueue(const QString&p){ if(p.isEmpty()||queued.contains(p)||running.contains(p)) return; queued.insert(p); q.enqueue(p); pump(); }

QString MetadataProbeManager::norm(QString c){ c=c.toLower(); if(c=="h264"||c=="avc1")return"H.264"; if(c=="hevc"||c=="h265")return"HEVC"; if(c=="av1")return"AV1"; if(c=="vp9")return"VP9"; if(c=="vp8")return"VP8"; if(c=="aac")return"AAC"; if(c=="mp3")return"MP3"; if(c=="flac")return"FLAC"; if(c=="opus")return"OPUS"; return c.isEmpty()?"?":c.toUpper(); }

MediaMetadata MetadataProbeManager::parse(const QByteArray&ba){ MediaMetadata m; auto d=QJsonDocument::fromJson(ba); if(!d.isObject())return m; auto root=d.object(); auto fmt=root.value("format").toObject(); double fd=fmt.value("duration").toString().toDouble(); if(fd>0){m.duration=fd;m.durationKnown=true;} QString fn=fmt.value("format_name").toString(); if(!fn.isEmpty())m.container=fn.section(',',0,0); QString firstAudio; for(auto v: root.value("streams").toArray()){ auto s=v.toObject(); QString type=s.value("codec_type").toString(); if(type=="video"&&m.codec.isEmpty()){ m.codec=norm(s.value("codec_name").toString()); int w=s.value("width").toInt(),h=s.value("height").toInt(); if(w>0&&h>0)m.resolution=QString::number(w)+"x"+QString::number(h); } else if(type=="audio"&&firstAudio.isEmpty()) firstAudio=norm(s.value("codec_name").toString()); } if(m.codec.isEmpty()&&!firstAudio.isEmpty()){m.codec=firstAudio;m.resolution="audio";} if(m.codec.isEmpty())m.codec="?"; if(m.resolution.isEmpty())m.resolution="?"; if(m.container.isEmpty())m.container="?"; return m; }

void MetadataProbeManager::pump(){ int maxJobs=qMax(2,QThread::idealThreadCount()/2); while(active<maxJobs&&!q.isEmpty()){ QString p=q.dequeue(); queued.remove(p); running.insert(p); active++; auto*pr=new QProcess(this); pr->setProgram("ffprobe"); pr->setArguments({"-v","error","-print_format","json","-show_format","-show_streams",p}); connect(pr,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,pr,p](int code,QProcess::ExitStatus st){ QByteArray out=pr->readAllStandardOutput(); pr->deleteLater(); running.remove(p); active--; if(st==QProcess::NormalExit&&code==0){ if(metadataReady) metadataReady(p,parse(out)); } else if(metadataFailed) metadataFailed(p); pump();}); pr->start(); }}
