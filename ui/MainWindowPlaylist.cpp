#include "MainWindow.h"
#include "TimelineWidget.h"
#include "media/MetadataProbeManager.h"
#include "media/ThumbnailManager.h"
#include "mpv/MpvWidget.h"
#include "playlist/PlaylistModel.h"
#include "playlist/PlaylistFilterProxyModel.h"
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
#include "playlist/PlaylistFormatting.h"
#include <QSettings>

void MainWindow::addFiles(const QStringList&files){if(currentPlaylistLocked())return;bool empty=playlistModel->count()==0; auto added=playlistModel->addFiles(files); for(auto&p:added){metadataProbe->enqueue(p);thumbnailManager->enqueue(p);} if(empty&&playlistModel->count()>0)playPlaylistRow(0); savePlaylistState();}

QModelIndex MainWindow::playlistViewIndexForRow(int sourceRow)const{
    if(!playlistModel||sourceRow<0||sourceRow>=playlistModel->count())return QModelIndex();
    QModelIndex src=playlistModel->index(sourceRow,0);
    return playlistProxyModel?playlistProxyModel->mapFromSource(src):src;
}
void MainWindow::setPlaylistCurrentSourceRow(int sourceRow,bool scroll){
    QModelIndex idx=playlistViewIndexForRow(sourceRow);
    if(!idx.isValid())return;
    playlistView->setCurrentIndex(idx);
    if(scroll)playlistView->scrollTo(idx);
}
void MainWindow::ensureVisiblePlaylistSelection(){
    if(!playlistView||!playlistModel)return;
    if(playlistProxyModel){
        QModelIndex current=playlistView->currentIndex();
        if(current.isValid()&&current.model()==playlistProxyModel)return;
        if(playlistProxyModel->rowCount()>0)playlistView->setCurrentIndex(playlistProxyModel->index(0,0));
        return;
    }
    QModelIndex current=playlistView->currentIndex();
    if(current.isValid())return;
    if(playlistModel->count()>0)playlistView->setCurrentIndex(playlistModel->index(0,0));
}
void MainWindow::updatePlaylistSummary()
{
    if (!playlistSummaryLabel || !playlistModel)
        return;

    const PlaylistSizeUnit summarySizeUnit = playlistSizeUnitFromInt(
        QSettings().value(QStringLiteral("playlist/sizeUnit"), 2).toInt());

    const int sourceTotalCount = playlistModel->count();
    const bool hasProxy = playlistProxyModel != nullptr;
    const int visibleCount = hasProxy ? playlistProxyModel->rowCount() : sourceTotalCount;
    const bool filtered = hasProxy && visibleCount != sourceTotalCount;

    auto sourceIndexForVisibleRow = [this, hasProxy](int row) -> QModelIndex {
        if (hasProxy) {
            const QModelIndex proxyIndex = playlistProxyModel->index(row, 0);
            return playlistProxyModel->mapToSource(proxyIndex);
        }
        return playlistModel->index(row, 0);
    };

    qint64 totalBytes = 0;
    double totalDuration = 0.0;

    for (int r = 0; r < visibleCount; ++r) {
        const QModelIndex idx = sourceIndexForVisibleRow(r);
        if (!idx.isValid())
            continue;
        totalBytes += idx.data(PlaylistModel::SizeRole).toLongLong();
        if (idx.data(PlaylistModel::DurationKnownRole).toBool())
            totalDuration += idx.data(PlaylistModel::DurationRole).toDouble();
    }

    const int sourceRow = currentRow();
    QModelIndex currentSourceIndex;
    int currentDisplayRow = -1;

    if (sourceRow >= 0 && sourceRow < sourceTotalCount) {
        currentSourceIndex = playlistModel->index(sourceRow, 0);
        if (hasProxy) {
            const QModelIndex proxyIndex = playlistProxyModel->mapFromSource(currentSourceIndex);
            if (proxyIndex.isValid())
                currentDisplayRow = proxyIndex.row();
        } else {
            currentDisplayRow = sourceRow;
        }
    }

    QString filesText;
    if (currentSourceIndex.isValid() && currentDisplayRow >= 0 && visibleCount > 0)
        filesText = QStringLiteral("%1/%2 files").arg(currentDisplayRow + 1).arg(visibleCount);
    else if (filtered)
        filesText = QStringLiteral("%1/%2 files").arg(visibleCount).arg(sourceTotalCount);
    else
        filesText = QStringLiteral("%1 files").arg(sourceTotalCount);

    QString sizeText = formatEuroSize(totalBytes, summarySizeUnit);
    QString durationText = formatHMS(totalDuration);

    if (currentSourceIndex.isValid()) {
        const qint64 currentBytes = currentSourceIndex.data(PlaylistModel::SizeRole).toLongLong();
        const bool currentDurationKnown = currentSourceIndex.data(PlaylistModel::DurationKnownRole).toBool();
        const double currentDuration = currentSourceIndex.data(PlaylistModel::DurationRole).toDouble();

        sizeText = QStringLiteral("%1 / %2").arg(formatEuroSize(currentBytes, summarySizeUnit), formatEuroSize(totalBytes, summarySizeUnit));
        durationText = QStringLiteral("%1 / %2")
                           .arg(currentDurationKnown ? formatHMS(currentDuration) : QStringLiteral("--:--:--"),
                               formatHMS(totalDuration));
    }

    playlistSummaryLabel->setText(QStringLiteral("%1  •  %2  •  %3").arg(filesText, sizeText, durationText));
}

void MainWindow::playNextPlaylistFile(){if(playlistModel->count()<=0)return; int r=currentRow(); if(r<0)r=0; r=(r+1)%playlistModel->count(); playPlaylistRow(r);}

void MainWindow::playPreviousPlaylistFile(){if(playlistModel->count()<=0)return; int r=currentRow(); if(r<0)r=0; r=(r-1+playlistModel->count())%playlistModel->count(); playPlaylistRow(r);}

int MainWindow::currentRow()const{auto idx=playlistView->currentIndex();if(!idx.isValid())return -1;if(playlistProxyModel&&idx.model()==playlistProxyModel)idx=playlistProxyModel->mapToSource(idx);else if(idx.model()!=playlistModel)return -1;return idx.row();}
QString MainWindow::selectedPath()const{return playlistModel->pathAt(currentRow());}

void MainWindow::removeMissingFilesFromPlaylist(){if(currentPlaylistLocked())return;for(int r=playlistModel->count()-1;r>=0;--r){QString p=playlistModel->pathAt(r); if(!QFileInfo::exists(p))playlistModel->removeRowAt(r);}}

void MainWindow::replacePlaylistWithFiles(const QStringList&files){if(currentPlaylistLocked())return;closeCurrentFile(); playlistModel->clearItems(); addFiles(files); savePlaylistState();}

void MainWindow::playPlaylistRow(int r){if(r<0&&playlistModel&&playlistModel->count()>0&&mpvWidget&&mpvWidget->currentFilePath().isEmpty())r=0;if(r<0||r>=playlistModel->count())return; QString p=playlistModel->pathAt(r); if(!QFileInfo::exists(p)){if(!currentPlaylistLocked()){playlistModel->removeRowAt(r); if(playlistModel->count()>0)setPlaylistCurrentSourceRow(qBound(0,r,playlistModel->count()-1)); savePlaylistState();} return;} setPlaylistCurrentSourceRow(r,true); suppressNextEndFileAdvance=true; mpvWidget->loadFile(p); QTimer::singleShot(750,this,[this]{suppressNextEndFileAdvance=false;}); mpvWidget->setFocus(Qt::ShortcutFocusReason); savePlaylistState();}

void MainWindow::selectPath(const QString&p){for(int r=0;r<playlistModel->count();++r)if(playlistModel->pathAt(r)==p){setPlaylistCurrentSourceRow(r);return;}}

void MainWindow::removeSelectedItem(){if(currentPlaylistLocked())return;int r=currentRow(); if(playlistModel->removeRowAt(r)&&playlistModel->count()>0)setPlaylistCurrentSourceRow(qBound(0,r,playlistModel->count()-1)); savePlaylistState();}

void MainWindow::clearPlaylist(){if(currentPlaylistLocked())return;playlistModel->clearItems();savePlaylistState();}

void MainWindow::moveSelectedItemUp(){if(currentPlaylistLocked())return;int r=currentRow(); if(playlistModel->moveRowUp(r))setPlaylistCurrentSourceRow(r-1);savePlaylistState();}

void MainWindow::moveSelectedItemDown(){if(currentPlaylistLocked())return;int r=currentRow(); if(playlistModel->moveRowDown(r))setPlaylistCurrentSourceRow(r+1);savePlaylistState();}

void MainWindow::toggleReviewedForCurrent(){if(currentPlaylistLocked())return;int r=currentRow();if(r<0)return;playlistModel->toggleReviewedAt(r);savePlaylistState();}
void MainWindow::showPlaylistContextMenu(const QPoint&p){
    const int row=currentRow();
    const bool hasRow=playlistModel&&row>=0&&row<playlistModel->count();
    const bool reviewed=hasRow&&playlistModel->index(row,0).data(PlaylistModel::ReviewedRole).toBool();
    const bool locked=currentPlaylistLocked();

    QMenu menu(this);

    QAction*playAction=menu.addAction(QStringLiteral("Play selected"));
    playAction->setEnabled(hasRow);
    connect(playAction,&QAction::triggered,this,[this,row]{playPlaylistRow(row);});

    QAction*reviewedAction=menu.addAction(reviewed?QStringLiteral("Mark unreviewed"):QStringLiteral("Mark reviewed"));
    reviewedAction->setEnabled(hasRow&&!locked);
    connect(reviewedAction,&QAction::triggered,this,[this]{toggleReviewedForCurrent();});

    menu.addSeparator();

    QAction*removeAction=menu.addAction(QStringLiteral("Remove from playlist"));
    removeAction->setEnabled(hasRow&&!locked);
    connect(removeAction,&QAction::triggered,this,[this,row]{if(row>=0&&playlistModel){playlistModel->removeRowAt(row);updatePlaylistSummary();savePlaylistState();}});

    QAction*trashAction=menu.addAction(QStringLiteral("Move file to trash"));
    trashAction->setEnabled(hasRow&&!locked);
    connect(trashAction,&QAction::triggered,this,[this]{moveSelectedFileToTrash();});

    menu.addSeparator();

    QAction*upAction=menu.addAction(QStringLiteral("Move up"));
    upAction->setEnabled(hasRow&&!locked&&row>0);
    connect(upAction,&QAction::triggered,this,[this,row]{if(playlistModel&&playlistModel->moveRowUp(row)){setPlaylistCurrentSourceRow(row-1,true);savePlaylistState();}});

    QAction*downAction=menu.addAction(QStringLiteral("Move down"));
    downAction->setEnabled(hasRow&&!locked&&playlistModel&&row<playlistModel->count()-1);
    connect(downAction,&QAction::triggered,this,[this,row]{if(playlistModel&&playlistModel->moveRowDown(row)){setPlaylistCurrentSourceRow(row+1,true);savePlaylistState();}});

    menu.addSeparator();

    QAction*clearAction=menu.addAction(QStringLiteral("Clear playlist"));
    clearAction->setEnabled(!locked&&playlistModel&&playlistModel->count()>0);
    connect(clearAction,&QAction::triggered,this,[this]{while(playlistModel&&playlistModel->count()>0)playlistModel->removeRowAt(0);updatePlaylistSummary();savePlaylistState();});

    menu.exec(playlistView->viewport()->mapToGlobal(p));
}
