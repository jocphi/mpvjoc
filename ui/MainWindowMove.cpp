#include <functional>
#include "family/FamilyMoveEvaluator.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QFrame>
#include <QHBoxLayout>
#include <QShortcut>
#include <QVBoxLayout>
#include <filesystem>
#include <system_error>
#include <QLabel>
#include "family/FamilyFilenameParser.h"
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
#include "media/ThumbnailManager.h"
#include "ui/TimelineWidget.h"
#include <QTimer>

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



namespace {
struct SafeFamilyMoveResult {
    bool ok=false;
    QString destination;
    QString error;
};





SafeFamilyMoveResult moveFamilyFileSafely(const QString&sourcePath,const QString&destinationDirectory,const std::function<void(int)>&progress)
{
    SafeFamilyMoveResult result;
    const QFileInfo sourceInfo(sourcePath);
    const QFileInfo directoryInfo(destinationDirectory);
    if(!sourceInfo.exists()||!sourceInfo.isFile()){
        result.error=QStringLiteral("Source is not an existing regular file:\n%1").arg(sourcePath);return result;
    }
    if(!directoryInfo.exists()||!directoryInfo.isDir()){
        result.error=QStringLiteral("Family destination is unavailable or not a directory:\n%1").arg(destinationDirectory);return result;
    }
    if(!directoryInfo.isWritable()){
        result.error=QStringLiteral("Family destination is not writable:\n%1").arg(destinationDirectory);return result;
    }

    const QString sourceAbs=sourceInfo.absoluteFilePath();
    const QString destination=QDir(directoryInfo.absoluteFilePath()).filePath(sourceInfo.fileName());
    const QFileInfo destinationInfo(destination);
    if(destinationInfo.exists()){
        result.error=QStringLiteral("Destination already exists; nothing was overwritten:\n%1").arg(destination);return result;
    }
    if(QFileInfo(sourceAbs).canonicalFilePath()==QFileInfo(destination).canonicalFilePath()||sourceAbs==QFileInfo(destination).absoluteFilePath()){
        result.error=QStringLiteral("Source and destination are the same file.");return result;
    }

    std::error_code renameError;
    std::filesystem::rename(std::filesystem::path(QFile::encodeName(sourceAbs).constData()),
                            std::filesystem::path(QFile::encodeName(destination).constData()),
                            renameError);
    if(!renameError){if(progress)progress(100);result.ok=true;result.destination=destination;return result;}

    if(renameError!=std::errc::cross_device_link){
        result.error=QStringLiteral("Could not move file:\n%1\n\n%2")
                         .arg(sourceAbs,QString::fromStdString(renameError.message()));
        return result;
    }

    const QString temporary=QDir(directoryInfo.absoluteFilePath()).filePath(
        QStringLiteral(".%1.mpvjoc-moving-%2-%3")
            .arg(sourceInfo.fileName()).arg(QCoreApplication::applicationPid()).arg(QDateTime::currentMSecsSinceEpoch()));
    QFile input(sourceAbs),output(temporary);
    if(!input.open(QIODevice::ReadOnly)){
        result.error=QStringLiteral("Could not open source for cross-filesystem copy:\n%1").arg(input.errorString());return result;
    }
    if(!output.open(QIODevice::WriteOnly|QIODevice::NewOnly)){
        result.error=QStringLiteral("Could not create temporary destination:\n%1").arg(output.errorString());return result;
    }

    QByteArray buffer(4*1024*1024,Qt::Uninitialized);
    const qint64 totalBytes=sourceInfo.size();
    qint64 copiedBytes=0;
    int lastProgress=-1;
    if(progress)progress(0);
    bool copyOk=true;
    while(true){
        const qint64 read=input.read(buffer.data(),buffer.size());
        if(read<0){copyOk=false;result.error=QStringLiteral("Read failed during cross-filesystem copy: %1").arg(input.errorString());break;}
        if(read==0)break;
        qint64 written=0;
        while(written<read){const qint64 n=output.write(buffer.constData()+written,read-written);if(n<=0){copyOk=false;result.error=QStringLiteral("Write failed during cross-filesystem copy: %1").arg(output.errorString());break;}written+=n;copiedBytes+=n;const int currentProgress=totalBytes>0?int((copiedBytes*100)/totalBytes):100;if(progress&&currentProgress!=lastProgress){lastProgress=currentProgress;progress(currentProgress);}}
        if(!copyOk)break;
    }
    if(copyOk&&!output.flush()){copyOk=false;result.error=QStringLiteral("Could not flush temporary destination: %1").arg(output.errorString());}
    output.close();input.close();
    if(copyOk&&QFileInfo(temporary).size()!=sourceInfo.size()){copyOk=false;result.error=QStringLiteral("Copied byte length does not match source; original was preserved.");}
    if(!copyOk){QFile::remove(temporary);return result;}
    if(!QFile::rename(temporary,destination)){result.error=QStringLiteral("Could not finalize temporary destination; original was preserved.");QFile::remove(temporary);return result;}
    if(!QFile::remove(sourceAbs)){result.error=QStringLiteral("Destination copy succeeded, but the original could not be removed. Both files were preserved for safety.\n%1").arg(sourceAbs);return result;}

    if(progress)progress(100);result.ok=true;result.destination=destination;return result;
}
}

void MainWindow::showFamilyDestinationPreview(const QString&text,bool error)
{
    if(!shortcutHelpOverlay)return;
    shortcutHelpOverlay->setText(text);
    shortcutHelpOverlay->setWordWrap(true);
    shortcutHelpOverlay->setAlignment(Qt::AlignLeft|Qt::AlignTop);
    shortcutHelpOverlay->setStyleSheet(error
        ?QStringLiteral("QLabel{background:rgba(70,16,16,235);color:#ffffff;border:2px solid #d05050;border-radius:8px;padding:14px;}")
        :QStringLiteral("QLabel{background:rgba(12,42,20,235);color:#ffffff;border:2px solid #39a857;border-radius:8px;padding:14px;}"));
    const int preferredWidth=qBound(420,width()*2/3,760);
    shortcutHelpOverlay->setFixedWidth(preferredWidth);
    shortcutHelpOverlay->adjustSize();
    const int x=qMax(8,(width()-shortcutHelpOverlay->width())/2);
    const int y=qMax(8,(height()-shortcutHelpOverlay->height())/2);
    shortcutHelpOverlay->move(x,y);
    shortcutHelpOverlay->show();
    shortcutHelpOverlay->raise();
}

void MainWindow::moveSelectedFileToFamilyDestination()
{
    if(currentPlaylistLocked()){
        showFamilyDestinationPreview(QStringLiteral("Family move\n\nThe active playlist is locked. Unlock it before moving files."),true);
        return;
    }

    PlaylistModel*sourceModel=playlistModel;
    int row=currentRow();
    QString sourcePath=selectedPath();
    if(sourcePath.isEmpty()&&mpvWidget){
        sourcePath=mpvWidget->currentFilePath();
        if(!sourcePath.isEmpty()){
            for(int r=0;r<sourceModel->count();++r)
                if(QFileInfo(sourceModel->pathAt(r)).absoluteFilePath()==QFileInfo(sourcePath).absoluteFilePath()){row=r;break;}
        }
    }
    if(!sourceModel||row<0||sourcePath.isEmpty()){
        showFamilyDestinationPreview(QStringLiteral("Family move\n\nNo selected or currently playing playlist file is available."),true);
        return;
    }

    const QString sourceAbs=QFileInfo(sourcePath).absoluteFilePath();
    const FamilyMoveEvaluation evaluation=FamilyMoveEvaluator::evaluate(sourceAbs,familyDestinations);
    if(evaluation.moveState!=FamilyMoveEvaluation::MoveState::Ready){
        showFamilyDestinationPreview(QStringLiteral("Family move blocked\n\n%1").arg(evaluation.reason),true);
        return;
    }

    if(activeFamilyMoveModel==sourceModel&&activeFamilyMoveSourcePath==sourceAbs)
        return;
    for(const PendingFamilyMove&pending:familyMoveQueue)
        if(pending.model==sourceModel&&pending.sourcePath==sourceAbs)
            return;

    const bool wasCurrent=mpvWidget&&
        QFileInfo(mpvWidget->currentFilePath()).absoluteFilePath()==sourceAbs;
    if(wasCurrent){
        suppressNextEndFileAdvance=true;
        closeCurrentFile();
        if(sourceModel==playlistModel&&sourceModel->count()>1){
            const int playbackRow=(row+1<sourceModel->count())?row+1:row-1;
            playPlaylistRow(playbackRow);
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }

    sourceModel->setFamilyMoveProgressAt(row,0);
    familyMoveQueue.push_back(PendingFamilyMove{QPointer<PlaylistModel>(sourceModel),sourceAbs});
    if(playlistView&&playlistView->viewport())playlistView->viewport()->update();

    if(!familyMoveBusy)
        QTimer::singleShot(0,this,[this]{processNextFamilyMove();});
}


void MainWindow::processNextFamilyMove()
{
    if(familyMoveBusy)return;

    while(!familyMoveQueue.isEmpty()){
        const PendingFamilyMove pending=familyMoveQueue.takeFirst();
        PlaylistModel*model=pending.model.data();
        if(!model)continue;

        int row=-1;
        for(int r=0;r<model->count();++r){
            if(QFileInfo(model->pathAt(r)).absoluteFilePath()==pending.sourcePath){row=r;break;}
        }
        if(row<0)continue;

        const FamilyMoveEvaluation evaluation=
            FamilyMoveEvaluator::evaluate(pending.sourcePath,familyDestinations);
        if(evaluation.moveState!=FamilyMoveEvaluation::MoveState::Ready){
            model->setFamilyMoveProgressAt(row,-1);
            showFamilyDestinationPreview(QStringLiteral("Queued family move blocked\n\n%1").arg(evaluation.reason),true);
            continue;
        }

        familyMoveBusy=true;
        activeFamilyMoveModel=model;
        activeFamilyMoveSourcePath=pending.sourcePath;

        const QString droppedFolderRoot=model->folderDropRootAt(row);
        const bool droppedFolderLastItem=model->isLastItemInFolderDropGroup(row);
        model->setFamilyMoveProgressAt(row,0);
        if(model==playlistModel&&playlistView&&playlistView->viewport())
            playlistView->viewport()->repaint();

        const SafeFamilyMoveResult moveResult=
            moveFamilyFileSafely(pending.sourcePath,evaluation.destinationFolder,
                [this,modelPath=QPointer<PlaylistModel>(model),sourcePath=pending.sourcePath,lastShown=-1](int percent) mutable {
                    PlaylistModel*progressModel=modelPath.data();
                    if(!progressModel)return;
                    int progressRow=-1;
                    for(int r=0;r<progressModel->count();++r)
                        if(QFileInfo(progressModel->pathAt(r)).absoluteFilePath()==sourcePath){progressRow=r;break;}
                    if(progressRow<0)return;
                    const int normalized=qBound(0,percent,100);
                    if(normalized==lastShown)return;
                    lastShown=normalized;
                    progressModel->setFamilyMoveProgressAt(progressRow,normalized);
                    if(progressModel==playlistModel&&playlistView&&playlistView->viewport()){
                        playlistView->viewport()->update();
                        playlistView->viewport()->repaint();
                    }
                    // Permit Ctrl+M and playback control while a synchronous
                    // cross-filesystem copy is progressing. familyMoveBusy
                    // prevents re-entering the active transfer; new requests
                    // are appended to familyMoveQueue.
                    QCoreApplication::processEvents(QEventLoop::AllEvents);
                });

        int finalRow=-1;
        for(int r=0;r<model->count();++r)
            if(QFileInfo(model->pathAt(r)).absoluteFilePath()==pending.sourcePath){finalRow=r;break;}

        if(!moveResult.ok){
            if(finalRow>=0)model->setFamilyMoveProgressAt(finalRow,-1);
            showFamilyDestinationPreview(QStringLiteral("Family move failed\n\n%1").arg(moveResult.error),true);
        }else{
            if(droppedFolderLastItem&&!droppedFolderRoot.isEmpty()&&
               droppedFolderIsCompletelyEmptyAfterMove(droppedFolderRoot))
                QFile::moveToTrash(droppedFolderRoot);

            if(finalRow>=0)model->removeRowAt(finalRow);
            if(model==playlistModel&&model->count()>0)
                setPlaylistCurrentSourceRow(qBound(0,finalRow,model->count()-1),true);
            savePlaylistState();
            if(model==playlistModel)updatePlaylistSummary();
        }

        activeFamilyMoveModel.clear();
        activeFamilyMoveSourcePath.clear();
        familyMoveBusy=false;

        if(!familyMoveQueue.isEmpty())
            QTimer::singleShot(0,this,[this]{processNextFamilyMove();});
        return;
    }
}

void MainWindow::moveSelectedFileToTarget(int idx){if(currentPlaylistLocked())return;if(idx<0||idx>=6)return; loadMoveConfig(); QString target=moveButtonPaths.value(idx); if(target.isEmpty())return; int r=currentRow(); if(r<0||r>=playlistModel->count())return; QString src=playlistModel->pathAt(r); const QString droppedFolderRoot=playlistModel->folderDropRootAt(r); const bool droppedFolderLastItem=playlistModel->isLastItemInFolderDropGroup(r); if(src.isEmpty()||!QFileInfo::exists(src)){playlistModel->removeRowAt(r); savePlaylistState(); return;} bool wasCurrent=(QFileInfo(mpvWidget->currentFilePath()).absoluteFilePath()==QFileInfo(src).absoluteFilePath()); if(wasCurrent)closeCurrentFile(); QString dest; if(moveFileToDirectory(src,target,dest)){if(droppedFolderLastItem&&!droppedFolderRoot.isEmpty()&&droppedFolderIsCompletelyEmptyAfterMove(droppedFolderRoot))QFile::moveToTrash(droppedFolderRoot);appendMoveLog(src,dest,moveButtonNames.value(idx,QString("Move %1").arg(idx+1))); playlistModel->removeRowAt(r); if(playlistModel->count()>0){int next=qBound(0,r,playlistModel->count()-1); setPlaylistCurrentSourceRow(next); playPlaylistRow(next);} else savePlaylistState();}}

