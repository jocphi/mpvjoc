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
