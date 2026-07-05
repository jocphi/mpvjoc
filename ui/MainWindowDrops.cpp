#include "MainWindow.h"
#include "TimelineWidget.h"
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
#include <QKeyEvent>
#include <QLineEdit>
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

bool MainWindow::eventFilter(QObject*o,QEvent*ev){if(o==playlistSearchEdit&&ev->type()==QEvent::KeyPress){
        auto*ke=static_cast<QKeyEvent*>(ev);
        const int k=ke->key();
        const auto mods=ke->modifiers();
        const bool plain=((mods&~Qt::KeypadModifier)==Qt::NoModifier);
        if(k==Qt::Key_Escape&&plain){
            if(!playlistSearchEdit->text().isEmpty())playlistSearchEdit->clear();
            setPlaylistKeyboardFocus(true);
            ke->accept();
            return true;
        }
    }
    if(o==timeline&&(ev->type()==QEvent::MouseButtonPress||ev->type()==QEvent::FocusIn)){setPlaylistKeyboardFocus(false);}
    if(ev->type()==QEvent::FocusIn){if(o==playlistView||o==playlistView->viewport()){playlistKeyboardFocus=true;if(timeline)timeline->setPlaylistFocusMode(true);}else if(o==mpvWidget){setPlaylistKeyboardFocus(false);}}
    if(ev->type()==QEvent::KeyPress){auto*ke=static_cast<QKeyEvent*>(ev);const int k=ke->key();const auto mods=ke->modifiers();const bool plain=((mods&~Qt::KeypadModifier)==Qt::NoModifier);if(k==Qt::Key_Tab&&plain){toggleKeyboardFocusTarget();ke->accept();return true;}if((o==playlistView||o==playlistView->viewport())&&ev->type()==QEvent::KeyPress){
        auto*ke=static_cast<QKeyEvent*>(ev);
        const int k=ke->key();
        const auto mods=ke->modifiers();
        const bool plain=((mods&~Qt::KeypadModifier)==Qt::NoModifier);
        const bool plainNavigation=plain&&(k==Qt::Key_Up||k==Qt::Key_Down||k==Qt::Key_Left||k==Qt::Key_Right||k==Qt::Key_PageUp||k==Qt::Key_PageDown||k==Qt::Key_Home||k==Qt::Key_End);
        const bool alphaNum=(k>=Qt::Key_A&&k<=Qt::Key_Z)||(k>=Qt::Key_0&&k<=Qt::Key_9);
        const bool knownNonTextShortcut=(k==Qt::Key_Return||k==Qt::Key_Enter||k==Qt::Key_Delete||k==Qt::Key_F1||k==Qt::Key_F4||k==Qt::Key_F5||k==Qt::Key_F12||k==Qt::Key_Plus||k==Qt::Key_Minus||k==Qt::Key_Period||k==Qt::Key_Comma||k==Qt::Key_Colon||k==Qt::Key_Semicolon||k==Qt::Key_onehalf);
        const bool shortcutCandidate=!plainNavigation&&(alphaNum||knownNonTextShortcut);
        if(shortcutCandidate){keyPressEvent(ke);ke->accept();return true;}
    }
    }
    bool video=o==static_cast<QObject*>(mpvWidget); bool list=o==playlistView->viewport(); if((video||list)&&(ev->type()==QEvent::DragEnter||ev->type()==QEvent::DragMove||ev->type()==QEvent::Drop)){auto urls=[&](const QMimeData*m){QStringList f; if(!m)return f; for(auto u:m->urls())if(u.isLocalFile())f<<u.toLocalFile(); return f;}; if(ev->type()==QEvent::DragEnter){auto*e=static_cast<QDragEnterEvent*>(ev); if(e->mimeData()->hasUrls()){e->acceptProposedAction();return true;}} if(ev->type()==QEvent::DragMove){auto*e=static_cast<QDragMoveEvent*>(ev); if(e->mimeData()->hasUrls()){e->acceptProposedAction();return true;}} if(ev->type()==QEvent::Drop){auto*e=static_cast<QDropEvent*>(ev); QStringList f=urls(e->mimeData()); if(!f.isEmpty()){if(video)replacePlaylistWithDroppedPaths(f); else addDroppedPaths(f); e->acceptProposedAction();return true;}}} return QMainWindow::eventFilter(o,ev);}

void MainWindow::dragEnterEvent(QDragEnterEvent*e){if(e->mimeData()->hasUrls())e->acceptProposedAction();}

void MainWindow::dropEvent(QDropEvent*e){QStringList f; for(auto u:e->mimeData()->urls())if(u.isLocalFile())f<<u.toLocalFile(); replacePlaylistWithDroppedPaths(f); e->acceptProposedAction();}

void MainWindow::addDroppedPaths(const QStringList&paths){
    bool empty=playlistModel->count()==0;
    bool any=false;
    for(const QString&p:sortedDroppedPaths(paths)){
        QFileInfo info(p);
        if(info.isDir()){
            const QStringList files=recursiveVideoFilesInFolder(info.absoluteFilePath());
            auto added=playlistModel->addFolderGroup(files, info.absoluteFilePath());
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

