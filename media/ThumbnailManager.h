#pragma once
#include <QObject>
#include <QString>
#include <QQueue>
#include <QSet>
#include <functional>

class ThumbnailManager: public QObject{
    Q_OBJECT
public:
    explicit ThumbnailManager(QObject*p=nullptr);
    std::function<void(const QString&,const QString&)> thumbnailReady;
    std::function<void(const QString&)> thumbnailFailed;
    void enqueue(const QString&p);
private:
    QString thumbPath(const QString&p)const;
    void pump();
    QString cache;
    QQueue<QString>q;
    QSet<QString>queued,running;
    int active=0;
};
