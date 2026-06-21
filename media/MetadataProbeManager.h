#pragma once
#include <QObject>
#include <QString>
#include <QQueue>
#include <QSet>
#include <functional>

struct MediaMetadata{
    double duration=0;
    bool durationKnown=false;
    QString codec,resolution,container;
};

class MetadataProbeManager: public QObject{
    Q_OBJECT
public:
    explicit MetadataProbeManager(QObject*p=nullptr);
    std::function<void(const QString&,const MediaMetadata&)> metadataReady;
    std::function<void(const QString&)> metadataFailed;
    void enqueue(const QString&p);
private:
    static QString norm(QString c);
    static MediaMetadata parse(const QByteArray&ba);
    void pump();
    QQueue<QString>q;
    QSet<QString>queued,running;
    int active=0;
};
