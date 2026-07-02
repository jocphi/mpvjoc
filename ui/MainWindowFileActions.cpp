#include "MainWindow.h"
#include "mpv/MpvWidget.h"
#include "playlist/PlaylistModel.h"
#include <QCloseEvent>
#include <QFile>
#include <QFileInfo>
#include <QListView>
#include <QMainWindow>
#include <QtGlobal>

MainWindow::~MainWindow(){savePlaylistState();}

void MainWindow::moveSelectedFileToTrash(){int r=currentRow(); if(r<0||r>=playlistModel->count())return; QString p=playlistModel->pathAt(r); if(p.isEmpty())return; bool wasCurrent=(QFileInfo(mpvWidget->currentFilePath()).absoluteFilePath()==QFileInfo(p).absoluteFilePath()); if(wasCurrent)closeCurrentFile(); bool ok=true; if(QFileInfo::exists(p))ok=QFile::moveToTrash(p); if(ok||!QFileInfo::exists(p)){playlistModel->removeRowAt(r); if(playlistModel->count()>0){int next=qBound(0,r,playlistModel->count()-1); playlistView->setCurrentIndex(playlistModel->index(next,0)); playPlaylistRow(next);} else savePlaylistState();} }

void MainWindow::closeEvent(QCloseEvent*e){savePlaylistState();QMainWindow::closeEvent(e);}

