#include "MainWindow.h"
#include "mpv/MpvWidget.h"
#include "playlist/PlaylistModel.h"
#include "playlist/PlaylistDelegate.h"
#include "media/MetadataProbeManager.h"
#include "media/ThumbnailManager.h"
#include "ui/TimelineWidget.h"
#include "util/Utils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QSplitter>
#include <QFileInfo>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDragMoveEvent>
#include <QEvent>
#include <QMimeData>
#include <QUrl>
#include <QListView>
#include <QTabWidget>
#include <QTextEdit>
#include <QAbstractItemView>
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>
#include <QSet>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCloseEvent>
#include <QTimer>
#include <QMenu>
#include <QClipboard>
#include <QDesktopServices>
#include <QDateTime>
#include <QTextStream>
#include <QApplication>
#include <QFont>
#include <QShortcut>
#include <QKeySequence>

MainWindow::MainWindow(){ setWindowFlag(Qt::FramelessWindowHint, true); mpvWidget=new MpvWidget(this); playlistModel=new PlaylistModel(this); metadataProbe=new MetadataProbeManager(this); thumbnailManager=new ThumbnailManager(this); playlistView=new QListView(this); playlistView->setModel(playlistModel); playlistView->setItemDelegate(new PlaylistDelegate(playlistView)); playlistView->setUniformItemSizes(true); playlistView->setSelectionMode(QAbstractItemView::SingleSelection); playlistView->setEditTriggers(QAbstractItemView::NoEditTriggers); playlistView->setContextMenuPolicy(Qt::CustomContextMenu); playlistView->setDragEnabled(true); playlistView->setAcceptDrops(true); playlistView->setDropIndicatorShown(true); playlistView->setDragDropMode(QAbstractItemView::InternalMove); playlistView->setDefaultDropAction(Qt::MoveAction); playlistView->setDragDropOverwriteMode(false);
 auto*openButton=new QPushButton("Open files",this); playPauseButton=new QPushButton("Play/Pause",this); muteButton=new QPushButton("Vol 100%",this); timeline=new TimelineWidget(this); timeLabel=new QLabel("00:00:00 / 00:00:00",this); timeLabel->hide(); auto*leftL=new QVBoxLayout; leftL->setContentsMargins(0,0,0,0); leftL->setSpacing(1); leftL->addWidget(mpvWidget,1); leftL->addWidget(timeline); auto*leftW=new QWidget(this); leftW->setLayout(leftL); auto*playlistControls=new QHBoxLayout; playlistControls->setContentsMargins(1,1,1,1); playlistControls->setSpacing(1); playlistControls->addWidget(openButton); playlistControls->addWidget(playPauseButton); playlistControls->addWidget(muteButton); scaleHalfButton=new QPushButton("½x",this); scaleOneButton=new QPushButton("1x",this); scaleTwoButton=new QPushButton("2x",this); clipButton=new QPushButton("clip",this); autoPlayButton=new QPushButton("auto",this); scaleHalfButton->setFixedWidth(42); scaleOneButton->setFixedWidth(42); scaleTwoButton->setFixedWidth(42); clipButton->setFixedWidth(52); autoPlayButton->setFixedWidth(62); playlistControls->addWidget(scaleHalfButton); playlistControls->addWidget(scaleOneButton); playlistControls->addWidget(scaleTwoButton); playlistControls->addWidget(clipButton); playlistControls->addWidget(autoPlayButton); playlistControls->addStretch(1); auto*playlistTab=new QWidget(this); auto*playlistTabLayout=new QVBoxLayout; playlistSummaryLabel=new QLabel("0 files  •  0.00 MB  •  00:00:00",this); playlistSummaryLabel->setContentsMargins(4,1,4,1); playlistSummaryLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);  playlistTabLayout->setContentsMargins(1,1,1,1); playlistTabLayout->setSpacing(1); playlistTabLayout->addWidget(playlistSummaryLabel); playlistTabLayout->addWidget(playlistView,1); playlistTabLayout->addLayout(playlistControls); playlistTab->setLayout(playlistTabLayout); auto*moveControls=new QVBoxLayout; moveControls->setContentsMargins(1,1,1,1); moveControls->setSpacing(1); for(int i=0;i<6;++i){auto*b=new QPushButton(QString("Move %1").arg(i+1),this); b->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed); moveButtons<<b; moveControls->addWidget(b); connect(b,&QPushButton::clicked,this,[this,i]{moveSelectedFileToTarget(i);});} moveControls->addStretch(1); moveLogView=new QTextEdit(this); moveLogView->setReadOnly(true); moveLogView->setLineWrapMode(QTextEdit::NoWrap); auto*moveTab=new QWidget(this); auto*moveTabLayout=new QVBoxLayout; moveTabLayout->setContentsMargins(1,1,1,1); moveTabLayout->setSpacing(1); moveTabLayout->addLayout(moveControls); moveTabLayout->addWidget(moveLogView,1); moveTab->setLayout(moveTabLayout); rightTabs=new QTabWidget(this); rightTabs->addTab(playlistTab,"Playlist"); rightTabs->addTab(moveTab,"Move"); auto*rightL=new QVBoxLayout; rightL->setContentsMargins(1,1,1,1); rightL->setSpacing(1); rightL->addWidget(rightTabs,1); auto*rightW=new QWidget(this); rightW->setLayout(rightL); playlistSplitter=new QSplitter(this); playlistSplitter->setHandleWidth(1); playlistSplitter->addWidget(leftW); playlistSplitter->addWidget(rightW); playlistSplitter->setStretchFactor(0,5); playlistSplitter->setStretchFactor(1,1); setCentralWidget(playlistSplitter); resize(1280,720); setWindowTitle("mpvjoc"); setAcceptDrops(true); mpvWidget->setAcceptDrops(true); mpvWidget->installEventFilter(this); playlistView->viewport()->setAcceptDrops(true); playlistView->viewport()->installEventFilter(this);
 auto*quitShortcut=new QShortcut(QKeySequence(Qt::Key_Q),this); quitShortcut->setContext(Qt::WindowShortcut); connect(quitShortcut,&QShortcut::activated,this,&MainWindow::close);
 connect(openButton,&QPushButton::clicked,this,&MainWindow::openFiles); connect(playPauseButton,&QPushButton::clicked,this,&MainWindow::playPauseOrStartSelected); connect(muteButton,&QPushButton::clicked,this,[this]{mpvWidget->toggleMute();}); connect(scaleHalfButton,&QPushButton::clicked,this,[this]{setMaxVideoScale(0.5);}); connect(scaleOneButton,&QPushButton::clicked,this,[this]{setMaxVideoScale(1.0);}); connect(scaleTwoButton,&QPushButton::clicked,this,[this]{setMaxVideoScale(2.0);}); connect(clipButton,&QPushButton::clicked,this,[this]{setClipVideoToScale(!clipVideoToScale);}); connect(autoPlayButton,&QPushButton::clicked,this,[this]{setAutoPlayNextEnabled(!autoPlayNextEnabled);}); connect(playlistView,&QListView::doubleClicked,this,[this](const QModelIndex&i){playPlaylistRow(i.row());}); connect(playlistModel,&QAbstractItemModel::rowsMoved,this,[this]{savePlaylistState();updatePlaylistSummary();}); connect(playlistModel,&QAbstractItemModel::rowsInserted,this,[this]{updatePlaylistSummary();}); connect(playlistModel,&QAbstractItemModel::rowsRemoved,this,[this]{updatePlaylistSummary();}); connect(playlistModel,&QAbstractItemModel::modelReset,this,[this]{updatePlaylistSummary();}); connect(playlistModel,&QAbstractItemModel::dataChanged,this,[this]{updatePlaylistSummary();}); connect(playlistView,&QListView::customContextMenuRequested,this,&MainWindow::showPlaylistContextMenu); connect(playlistSplitter,&QSplitter::splitterMoved,this,[this]{savePlaylistState();}); fastPlaybackTimer=new QTimer(this); fastPlaybackTimer->setInterval(1000); connect(fastPlaybackTimer,&QTimer::timeout,this,&MainWindow::fastPlaybackTick);
 metadataProbe->metadataReady=[this](const QString&p,const MediaMetadata&m){playlistModel->setMetadataForPath(p,m);if(!restoringPlaybackState)savePlaylistState();}; metadataProbe->metadataFailed=[this](const QString&p){playlistModel->markProbeFailed(p);if(!restoringPlaybackState)savePlaylistState();}; thumbnailManager->thumbnailReady=[this](const QString&p,const QString&t){playlistModel->setThumbnailForPath(p,t);if(!restoringPlaybackState)savePlaylistState();}; thumbnailManager->thumbnailFailed=[this](const QString&p){playlistModel->markThumbnailFailed(p);if(!restoringPlaybackState)savePlaylistState();};
 mpvWidget->onPosition=[this](double s){onPositionChanged(s);}; mpvWidget->onDuration=[this](double s){onDurationChanged(s);}; mpvWidget->onPause=[this](bool p){playPauseButton->setText(p?"Play":"Pause");mpvWidget->showPlaybackOverlay(p);}; mpvWidget->onFile=[this](QString p){timeline->setTitle(QFileInfo(p).fileName());selectPath(p);QTimer::singleShot(300,this,[this]{if(mpvWidget)mpvWidget->showScaleOverlay();});}; mpvWidget->onEndFile=[this]{if(suppressNextEndFileAdvance){suppressNextEndFileAdvance=false;return;}if(autoPlayNextEnabled&&playlistModel->count()>1)playNextPlaylistFile();}; mpvWidget->onVolume=[this](double v){if(restoringPlaybackState)return;currentVolume=v;updateMuteVolumeButton();if(mpvWidget)mpvWidget->showVolumeOverlay(currentVolume,currentMuted);savePlaylistState();}; mpvWidget->onMute=[this](bool m){if(restoringPlaybackState)return;currentMuted=m;updateMuteVolumeButton();if(mpvWidget)mpvWidget->showVolumeOverlay(currentVolume,currentMuted);savePlaylistState();}; timeline->seekRequested=[this](double s){mpvWidget->seekAbsolute(s);}; timeline->previewPositionChanged=[this](double s){updateTimeLabel(s,duration);}; updateScaleButtons(); updateClipButton(); updateAutoPlayButton(); mpvWidget->setMaxVideoScale(maxVideoScale); mpvWidget->setClipVideoToScale(clipVideoToScale); updateMuteVolumeButton(); updateMoveButtons(); loadPlaylistState(); loadMoveConfig(); updatePlaylistSummary(); QTimer::singleShot(900,this,[this]{restoringPlaybackState=false;}); probeMissingMetadata(); generateMissingThumbnails(); }
MainWindow::~MainWindow(){savePlaylistState();}
void MainWindow::closeEvent(QCloseEvent*e){savePlaylistState();QMainWindow::closeEvent(e);} void MainWindow::updateMuteVolumeButton(){ if(!muteButton)return; muteButton->setText(QString("Vol %1%").arg(int(currentVolume+0.5))); QFont f=muteButton->font(); f.setStrikeOut(currentMuted); muteButton->setFont(f); muteButton->setToolTip(currentMuted?"Muted - click to unmute":"Click to mute"); }
void MainWindow::playPauseOrStartSelected(){ if(mpvWidget->currentFilePath().isEmpty()&&playlistModel->count()>0){playPlaylistRow(currentRow());return;} mpvWidget->togglePause();}
void MainWindow::closeCurrentFile(){if(fastPlaybackTimer)fastPlaybackTimer->stop();warpPlaybackMode=false;updateWarpOverlay();suppressNextEndFileAdvance=true;mpvWidget->stopPlayback();position=0;duration=0;timeline->setPosition(0);timeline->setDuration(0);timeline->setTitle(QString());updateTimeLabel(0,0);playPauseButton->setText("Play");} void MainWindow::moveSelectedFileToTrash(){int r=currentRow(); if(r<0||r>=playlistModel->count())return; QString p=playlistModel->pathAt(r); if(p.isEmpty())return; bool wasCurrent=(QFileInfo(mpvWidget->currentFilePath()).absoluteFilePath()==QFileInfo(p).absoluteFilePath()); if(wasCurrent)closeCurrentFile(); bool ok=true; if(QFileInfo::exists(p))ok=QFile::moveToTrash(p); if(ok||!QFileInfo::exists(p)){playlistModel->removeRowAt(r); if(playlistModel->count()>0){int next=qBound(0,r,playlistModel->count()-1); playlistView->setCurrentIndex(playlistModel->index(next,0)); playPlaylistRow(next);} else savePlaylistState();} }
QString MainWindow::moveConfigFilePath()const{QFileInfo info(stateFilePath()); return QDir(info.absolutePath()).filePath("move-buttons.conf");} QString MainWindow::moveLogFilePath()const{QFileInfo info(stateFilePath()); return QDir(info.absolutePath()).filePath("move-log.tsv");}
void MainWindow::ensureMoveLists(){while(moveButtonNames.size()<6)moveButtonNames<<QString("Move %1").arg(moveButtonNames.size()+1); while(moveButtonPaths.size()<6)moveButtonPaths<<QString();}
void MainWindow::createDefaultMoveConfig(){QFile f(moveConfigFilePath()); if(f.exists())return; if(f.open(QIODevice::WriteOnly|QIODevice::Text)){QTextStream s(&f); s<<"move_button_count=6\n"; for(int i=1;i<=6;++i){s<<"move_button_"<<i<<"_name=Move "<<i<<"\n"; s<<"move_button_"<<i<<"_path=\n";}}}
void MainWindow::loadMoveConfig(){ensureMoveLists(); createDefaultMoveConfig(); QFile f(moveConfigFilePath()); if(f.open(QIODevice::ReadOnly|QIODevice::Text)){while(!f.atEnd()){QString line=QString::fromUtf8(f.readLine()).trimmed(); if(line.isEmpty()||line.startsWith('#'))continue; int eq=line.indexOf('='); if(eq<0)continue; QString key=line.left(eq).trimmed(); QString val=line.mid(eq+1).trimmed(); if(key=="move_button_count")moveButtonCount=qBound(1,val.toInt(),6); for(int i=0;i<6;++i){QString n=QString("move_button_%1_name").arg(i+1); QString p=QString("move_button_%1_path").arg(i+1); if(key==n)moveButtonNames[i]=val; else if(key==p)moveButtonPaths[i]=val;}}} updateMoveButtons(); refreshMoveLogView();}
void MainWindow::refreshMoveLogView(){if(!moveLogView)return; QFile f(moveLogFilePath()); if(f.open(QIODevice::ReadOnly|QIODevice::Text))moveLogView->setPlainText(QString::fromUtf8(f.readAll())); else moveLogView->setPlainText(QString());}
void MainWindow::updateMoveButtons(){ensureMoveLists(); for(int i=0;i<moveButtons.size();++i){auto*b=moveButtons[i]; bool visible=i<moveButtonCount; b->setVisible(visible); QString name=moveButtonNames.value(i,QString("Move %1").arg(i+1)); QString target=moveButtonPaths.value(i); b->setText(name.isEmpty()?QString("Move %1").arg(i+1):name); b->setEnabled(visible&&!target.isEmpty()); b->setToolTip(target.isEmpty()?QString("Shift+%1 - configure in %2").arg(i+1).arg(moveConfigFilePath()):QString("Shift+%1 - %2").arg(i+1).arg(target));}}
QString MainWindow::uniqueMoveDestination(const QString&dir,const QString&fileName)const{QDir d(dir); QString dest=d.filePath(fileName); if(!QFileInfo::exists(dest))return dest; QFileInfo fi(fileName); QString base=fi.completeBaseName(); QString ext=fi.suffix(); for(int n=1;n<10000;++n){QString candidate=ext.isEmpty()?QString("%1 (%2)").arg(base).arg(n):QString("%1 (%2).%3").arg(base).arg(n).arg(ext); dest=d.filePath(candidate); if(!QFileInfo::exists(dest))return dest;} return d.filePath(QString::number(QDateTime::currentMSecsSinceEpoch())+"_"+fileName);}
bool MainWindow::moveFileToDirectory(const QString&src,const QString&targetDir,QString&destOut){QDir().mkpath(targetDir); destOut=uniqueMoveDestination(targetDir,QFileInfo(src).fileName()); if(QFile::rename(src,destOut))return true; if(QFile::copy(src,destOut)){QFile::remove(src); return true;} return false;}
double MainWindow::normalizedMaxVideoScale(double scale)const{ if(scale<0.75)return 0.5; if(scale<1.5)return 1.0; return 2.0;}
void MainWindow::updateScaleButtons(){ auto apply=[this](QPushButton*b,bool active){ if(!b)return; QFont f=b->font(); f.setStrikeOut(!active); b->setFont(f); b->setEnabled(true); }; apply(scaleHalfButton,maxVideoScale==0.5); apply(scaleOneButton,maxVideoScale==1.0); apply(scaleTwoButton,maxVideoScale==2.0); updateClipButton();}
void MainWindow::updateClipButton(){if(!clipButton)return;clipButton->setText(clipVideoToScale?"clip":"scale");clipButton->setToolTip(clipVideoToScale?"Clip enabled: exact scale may clip video":"Scale enabled: scale down only when needed to show full video");}
void MainWindow::toggleShortcutHelpOverlay(){
    if(shortcutHelpOverlay&&shortcutHelpOverlay->isVisible()){
        shortcutHelpOverlay->hide();
        return;
    }
    if(!shortcutHelpOverlay){
        shortcutHelpOverlay=new QLabel(this);
        shortcutHelpOverlay->setTextFormat(Qt::PlainText);
        shortcutHelpOverlay->setWordWrap(false);
        shortcutHelpOverlay->setAlignment(Qt::AlignLeft|Qt::AlignTop);
        shortcutHelpOverlay->setAttribute(Qt::WA_StyledBackground,true);
        QFont f=shortcutHelpOverlay->font();
        f.setFamily(QStringLiteral("monospace"));
        f.setPointSize(10);
        shortcutHelpOverlay->setFont(f);
        shortcutHelpOverlay->setStyleSheet(QStringLiteral(
            "QLabel{background:rgba(0,0,0,215);color:#eeeeee;border:2px solid #66ccff;border-radius:8px;padding:14px;}"));
        shortcutHelpOverlay->setText(QStringLiteral(
            "Keyboard shortcuts\n"
            "\n"
            "Playback\n"
            "  P          Play/Pause, or leave Warp mode if active\n"
            "  Ctrl+P     Toggle Warp mode\n"
            "  Left/Right Change Warp factor while Warp mode is active\n"
            "  F          Next frame\n"
            "  D          Previous frame\n"
            "  M          Toggle mute\n"
            "  + / -      Volume up/down\n"
            "\n"
            "Scale / display\n"
            "  1/2        Set scale to 50% with the 1/2 key\n"
            "  1          Set scale to 100%\n"
            "  2          Set scale to 200%\n"
            "  0          Toggle clip/scale\n"
            "  F1         Toggle persistent info overlay\n"
            "  F12        Show/hide this help\n"
            "  Esc        Hide this help\n"
            "\n"
            "Playlist\n"
            "  Enter      Play selected\n"
            "  PageDown   Next item\n"
            "  PageUp     Previous item\n"
            "  Delete     Remove from playlist\n"
            "  Shift+Del  Move file to trash\n"
            "  Ctrl+Del   Clear playlist\n"
            "  Alt+Up     Move selected item up\n"
            "  Alt+Down   Move selected item down\n"
            "\n"
            "Seeking\n"
            "  .          Forward 60 seconds\n"
            "  ,          Back 60 seconds\n"
            "  Ctrl+.     Forward 10 seconds\n"
            "  Ctrl+,     Back 10 seconds\n"
            "  :          Forward 30 seconds\n"
            "  ;          Back 30 seconds"));
    }
    const int w=qMin(width()-40,620);
    const int h=qMin(height()-40,520);
    shortcutHelpOverlay->setGeometry((width()-w)/2,(height()-h)/2,w,h);
    shortcutHelpOverlay->show();
    shortcutHelpOverlay->raise();
}
void MainWindow::setClipVideoToScale(bool crop){clipVideoToScale=crop;if(mpvWidget){mpvWidget->setClipVideoToScale(clipVideoToScale);mpvWidget->showScaleOverlay();}updateClipButton();if(!restoringPlaybackState)savePlaylistState();}
void MainWindow::setMaxVideoScale(double scale){ maxVideoScale=normalizedMaxVideoScale(scale); if(mpvWidget){mpvWidget->setMaxVideoScale(maxVideoScale);mpvWidget->showScaleOverlay();} updateScaleButtons(); if(!restoringPlaybackState)savePlaylistState();}
void MainWindow::appendMoveLog(const QString&src,const QString&dest,const QString&buttonName){QFile f(moveLogFilePath()); if(f.open(QIODevice::Append|QIODevice::Text)){QTextStream s(&f); s<<QDateTime::currentDateTime().toString(Qt::ISODate)<<"\t"<<buttonName<<"\t"<<src<<"\t"<<dest<<"\n";} refreshMoveLogView();}
void MainWindow::setAutoPlayNextEnabled(bool enabled){autoPlayNextEnabled=enabled;updateAutoPlayButton();if(!restoringPlaybackState)savePlaylistState();}
void MainWindow::updateAutoPlayButton(){if(!autoPlayButton)return;autoPlayButton->setText(autoPlayNextEnabled?"auto":"manual");autoPlayButton->setToolTip(autoPlayNextEnabled?"Autoplay next playlist item enabled":"Autoplay next playlist item disabled");}
void MainWindow::updateWarpOverlay(){if(mpvWidget)mpvWidget->setWarpOverlay(warpPlaybackMode,warpFactor);}
void MainWindow::setWarpFactor(int factor){warpFactor=qBound(1,factor,9);updateWarpOverlay();if(warpPlaybackMode&&mpvWidget)mpvWidget->showWarpFeedback(warpFactor);}
void MainWindow::toggleWarpPlaybackMode(){warpPlaybackMode=!warpPlaybackMode;if(warpPlaybackMode){setAutoPlayNextEnabled(false);if(mpvWidget->currentFilePath().isEmpty()&&playlistModel->count()>0)playPlaylistRow(currentRow());mpvWidget->setPause(false);if(fastPlaybackTimer)fastPlaybackTimer->start();if(mpvWidget)mpvWidget->showWarpFeedback(warpFactor);}else{if(fastPlaybackTimer)fastPlaybackTimer->stop();setAutoPlayNextEnabled(true);mpvWidget->setPause(false);mpvWidget->showPlaybackOverlay(false);}updateWarpOverlay();}
void MainWindow::fastPlaybackTick(){
    if(!warpPlaybackMode||!mpvWidget)return;
    if(mpvWidget->currentFilePath().isEmpty()){
        warpPlaybackMode=false;
        if(fastPlaybackTimer)fastPlaybackTimer->stop();
        updateWarpOverlay();
        return;
    }
    const double skipSeconds=10.0*warpFactor;
    if(duration>0.0&&position>=0.0&&(duration-position)<=skipSeconds){
        toggleWarpPlaybackMode();
        return;
    }
    mpvWidget->seekRelativeKeyframe(skipSeconds);
}
void MainWindow::moveSelectedFileToTarget(int idx){if(idx<0||idx>=6)return; loadMoveConfig(); QString target=moveButtonPaths.value(idx); if(target.isEmpty())return; int r=currentRow(); if(r<0||r>=playlistModel->count())return; QString src=playlistModel->pathAt(r); if(src.isEmpty()||!QFileInfo::exists(src)){playlistModel->removeRowAt(r); savePlaylistState(); return;} bool wasCurrent=(QFileInfo(mpvWidget->currentFilePath()).absoluteFilePath()==QFileInfo(src).absoluteFilePath()); if(wasCurrent)closeCurrentFile(); QString dest; if(moveFileToDirectory(src,target,dest)){appendMoveLog(src,dest,moveButtonNames.value(idx,QString("Move %1").arg(idx+1))); playlistModel->removeRowAt(r); if(playlistModel->count()>0){int next=qBound(0,r,playlistModel->count()-1); playlistView->setCurrentIndex(playlistModel->index(next,0)); playPlaylistRow(next);} else savePlaylistState();}}
void MainWindow::probeMissingMetadata(){for(auto&p:playlistModel->pathsNeedingProbe())metadataProbe->enqueue(p);} void MainWindow::generateMissingThumbnails(){for(auto&p:playlistModel->pathsNeedingThumbnail())thumbnailManager->enqueue(p);} void MainWindow::updateTimeLabel(double p,double d){if(timeLabel)timeLabel->setText(formatHMS(p)+" / "+formatHMS(d));} void MainWindow::openFiles(){addFiles(QFileDialog::getOpenFileNames(this,"Open media files",{},"Media files (*.mkv *.mp4 *.avi *.mov *.webm *.mp3 *.flac *.wav);;All files (*)"));}
void MainWindow::onPositionChanged(double s){position=s;timeline->setPosition(s);updateTimeLabel(position,duration);} void MainWindow::onDurationChanged(double s){duration=s;timeline->setDuration(s);playlistModel->setDurationForPath(mpvWidget->currentFilePath(),s);savePlaylistState();updateTimeLabel(position,duration);} 