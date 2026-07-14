#include "PlaylistModel.h"
#include <QMimeData>
#include <QFileInfo>
#include <QJsonObject>
#include <QSet>
#include <QtGlobal>

PlaylistModel::PlaylistModel(QObject*p):QAbstractListModel(p){}
int PlaylistModel::rowCount(const QModelIndex&p)const{return p.isValid()?0:items.size();}
QVariant PlaylistModel::data(const QModelIndex&i,int role)const{ if(!i.isValid()||i.row()<0||i.row()>=items.size())return{}; const auto&it=items[i.row()]; const bool inFolderGroup=!it.folderDropRoot.isEmpty(); const bool firstInFolderGroup=inFolderGroup&&(i.row()==0||items[i.row()-1].folderDropRoot!=it.folderDropRoot); const bool lastInFolderGroup=inFolderGroup&&(i.row()==items.size()-1||items[i.row()+1].folderDropRoot!=it.folderDropRoot); switch(role){case Qt::DisplayRole:case TitleRole:return it.title;case Qt::ToolTipRole:return QVariant();case PathRole:return it.path;case SizeRole:return it.sizeBytes;case DurationRole:return it.duration;case DurationKnownRole:return it.durationKnown;case CodecRole:return it.codec;case ResolutionRole:return it.resolution;case ContainerRole:return it.container;case MetadataProbedRole:return it.metadataProbed;case ThumbnailPathRole:return it.thumbnailPath;case ThumbnailReadyRole:return it.thumbnailReady;case ThumbnailAttemptedRole:return it.thumbnailAttempted;case ReviewedRole:return it.reviewed;case FamilyMoveProgressRole:return it.familyMoveProgress;case FolderDropGroupRole:return inFolderGroup;case FolderDropGroupFirstRole:return firstInFolderGroup;case FolderDropGroupLastRole:return lastInFolderGroup;default:return{};} }
Qt::ItemFlags PlaylistModel::flags(const QModelIndex&i)const{ auto f=QAbstractListModel::flags(i); return i.isValid()? f|Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled : f|Qt::ItemIsDropEnabled; }
Qt::DropActions PlaylistModel::supportedDropActions()const{return Qt::MoveAction;} Qt::DropActions PlaylistModel::supportedDragActions()const{return Qt::MoveAction;}
QStringList PlaylistModel::mimeTypes()const{return {"application/x-mpvjoc-row"};}
QMimeData*PlaylistModel::mimeData(const QModelIndexList&idxs)const{ auto*m=new QMimeData; if(!idxs.isEmpty())m->setData("application/x-mpvjoc-row",QByteArray::number(idxs.first().row())); return m; }
bool PlaylistModel::dropMimeData(const QMimeData*d,Qt::DropAction a,int row,int,const QModelIndex&parent){ if(a!=Qt::MoveAction||!d||!d->hasFormat("application/x-mpvjoc-row"))return false; bool ok=false; int src=d->data("application/x-mpvjoc-row").toInt(&ok); if(!ok)return false; int dest=row<0?(parent.isValid()?parent.row():items.size()):row; if(dest>src)dest--; return moveRowTo(src,qBound(0,dest,items.size()-1)); }
bool PlaylistModel::moveRows(const QModelIndex&,int src,int count,const QModelIndex&,int dest){ if(count!=1||src<0||src>=items.size()||dest<0||dest>items.size()||dest==src||dest==src+1)return false; clearFolderDropGroups(); int final=dest>src?dest-1:dest; if(!beginMoveRows(QModelIndex(),src,src,QModelIndex(),dest))return false; items.move(src,final); endMoveRows(); return true; }
bool PlaylistModel::moveRowTo(int src,int final){ if(src==final||src<0||final<0||src>=items.size()||final>=items.size())return false; return moveRows({},src,1,{},final>src?final+1:final); }
QStringList PlaylistModel::addFiles(const QStringList&files){ QVector<PlaylistItem> ns; QStringList added; QSet<QString> existing; for(const auto&i:items)existing.insert(i.path); for(auto f:files){ QFileInfo info(f); QString abs=info.absoluteFilePath(); if(!info.exists()||!info.isFile()||existing.contains(abs))continue; existing.insert(abs); PlaylistItem it; it.path=abs; it.title=info.fileName(); it.sizeBytes=info.size(); ns<<it; added<<it.path;} if(!ns.isEmpty()){beginInsertRows({},items.size(),items.size()+ns.size()-1); for(auto&i:ns)items<<i; endInsertRows();} return added; }
QStringList PlaylistModel::addFolderGroup(const QStringList&files,const QString&folderRoot){int first=items.size();QString root=QFileInfo(folderRoot).absoluteFilePath();QStringList added=addFiles(files);if(!added.isEmpty()){for(int r=first;r<items.size();++r)items[r].folderDropRoot=root;emit dataChanged(index(first,0),index(items.size()-1,0),{FolderDropGroupRole,FolderDropGroupFirstRole,FolderDropGroupLastRole});}return added;}
void PlaylistModel::clearFolderDropGroups(){bool changed=false;folderDropGroups.clear();for(auto&i:items){if(!i.folderDropRoot.isEmpty()){i.folderDropRoot.clear();changed=true;}}if(changed&&!items.isEmpty())emit dataChanged(index(0,0),index(items.size()-1,0),{FolderDropGroupRole,FolderDropGroupFirstRole,FolderDropGroupLastRole});}
bool PlaylistModel::updatePathAt(int r,const QString&newPath){
    if(r<0||r>=items.size())return false;
    QFileInfo info(newPath);
    if(newPath.isEmpty()||!info.exists()||!info.isFile())return false;
    clearFolderDropGroups();
    auto&item=items[r];
    item.path=info.absoluteFilePath();
    item.title=info.fileName();
    item.sizeBytes=info.size();
    item.thumbnailPath.clear();
    item.thumbnailReady=false;
    item.thumbnailAttempted=false;
    emit dataChanged(index(r,0),index(r,0));
    return true;
}
bool PlaylistModel::setFamilyMoveProgressAt(int r,int percent){
    if(r<0||r>=items.size())return false;
    const int normalized=percent<0?-1:qBound(0,percent,100);
    if(items[r].familyMoveProgress==normalized)return true;
    items[r].familyMoveProgress=normalized;
    emit dataChanged(index(r,0),index(r,0),{FamilyMoveProgressRole});
    return true;
}
QString PlaylistModel::pathAt(int r)const{return r>=0&&r<items.size()?items[r].path:QString();}
QString PlaylistModel::folderDropRootAt(int r)const{return r>=0&&r<items.size()?items[r].folderDropRoot:QString();}
bool PlaylistModel::isLastItemInFolderDropGroup(int r)const{if(r<0||r>=items.size()||items[r].folderDropRoot.isEmpty())return false;const QString root=items[r].folderDropRoot;for(int n=0;n<items.size();++n)if(n!=r&&items[n].folderDropRoot==root)return false;return true;}
 int PlaylistModel::count()const{return items.size();}
QStringList PlaylistModel::pathsNeedingProbe()const{QStringList r; for(auto&i:items)if(!i.metadataProbed)r<<i.path; return r;} QStringList PlaylistModel::pathsNeedingThumbnail()const{QStringList r; for(auto&i:items)if(!i.thumbnailReady&&!i.thumbnailAttempted)r<<i.path; return r;}
bool PlaylistModel::removeRowAt(int r){ if(r<0||r>=items.size())return false; beginRemoveRows({},r,r); items.removeAt(r); endRemoveRows(); if(!items.isEmpty())emit dataChanged(index(0,0),index(items.size()-1,0),{FolderDropGroupRole,FolderDropGroupFirstRole,FolderDropGroupLastRole}); return true; } void PlaylistModel::clearItems(){beginResetModel();items.clear();folderDropGroups.clear();endResetModel();}
bool PlaylistModel::moveRowUp(int r){return moveRowTo(r,r-1);} bool PlaylistModel::moveRowDown(int r){return moveRowTo(r,r+1);} bool PlaylistModel::toggleReviewedAt(int r){if(r<0||r>=items.size())return false;items[r].reviewed=!items[r].reviewed;emit dataChanged(index(r,0),index(r,0),{ReviewedRole});return true;} void PlaylistModel::resetThumbnailAttempts(){ for(auto&i:items)i.thumbnailAttempted=false; if(!items.isEmpty())emit dataChanged(index(0,0),index(items.size()-1,0)); }
void PlaylistModel::setMetadataForPath(const QString&p,const MediaMetadata&m){ for(int r=0;r<items.size();++r)if(items[r].path==p){items[r].duration=m.duration;items[r].durationKnown=m.durationKnown;items[r].codec=m.codec;items[r].resolution=m.resolution;items[r].container=m.container;items[r].metadataProbed=true;emit dataChanged(index(r,0),index(r,0));return;}}
void PlaylistModel::markProbeFailed(const QString&p){ for(int r=0;r<items.size();++r)if(items[r].path==p){items[r].metadataProbed=true;emit dataChanged(index(r,0),index(r,0));return;}}
void PlaylistModel::setThumbnailForPath(const QString&p,const QString&t){ for(int r=0;r<items.size();++r)if(items[r].path==p){items[r].thumbnailPath=t;items[r].thumbnailReady=true;items[r].thumbnailAttempted=true;emit dataChanged(index(r,0),index(r,0));return;}}
void PlaylistModel::markThumbnailFailed(const QString&p){ for(int r=0;r<items.size();++r)if(items[r].path==p){items[r].thumbnailAttempted=true;emit dataChanged(index(r,0),index(r,0));return;}}
void PlaylistModel::resetMetadataForPath(const QString&p){for(int r=0;r<items.size();++r)if(items[r].path==p){items[r].metadataProbed=false;items[r].codec.clear();items[r].resolution.clear();items[r].container.clear();emit dataChanged(index(r,0),index(r,0));}}
void PlaylistModel::resetThumbnailAttemptForPath(const QString&p){for(int r=0;r<items.size();++r)if(items[r].path==p){items[r].thumbnailReady=false;items[r].thumbnailAttempted=false;items[r].thumbnailPath.clear();emit dataChanged(index(r,0),index(r,0));}}
void PlaylistModel::setDurationForPath(const QString&p,double d){ if(d<=0)return; for(int r=0;r<items.size();++r)if(items[r].path==p){items[r].duration=d;items[r].durationKnown=true;emit dataChanged(index(r,0),index(r,0));}}
QJsonArray PlaylistModel::toJson()const{QJsonArray a; for(auto&i:items){QJsonObject o; o["path"]=i.path;o["title"]=i.title;o["sizeBytes"]=QString::number(i.sizeBytes);o["duration"]=i.duration;o["durationKnown"]=i.durationKnown;o["codec"]=i.codec;o["resolution"]=i.resolution;o["container"]=i.container;o["metadataProbed"]=i.metadataProbed;o["thumbnailPath"]=i.thumbnailPath;o["thumbnailReady"]=i.thumbnailReady;o["thumbnailAttempted"]=i.thumbnailAttempted;o["reviewed"]=i.reviewed;o["folderDropRoot"]=i.folderDropRoot;a<<o;} return a;}
void PlaylistModel::fromJson(const QJsonArray&a){beginResetModel();items.clear(); for(auto v:a){auto o=v.toObject(); QFileInfo info(o["path"].toString()); PlaylistItem i; i.path=o["path"].toString(); if(i.path.isEmpty())continue; i.title=o["title"].toString(info.fileName()); i.sizeBytes=o["sizeBytes"].toString(QString::number(info.size())).toLongLong(); i.duration=o["duration"].toDouble(); i.durationKnown=o["durationKnown"].toBool(); i.codec=o["codec"].toString(); i.resolution=o["resolution"].toString(); i.container=o["container"].toString(); i.metadataProbed=o["metadataProbed"].toBool(); i.thumbnailPath=o["thumbnailPath"].toString(); i.thumbnailReady=o["thumbnailReady"].toBool()&&QFileInfo::exists(i.thumbnailPath); i.thumbnailAttempted=o["thumbnailAttempted"].toBool(); i.reviewed=o["reviewed"].toBool(); i.folderDropRoot=o["folderDropRoot"].toString(); if(!i.thumbnailReady)i.thumbnailAttempted=false; items<<i;} endResetModel();}
