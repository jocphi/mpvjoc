#include "MainWindow.h"
#include "mpv/MpvWidget.h"
#include "playlist/PlaylistModel.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListView>
#include <QSplitter>
#include <QStandardPaths>
#include <QTimer>

QString MainWindow::stateFilePath()const{QString d=QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation); if(d.isEmpty())d=QDir::homePath()+"/.config/mpvjoc"; QDir().mkpath(d); return QDir(d).filePath("playlist-state.json");}

void MainWindow::savePlaylistState(){QJsonObject root; root["version"]=9; root["selectedRow"]=currentRow(); root["volume"]=currentVolume; root["muted"]=currentMuted; root["maxVideoScale"]=maxVideoScale; root["clipVideoToScale"]=clipVideoToScale; root["autoPlayNextEnabled"]=autoPlayNextEnabled; root["windowWidth"]=width(); root["windowHeight"]=height(); QJsonArray ss; if(playlistSplitter)for(int s:playlistSplitter->sizes())ss<<s; root["splitterSizes"]=ss; root["items"]=playlistModel->toJson(); QFile f(stateFilePath()); if(f.open(QIODevice::WriteOnly|QIODevice::Truncate))f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));}

void MainWindow::loadPlaylistState(){QFile f(stateFilePath()); if(!f.open(QIODevice::ReadOnly))return; auto d=QJsonDocument::fromJson(f.readAll()); if(!d.isObject())return; auto root=d.object(); playlistModel->fromJson(root["items"].toArray()); removeMissingFilesFromPlaylist(); int ww=root["windowWidth"].toInt(width()); int wh=root["windowHeight"].toInt(height()); if(ww>300&&wh>200)resize(ww,wh); currentVolume=root["volume"].toDouble(currentVolume); currentMuted=root["muted"].toBool(currentMuted); maxVideoScale=normalizedMaxVideoScale(root["maxVideoScale"].toDouble(maxVideoScale)); clipVideoToScale=root["clipVideoToScale"].toBool(root["cropVideoToScale"].toBool(clipVideoToScale)); autoPlayNextEnabled=root["autoPlayNextEnabled"].toBool(autoPlayNextEnabled); updateMuteVolumeButton(); updateScaleButtons(); updateClipButton(); updateAutoPlayButton(); if(mpvWidget){mpvWidget->setMaxVideoScale(maxVideoScale);mpvWidget->setClipVideoToScale(clipVideoToScale);} auto arr=root["splitterSizes"].toArray(); if(playlistSplitter&&arr.size()>=2){QList<int>s; for(auto v:arr)s<<v.toInt(); playlistSplitter->setSizes(s);} int r=qBound(0,root["selectedRow"].toInt(0),qMax(0,playlistModel->count()-1)); if(playlistModel->count()>0)playlistView->setCurrentIndex(playlistModel->index(r,0)); const double savedVolume=currentVolume; const bool savedMuted=currentMuted; QTimer::singleShot(0,this,[this,savedVolume,savedMuted]{currentVolume=savedVolume;currentMuted=savedMuted;updateMuteVolumeButton();mpvWidget->setVolume(savedVolume);mpvWidget->setMute(savedMuted);}); QTimer::singleShot(250,this,[this,savedVolume,savedMuted]{currentVolume=savedVolume;currentMuted=savedMuted;updateMuteVolumeButton();mpvWidget->setVolume(savedVolume);mpvWidget->setMute(savedMuted);}); QTimer::singleShot(650,this,[this,savedVolume,savedMuted]{currentVolume=savedVolume;currentMuted=savedMuted;updateMuteVolumeButton();});}

