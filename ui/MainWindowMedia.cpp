#include "MainWindow.h"
#include "media/MetadataProbeManager.h"
#include "media/ThumbnailManager.h"
#include "playlist/PlaylistModel.h"
#include <QFileDialog>
#include <QStringList>

void MainWindow::openFiles(){addFiles(QFileDialog::getOpenFileNames(this,"Open media files",{},"Media files (*.mkv *.mp4 *.avi *.mov *.webm *.mp3 *.flac *.wav);;All files (*)"));}

void MainWindow::probeMissingMetadata(){for(auto&p:playlistModel->pathsNeedingProbe())metadataProbe->enqueue(p);}

void MainWindow::generateMissingThumbnails(){for(auto&p:playlistModel->pathsNeedingThumbnail())thumbnailManager->enqueue(p);}

void MainWindow::refreshMediaScans(){
    if(!playlistModel)return;
    QStringList paths;
    for(int r=0;r<playlistModel->count();++r){
        QString p=playlistModel->pathAt(r);
        if(p.isEmpty())continue;
        paths<<p;
    }
    for(const QString&p:paths)playlistModel->resetMetadataForPath(p);
    for(const QString&p:paths)playlistModel->resetThumbnailAttemptForPath(p);
    probeMissingMetadata();
    generateMissingThumbnails();
    updatePlaylistSummary();
    if(!restoringPlaybackState)savePlaylistState();
}
