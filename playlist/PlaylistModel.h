#pragma once
#include <QAbstractListModel>
#include <QVector>
#include <QJsonArray>
#include <QPair>
#include "media/MetadataProbeManager.h"

struct PlaylistItem{ QString path,title; qint64 sizeBytes=0; double duration=0; bool durationKnown=false; QString codec,resolution,container; bool metadataProbed=false; QString thumbnailPath; bool thumbnailReady=false; bool thumbnailAttempted=false; bool reviewed=false; };

class PlaylistModel: public QAbstractListModel{
    Q_OBJECT
public:
    enum Roles{PathRole=Qt::UserRole+1,TitleRole,SizeRole,DurationRole,DurationKnownRole,CodecRole,ResolutionRole,ContainerRole,MetadataProbedRole,ThumbnailPathRole,ThumbnailReadyRole,ThumbnailAttemptedRole,FolderDropGroupRole,FolderDropGroupFirstRole,FolderDropGroupLastRole,ReviewedRole};
    explicit PlaylistModel(QObject*p=nullptr);
    int rowCount(const QModelIndex&p=QModelIndex())const override;
    QVariant data(const QModelIndex&i,int role=Qt::DisplayRole)const override;
    Qt::ItemFlags flags(const QModelIndex&i)const override;
    Qt::DropActions supportedDropActions()const override;
    Qt::DropActions supportedDragActions()const override;
    QStringList mimeTypes()const override;
    QMimeData*mimeData(const QModelIndexList&idxs)const override;
    bool dropMimeData(const QMimeData*d,Qt::DropAction a,int row,int,const QModelIndex&parent)override;
    bool moveRows(const QModelIndex&,int src,int count,const QModelIndex&,int dest)override;
    bool moveRowTo(int src,int final);
    QStringList addFiles(const QStringList&files);
    QStringList addFolderGroup(const QStringList&files);
    void clearFolderDropGroups();
    QString pathAt(int r)const;
    int count()const;
    QStringList pathsNeedingProbe()const;
    QStringList pathsNeedingThumbnail()const;
    bool removeRowAt(int r);
    void clearItems();
    bool moveRowUp(int r);
    bool moveRowDown(int r);
    bool toggleReviewedAt(int r);
    void resetThumbnailAttempts();
    void setMetadataForPath(const QString&p,const MediaMetadata&m);
    void markProbeFailed(const QString&p);
    void setThumbnailForPath(const QString&p,const QString&t);
    void markThumbnailFailed(const QString&p);
    void resetMetadataForPath(const QString&p);
    void resetThumbnailAttemptForPath(const QString&p);
    void setDurationForPath(const QString&p,double d);
    QJsonArray toJson()const;
    void fromJson(const QJsonArray&a);
private:
    QVector<PlaylistItem>items;
    QVector<QPair<int,int>>folderDropGroups;
};
