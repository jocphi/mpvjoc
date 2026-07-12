#include "MainWindow.h"
#include "playlist/PlaylistModel.h"
#include "mpv/MpvWidget.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListView>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextEdit>
#include <QTextStream>
#include <QVector>
#include <QDirIterator>

namespace {
bool droppedFolderIsCompletelyEmptyAfterMove(const QString& folderRoot)
{
    QFileInfo info(folderRoot);
    if (!info.exists() || !info.isDir())
        return false;

    QDirIterator it(info.absoluteFilePath(),
        QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);

    return !it.hasNext();
}
}


QString MainWindow::moveConfigFilePath()const{QFileInfo info(stateFilePath()); return QDir(info.absolutePath()).filePath("move-buttons.conf");}

QString MainWindow::moveLogFilePath()const{QFileInfo info(stateFilePath()); return QDir(info.absolutePath()).filePath("move-log.tsv");}

void MainWindow::ensureMoveLists(){while(moveButtonNames.size()<6)moveButtonNames<<QString("Move %1").arg(moveButtonNames.size()+1); while(moveButtonPaths.size()<6)moveButtonPaths<<QString();}

void MainWindow::createDefaultMoveConfig(){QFile f(moveConfigFilePath()); if(f.exists())return; if(f.open(QIODevice::WriteOnly|QIODevice::Text)){QTextStream s(&f); s<<"move_button_count=6\n"; for(int i=1;i<=6;++i){s<<"move_button_"<<i<<"_name=Move "<<i<<"\n"; s<<"move_button_"<<i<<"_path=\n";}}}

void MainWindow::saveMoveConfig()const{
    QFile f(moveConfigFilePath());
    if(!f.open(QIODevice::WriteOnly|QIODevice::Text))return;
    QTextStream s(&f);
    s<<"move_button_count="<<qBound(1,moveButtonCount,6)<<"\n";
    for(int i=0;i<6;++i){
        s<<"move_button_"<<(i+1)<<"_name="<<moveButtonNames.value(i,QString("Move %1").arg(i+1))<<"\n";
        s<<"move_button_"<<(i+1)<<"_path="<<moveButtonPaths.value(i)<<"\n";
    }
}
void MainWindow::loadMoveConfig(){ensureMoveLists(); createDefaultMoveConfig(); QFile f(moveConfigFilePath()); if(f.open(QIODevice::ReadOnly|QIODevice::Text)){while(!f.atEnd()){QString line=QString::fromUtf8(f.readLine()).trimmed(); if(line.isEmpty()||line.startsWith('#'))continue; int eq=line.indexOf('='); if(eq<0)continue; QString key=line.left(eq).trimmed(); QString val=line.mid(eq+1).trimmed(); if(key=="move_button_count")moveButtonCount=qBound(1,val.toInt(),6); for(int i=0;i<6;++i){QString n=QString("move_button_%1_name").arg(i+1); QString p=QString("move_button_%1_path").arg(i+1); if(key==n)moveButtonNames[i]=val; else if(key==p)moveButtonPaths[i]=val;}}} updateMoveButtons(); refreshMoveLogView();}

void MainWindow::refreshMoveLogView(){if(!moveLogView)return; QFile f(moveLogFilePath()); if(f.open(QIODevice::ReadOnly|QIODevice::Text))moveLogView->setPlainText(QString::fromUtf8(f.readAll())); else moveLogView->setPlainText(QString());}

void MainWindow::updateMoveButtons(){ensureMoveLists(); for(int i=0;i<moveButtons.size();++i){auto*b=moveButtons[i]; bool visible=i<moveButtonCount; b->setVisible(visible); QString name=moveButtonNames.value(i,QString("Move %1").arg(i+1)); QString target=moveButtonPaths.value(i); b->setText(name.isEmpty()?QString("Move %1").arg(i+1):name); b->setEnabled(visible&&!target.isEmpty()); b->setToolTip(target.isEmpty()?QString("Shift+%1 - configure in %2").arg(i+1).arg(moveConfigFilePath()):QString("Shift+%1 - %2").arg(i+1).arg(target));}}

QString MainWindow::uniqueMoveDestination(const QString&dir,const QString&fileName)const{QDir d(dir); QString dest=d.filePath(fileName); if(!QFileInfo::exists(dest))return dest; QFileInfo fi(fileName); QString base=fi.completeBaseName(); QString ext=fi.suffix(); for(int n=1;n<10000;++n){QString candidate=ext.isEmpty()?QString("%1 (%2)").arg(base).arg(n):QString("%1 (%2).%3").arg(base).arg(n).arg(ext); dest=d.filePath(candidate); if(!QFileInfo::exists(dest))return dest;} return d.filePath(QString::number(QDateTime::currentMSecsSinceEpoch())+"_"+fileName);}

bool MainWindow::moveFileToDirectory(const QString&src,const QString&targetDir,QString&destOut){QDir().mkpath(targetDir); destOut=uniqueMoveDestination(targetDir,QFileInfo(src).fileName()); if(QFile::rename(src,destOut))return true; if(QFile::copy(src,destOut)){QFile::remove(src); return true;} return false;}

void MainWindow::appendMoveLog(const QString&src,const QString&dest,const QString&buttonName){QFile f(moveLogFilePath()); if(f.open(QIODevice::Append|QIODevice::Text)){QTextStream s(&f); s<<QDateTime::currentDateTime().toString(Qt::ISODate)<<"\t"<<buttonName<<"\t"<<src<<"\t"<<dest<<"\n";} refreshMoveLogView();}

void MainWindow::moveSelectedFileToTarget(int idx){if(idx<0||idx>=6)return; loadMoveConfig(); QString target=moveButtonPaths.value(idx); if(target.isEmpty())return; int r=currentRow(); if(r<0||r>=playlistModel->count())return; QString src=playlistModel->pathAt(r); const QString droppedFolderRoot=playlistModel->folderDropRootAt(r); const bool droppedFolderLastItem=playlistModel->isLastItemInFolderDropGroup(r); if(src.isEmpty()||!QFileInfo::exists(src)){playlistModel->removeRowAt(r); savePlaylistState(); return;} bool wasCurrent=(QFileInfo(mpvWidget->currentFilePath()).absoluteFilePath()==QFileInfo(src).absoluteFilePath()); if(wasCurrent)closeCurrentFile(); QString dest; if(moveFileToDirectory(src,target,dest)){if(droppedFolderLastItem&&!droppedFolderRoot.isEmpty()&&droppedFolderIsCompletelyEmptyAfterMove(droppedFolderRoot))QFile::moveToTrash(droppedFolderRoot);appendMoveLog(src,dest,moveButtonNames.value(idx,QString("Move %1").arg(idx+1))); playlistModel->removeRowAt(r); if(playlistModel->count()>0){int next=qBound(0,r,playlistModel->count()-1); setPlaylistCurrentSourceRow(next); playPlaylistRow(next);} else savePlaylistState();}}

