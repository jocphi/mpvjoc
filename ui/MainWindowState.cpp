#include "MainWindow.h"
#include "PlaylistWorkspace.h"
#include "mpv/MpvWidget.h"
#include "playlist/PlaylistModel.h"
#include "playlist/PlaylistFilterProxyModel.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListView>
#include <QLineEdit>
#include <QTabWidget>
#include <QSplitter>
#include <QStandardPaths>
#include <QTimer>

QString MainWindow::stateFilePath()const{QString d=QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation); if(d.isEmpty())d=QDir::homePath()+"/.config/mpvjoc"; QDir().mkpath(d); return QDir(d).filePath("playlist-state.json");}

void MainWindow::savePlaylistState(){
    QJsonObject root;
    root["version"]=10;
    root["volume"]=currentVolume;
    root["muted"]=currentMuted;
    root["maxVideoScale"]=maxVideoScale;
    root["clipVideoToScale"]=clipVideoToScale;
    root["autoPlayNextEnabled"]=autoPlayNextEnabled;
    root["windowWidth"]=width();
    root["windowHeight"]=height();

    QJsonArray splitterSizes;
    if(playlistSplitter)for(int size:playlistSplitter->sizes())splitterSizes<<size;
    root["splitterSizes"]=splitterSizes;

    QJsonArray playlists;
    if(rightTabs){
        for(int tab=0;tab<rightTabs->count();++tab){
            auto*workspace=static_cast<PlaylistWorkspace*>(rightTabs->widget(tab));
            if(!workspace)continue;
            QJsonObject playlist;
            playlist["name"]=rightTabs->tabText(tab);
            playlist["searchText"]=workspace->searchEdit->text();
            QModelIndex selected=workspace->view->currentIndex();
            if(selected.isValid()&&selected.model()==workspace->proxy)
                selected=workspace->proxy->mapToSource(selected);
            playlist["selectedRow"]=selected.isValid()?selected.row():-1;
            playlist["items"]=workspace->model->toJson();
            playlists<<playlist;
        }
        root["currentPlaylistTab"]=rightTabs->currentIndex();
    }
    root["playlists"]=playlists;
    root["playlistSerial"]=playlistSerial;

    // Keep the active playlist mirrored in the legacy fields for easier
    // downgrade/recovery with older builds.
    root["selectedRow"]=currentRow();
    root["items"]=playlistModel?playlistModel->toJson():QJsonArray();

    QFile file(stateFilePath());
    if(file.open(QIODevice::WriteOnly|QIODevice::Truncate))
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void MainWindow::loadPlaylistState(){
    QFile file(stateFilePath());
    if(!file.open(QIODevice::ReadOnly))return;
    const QJsonDocument document=QJsonDocument::fromJson(file.readAll());
    if(!document.isObject())return;
    const QJsonObject root=document.object();

    const int windowWidth=root["windowWidth"].toInt(width());
    const int windowHeight=root["windowHeight"].toInt(height());
    if(windowWidth>300&&windowHeight>200)resize(windowWidth,windowHeight);

    currentVolume=root["volume"].toDouble(currentVolume);
    currentMuted=root["muted"].toBool(currentMuted);
    maxVideoScale=normalizedMaxVideoScale(root["maxVideoScale"].toDouble(maxVideoScale));
    clipVideoToScale=root["clipVideoToScale"].toBool(root["cropVideoToScale"].toBool(clipVideoToScale));
    autoPlayNextEnabled=root["autoPlayNextEnabled"].toBool(autoPlayNextEnabled);

    struct RestoredPlaylist{
        PlaylistWorkspace*workspace=nullptr;
        int selectedRow=-1;
    };
    QVector<RestoredPlaylist> restored;
    const QJsonArray savedPlaylists=root["playlists"].toArray();

    // Rebuild the tab set in saved order. During constructor startup
    // restoringPlaybackState is true, so workspace signals do not write an
    // intermediate state file.
    if(rightTabs){
        while(rightTabs->count()>0){
            QWidget*page=rightTabs->widget(0);
            rightTabs->removeTab(0);
            if(page)page->deleteLater();
        }
        playlistWorkspaces.clear();

        if(!savedPlaylists.isEmpty()){
            for(int index=0;index<savedPlaylists.size();++index){
                const QJsonObject saved=savedPlaylists[index].toObject();
                QString name=saved["name"].toString().trimmed();
                if(name.isEmpty())name=QStringLiteral("Playlist %1").arg(index+1);
                PlaylistWorkspace*workspace=createPlaylistWorkspace(name);
                workspace->model->fromJson(saved["items"].toArray());
                for(int row=workspace->model->count()-1;row>=0;--row)
                    if(!QFileInfo::exists(workspace->model->pathAt(row)))workspace->model->removeRowAt(row);
                workspace->searchEdit->setText(saved["searchText"].toString());
                restored<<RestoredPlaylist{workspace,saved["selectedRow"].toInt(-1)};
            }
        }else{
            // Backward compatibility with version 9 and older single-playlist
            // state files.
            PlaylistWorkspace*workspace=createPlaylistWorkspace(QStringLiteral("Playlist 1"));
            workspace->model->fromJson(root["items"].toArray());
            for(int row=workspace->model->count()-1;row>=0;--row)
                if(!QFileInfo::exists(workspace->model->pathAt(row)))workspace->model->removeRowAt(row);
            restored<<RestoredPlaylist{workspace,root["selectedRow"].toInt(-1)};
        }

        playlistSerial=qMax(root["playlistSerial"].toInt(restored.size()+1),restored.size()+1);
        const int active=qBound(0,root["currentPlaylistTab"].toInt(0),qMax(0,rightTabs->count()-1));
        rightTabs->setCurrentIndex(active);
        activatePlaylistWorkspace(active);

        for(const RestoredPlaylist&entry:restored){
            if(!entry.workspace||entry.workspace->model->count()<=0||entry.selectedRow<0)continue;
            const int sourceRow=qBound(0,entry.selectedRow,entry.workspace->model->count()-1);
            QModelIndex source=entry.workspace->model->index(sourceRow,0);
            QModelIndex visible=entry.workspace->proxy->mapFromSource(source);
            if(visible.isValid())entry.workspace->view->setCurrentIndex(visible);
        }
        activatePlaylistWorkspace(active);
    }

    updateMuteVolumeButton();
    updateScaleButtons();
    updateClipButton();
    updateAutoPlayButton();
    if(mpvWidget){
        mpvWidget->setMaxVideoScale(maxVideoScale);
        mpvWidget->setClipVideoToScale(clipVideoToScale);
    }

    const QJsonArray savedSplitterSizes=root["splitterSizes"].toArray();
    if(playlistSplitter&&savedSplitterSizes.size()>=2){
        QList<int>sizes;
        for(const QJsonValue&value:savedSplitterSizes)sizes<<value.toInt();
        playlistSplitter->setSizes(sizes);
    }

    const double savedVolume=currentVolume;
    const bool savedMuted=currentMuted;
    QTimer::singleShot(0,this,[this,savedVolume,savedMuted]{currentVolume=savedVolume;currentMuted=savedMuted;updateMuteVolumeButton();mpvWidget->setVolume(savedVolume);mpvWidget->setMute(savedMuted);});
    QTimer::singleShot(250,this,[this,savedVolume,savedMuted]{currentVolume=savedVolume;currentMuted=savedMuted;updateMuteVolumeButton();mpvWidget->setVolume(savedVolume);mpvWidget->setMute(savedMuted);});
    QTimer::singleShot(650,this,[this,savedVolume,savedMuted]{currentVolume=savedVolume;currentMuted=savedMuted;updateMuteVolumeButton();});
    updatePlaylistSummary();
}

