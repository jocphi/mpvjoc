#include "MainWindow.h"
#include "mpv/MpvWidget.h"
#include "playlist/PlaylistModel.h"
#include "media/MetadataProbeManager.h"
#include "media/ThumbnailManager.h"
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileInfo>
#include <QListView>
#include <QMimeData>
#include <QSet>
#include <QUrl>

static bool isVideoPlaylistFile(const QFileInfo&info){
    if(!info.exists()||!info.isFile())return false;
    static const QSet<QString> exts={
        "3g2","3gp","avi","divx","dv","f4v","flv","m2ts","m4v","mkv","mov","mp4","mpeg","mpg","mts","mxf","ogm","ogv","rm","rmvb","ts","vob","webm","wmv"
    };
    return exts.contains(info.suffix().toLower());
}

static QStringList recursiveVideoFilesInFolder(const QString&folder){
    QStringList files;
    QDirIterator it(folder,QDir::Files|QDir::Readable,QDirIterator::Subdirectories);
    while(it.hasNext()){
        QFileInfo info(it.next());
        if(isVideoPlaylistFile(info))files<<info.absoluteFilePath();
    }
    files.sort(Qt::CaseInsensitive);
    return files;
}

static QStringList sortedDroppedPaths(const QStringList&paths){
    QStringList sorted=paths;
    sorted.sort(Qt::CaseInsensitive);
    return sorted;
}

bool MainWindow::eventFilter(QObject*o,QEvent*ev){bool video=o==static_cast<QObject*>(mpvWidget); bool list=o==playlistView->viewport(); if((video||list)&&(ev->type()==QEvent::DragEnter||ev->type()==QEvent::DragMove||ev->type()==QEvent::Drop)){auto urls=[&](const QMimeData*m){QStringList f; if(!m)return f; for(auto u:m->urls())if(u.isLocalFile())f<<u.toLocalFile(); return f;}; if(ev->type()==QEvent::DragEnter){auto*e=static_cast<QDragEnterEvent*>(ev); if(e->mimeData()->hasUrls()){e->acceptProposedAction();return true;}} if(ev->type()==QEvent::DragMove){auto*e=static_cast<QDragMoveEvent*>(ev); if(e->mimeData()->hasUrls()){e->acceptProposedAction();return true;}} if(ev->type()==QEvent::Drop){auto*e=static_cast<QDropEvent*>(ev); QStringList f=urls(e->mimeData()); if(!f.isEmpty()){if(video)replacePlaylistWithDroppedPaths(f); else addDroppedPaths(f); e->acceptProposedAction();return true;}}} return QMainWindow::eventFilter(o,ev);}

void MainWindow::dragEnterEvent(QDragEnterEvent*e){if(e->mimeData()->hasUrls())e->acceptProposedAction();}

void MainWindow::dropEvent(QDropEvent*e){QStringList f; for(auto u:e->mimeData()->urls())if(u.isLocalFile())f<<u.toLocalFile(); replacePlaylistWithDroppedPaths(f); e->acceptProposedAction();}

void MainWindow::addDroppedPaths(const QStringList&paths){
    bool empty=playlistModel->count()==0;
    bool any=false;
    for(const QString&p:sortedDroppedPaths(paths)){
        QFileInfo info(p);
        if(info.isDir()){
            const QStringList files=recursiveVideoFilesInFolder(info.absoluteFilePath());
            auto added=playlistModel->addFolderGroup(files);
            for(auto&a:added){metadataProbe->enqueue(a);thumbnailManager->enqueue(a);}
            any=any||!added.isEmpty();
        }else if(isVideoPlaylistFile(info)){
            auto added=playlistModel->addFiles(QStringList{info.absoluteFilePath()});
            for(auto&a:added){metadataProbe->enqueue(a);thumbnailManager->enqueue(a);}
            any=any||!added.isEmpty();
        }
    }
    if(empty&&playlistModel->count()>0)playPlaylistRow(0);
    if(any)savePlaylistState();
    updatePlaylistSummary();
}

void MainWindow::replacePlaylistWithDroppedPaths(const QStringList&paths){
    closeCurrentFile();
    playlistModel->clearItems();
    addDroppedPaths(paths);
    savePlaylistState();
}

