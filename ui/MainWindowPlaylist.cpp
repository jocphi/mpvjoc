#include "MainWindow.h"
#include "TimelineWidget.h"
#include "media/MetadataProbeManager.h"
#include "media/ThumbnailManager.h"
#include "mpv/MpvWidget.h"
#include "playlist/PlaylistModel.h"
#include "util/Utils.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QModelIndex>
#include <QTimer>
#include <QUrl>
#include <QtGlobal>

void MainWindow::addFiles(const QStringList&files){bool empty=playlistModel->count()==0; auto added=playlistModel->addFiles(files); for(auto&p:added){metadataProbe->enqueue(p);thumbnailManager->enqueue(p);} if(empty&&playlistModel->count()>0)playPlaylistRow(0); savePlaylistState();}

void MainWindow::updatePlaylistSummary(){if(!playlistSummaryLabel||!playlistModel)return;qint64 totalBytes=0;double totalDuration=0;for(int r=0;r<playlistModel->count();++r){QModelIndex idx=playlistModel->index(r,0);totalBytes+=idx.data(PlaylistModel::SizeRole).toLongLong();if(idx.data(PlaylistModel::DurationKnownRole).toBool())totalDuration+=idx.data(PlaylistModel::DurationRole).toDouble();}playlistSummaryLabel->setText(QString("%1 files  •  %2  •  %3").arg(playlistModel->count()).arg(formatBytes(totalBytes)).arg(formatHMS(totalDuration)));}

void MainWindow::playNextPlaylistFile(){if(playlistModel->count()<=0)return; int r=currentRow(); if(r<0)r=0; r=(r+1)%playlistModel->count(); playPlaylistRow(r);}

void MainWindow::playPreviousPlaylistFile(){if(playlistModel->count()<=0)return; int r=currentRow(); if(r<0)r=0; r=(r-1+playlistModel->count())%playlistModel->count(); playPlaylistRow(r);}

int MainWindow::currentRow()const{auto i=playlistView->currentIndex();return i.isValid()?i.row():0;}

QString MainWindow::selectedPath()const{return playlistModel->pathAt(currentRow());}

void MainWindow::removeMissingFilesFromPlaylist(){for(int r=playlistModel->count()-1;r>=0;--r){QString p=playlistModel->pathAt(r); if(!QFileInfo::exists(p))playlistModel->removeRowAt(r);}}

void MainWindow::replacePlaylistWithFiles(const QStringList&files){closeCurrentFile(); playlistModel->clearItems(); addFiles(files); savePlaylistState();}

void MainWindow::playPlaylistRow(int r){if(r<0||r>=playlistModel->count())return; QString p=playlistModel->pathAt(r); if(!QFileInfo::exists(p)){playlistModel->removeRowAt(r); if(playlistModel->count()>0)playlistView->setCurrentIndex(playlistModel->index(qBound(0,r,playlistModel->count()-1),0)); savePlaylistState(); return;} playlistView->setCurrentIndex(playlistModel->index(r,0)); playlistView->scrollTo(playlistModel->index(r,0)); suppressNextEndFileAdvance=true; mpvWidget->loadFile(p); QTimer::singleShot(750,this,[this]{suppressNextEndFileAdvance=false;}); mpvWidget->setFocus(Qt::ShortcutFocusReason); savePlaylistState();}

void MainWindow::selectPath(const QString&p){for(int r=0;r<playlistModel->count();++r)if(playlistModel->pathAt(r)==p){playlistView->setCurrentIndex(playlistModel->index(r,0));return;}}

void MainWindow::removeSelectedItem(){int r=currentRow(); if(playlistModel->removeRowAt(r)&&playlistModel->count()>0)playlistView->setCurrentIndex(playlistModel->index(qBound(0,r,playlistModel->count()-1),0)); savePlaylistState();}

void MainWindow::clearPlaylist(){playlistModel->clearItems();savePlaylistState();}

void MainWindow::moveSelectedItemUp(){int r=currentRow(); if(playlistModel->moveRowUp(r))playlistView->setCurrentIndex(playlistModel->index(r-1,0));savePlaylistState();}

void MainWindow::moveSelectedItemDown(){int r=currentRow(); if(playlistModel->moveRowDown(r))playlistView->setCurrentIndex(playlistModel->index(r+1,0));savePlaylistState();}

void MainWindow::showPlaylistContextMenu(const QPoint&p){auto idx=playlistView->indexAt(p); if(idx.isValid())playlistView->setCurrentIndex(idx); QMenu m(this); auto*play=m.addAction("Play"); auto*rem=m.addAction("Remove from playlist"); auto*trash=m.addAction("Move file to trash"); auto*copy=m.addAction("Copy path"); auto*show=m.addAction("Show in file manager"); auto*rep=m.addAction("Reprobe metadata"); auto*th=m.addAction("Regenerate thumbnail"); auto*clear=m.addAction("Clear playlist"); auto*a=m.exec(playlistView->viewport()->mapToGlobal(p)); if(!a)return; if(a==play)playPlaylistRow(currentRow()); else if(a==rem)removeSelectedItem(); else if(a==trash)moveSelectedFileToTrash(); else if(a==copy)QApplication::clipboard()->setText(selectedPath()); else if(a==show)QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(selectedPath()).absolutePath())); else if(a==rep){playlistModel->resetMetadataForPath(selectedPath());metadataProbe->enqueue(selectedPath());} else if(a==th){playlistModel->resetThumbnailAttemptForPath(selectedPath());thumbnailManager->enqueue(selectedPath());} else if(a==clear)clearPlaylist();}

