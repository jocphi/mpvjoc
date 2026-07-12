#include "MainWindow.h"
#include "PlaylistWorkspace.h"
#include "TimelineWidget.h"
#include "media/MetadataProbeManager.h"
#include "media/ThumbnailManager.h"
#include "mpv/MpvWidget.h"
#include "playlist/PlaylistDelegate.h"
#include "playlist/PlaylistModel.h"
#include "playlist/PlaylistFilterProxyModel.h"
#include "util/TextHighlighting.h"
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QModelIndex>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QShortcut>
#include <QSizePolicy>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include "Version.h"

namespace {
class HighlightedStatusLabel : public QLabel {
public:
    explicit HighlightedStatusLabel(const QString& text, QWidget* parent = nullptr)
        : QLabel(text, parent)
    {
        setContentsMargins(7, 2, 7, 2);
        setMinimumHeight(22);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(24, 24, 24));
        painter.setPen(QColor(85, 85, 85));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
        drawHighlightedTitle(&painter, contentsRect(), text(), QColor(Qt::white));
    }
};
}

MainWindow::MainWindow(){ setWindowFlag(Qt::FramelessWindowHint, true); mpvWidget=new MpvWidget(this); metadataProbe=new MetadataProbeManager(this); thumbnailManager=new ThumbnailManager(this); rightTabs=new QTabWidget(this); rightTabs->setTabsClosable(true); rightTabs->setMovable(true); auto*initialPlaylist=createPlaylistWorkspace(QStringLiteral("Playlist 1")); Q_UNUSED(initialPlaylist);
 timeline=new TimelineWidget(this); timeline->installEventFilter(this);
 timeLabel=new QLabel("00:00:00 / 00:00:00",this);
 titleStatusLabel=new HighlightedStatusLabel(QStringLiteral("No file loaded"),this);
 muteButton=new QPushButton("Volume: 100%",this);
 autoPlayButton=new QPushButton("autoplay",this);
 scaleStatusButton=new QPushButton("1x · --% actual",this);
 clipButton=new QPushButton("clipped",this);
 auto statusLabelStyle=QStringLiteral("QLabel{background:#181818;color:#eeeeee;border:1px solid #555;padding:2px 7px;}");
 timeLabel->setStyleSheet(statusLabelStyle);

 timeLabel->setAlignment(Qt::AlignCenter);
 titleStatusLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
 timeLabel->setMinimumWidth(145);
 titleStatusLabel->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
 titleStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
 for(auto*b:{muteButton,autoPlayButton,scaleStatusButton,clipButton}){b->setFocusPolicy(Qt::NoFocus);b->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);}
 auto*statusBar=new QHBoxLayout;
 statusBar->setContentsMargins(1,1,1,1);
 statusBar->setSpacing(1);
 statusBar->addWidget(timeLabel);
 statusBar->addWidget(titleStatusLabel,1);
 statusBar->addWidget(muteButton);
 statusBar->addWidget(autoPlayButton);
 statusBar->addWidget(scaleStatusButton);
 statusBar->addWidget(clipButton);
 auto*leftL=new QVBoxLayout; leftL->setContentsMargins(0,0,0,0); leftL->setSpacing(1); leftL->addWidget(mpvWidget,1); leftL->addWidget(timeline); leftL->addLayout(statusBar); auto*leftW=new QWidget(this); leftW->setLayout(leftL); auto*addPlaylistButton=new QPushButton(QStringLiteral("+"),rightTabs); addPlaylistButton->setFixedSize(28,24); addPlaylistButton->setToolTip(QStringLiteral("Add playlist")); rightTabs->setCornerWidget(addPlaylistButton,Qt::TopRightCorner); connect(addPlaylistButton,&QPushButton::clicked,this,&MainWindow::addPlaylistWorkspace); connect(rightTabs,&QTabWidget::currentChanged,this,&MainWindow::activatePlaylistWorkspace); connect(rightTabs,&QTabWidget::tabCloseRequested,this,&MainWindow::closePlaylistWorkspace); auto*rightL=new QVBoxLayout; rightL->setContentsMargins(1,1,1,1); rightL->setSpacing(1); rightL->addWidget(rightTabs,1); auto*rightW=new QWidget(this); rightW->setLayout(rightL); playlistSplitter=new QSplitter(this); playlistSplitter->setHandleWidth(1); playlistSplitter->addWidget(leftW); playlistSplitter->addWidget(rightW); playlistSplitter->setStretchFactor(0,5); playlistSplitter->setStretchFactor(1,1); setCentralWidget(playlistSplitter); resize(1280,720); setWindowTitle(QStringLiteral("mpvjoc %1").arg(QString::fromLatin1(MPVJOC_VERSION))); setAcceptDrops(true); mpvWidget->setAcceptDrops(true); mpvWidget->installEventFilter(this);
 auto*quitShortcut=new QShortcut(QKeySequence(Qt::Key_Q),this); quitShortcut->setContext(Qt::WindowShortcut); connect(quitShortcut,&QShortcut::activated,this,&MainWindow::close);
 connect(muteButton,&QPushButton::clicked,this,[this]{mpvWidget->toggleMute();});
 connect(autoPlayButton,&QPushButton::clicked,this,[this]{setAutoPlayNextEnabled(!autoPlayNextEnabled);});
 connect(scaleStatusButton,&QPushButton::clicked,this,[this]{setMaxVideoScale(maxVideoScale==0.5?1.0:(maxVideoScale==1.0?2.0:0.5));});
 connect(clipButton,&QPushButton::clicked,this,[this]{setClipVideoToScale(!clipVideoToScale);});
 connect(playlistSplitter,&QSplitter::splitterMoved,this,[this]{savePlaylistState();});
 fastPlaybackTimer=new QTimer(this);
 fastPlaybackTimer->setInterval(1000);
 connect(fastPlaybackTimer,&QTimer::timeout,this,&MainWindow::fastPlaybackTick);
 metadataProbe->metadataReady=[this](const QString&p,const MediaMetadata&m){for(auto*w:playlistWorkspaces)w->model->setMetadataForPath(p,m);if(!restoringPlaybackState)savePlaylistState();};
 metadataProbe->metadataFailed=[this](const QString&p){for(auto*w:playlistWorkspaces)w->model->markProbeFailed(p);if(!restoringPlaybackState)savePlaylistState();};
 thumbnailManager->thumbnailReady=[this](const QString&p,const QString&t){for(auto*w:playlistWorkspaces)w->model->setThumbnailForPath(p,t);if(!restoringPlaybackState)savePlaylistState();};
 thumbnailManager->thumbnailFailed=[this](const QString&p){for(auto*w:playlistWorkspaces)w->model->markThumbnailFailed(p);if(!restoringPlaybackState)savePlaylistState();};
 mpvWidget->onPosition=[this](double s){onPositionChanged(s);}; mpvWidget->onDuration=[this](double s){onDurationChanged(s);}; mpvWidget->onPause=[this](bool p){mpvWidget->showPlaybackOverlay(p);}; mpvWidget->onFile=[this](QString p){const QString name=QFileInfo(p).fileName();timeline->setTitle(name);if(titleStatusLabel)titleStatusLabel->setText(name.isEmpty()?QStringLiteral("No file loaded"):name);selectPath(p);updateScaleButtons();QTimer::singleShot(300,this,[this]{if(mpvWidget)mpvWidget->showScaleOverlay();updateScaleButtons();});}; mpvWidget->onEndFile=[this]{if(suppressNextEndFileAdvance){suppressNextEndFileAdvance=false;return;}if(autoPlayNextEnabled&&playlistModel->count()>1)playNextPlaylistFile();}; mpvWidget->onVolume=[this](double v){if(restoringPlaybackState)return;currentVolume=v;updateMuteVolumeButton();if(mpvWidget)mpvWidget->showVolumeOverlay(currentVolume,currentMuted);savePlaylistState();}; mpvWidget->onMute=[this](bool m){if(restoringPlaybackState)return;currentMuted=m;updateMuteVolumeButton();if(mpvWidget)mpvWidget->showVolumeOverlay(currentVolume,currentMuted);savePlaylistState();}; timeline->seekRequested=[this](double s){mpvWidget->seekAbsolute(s);}; timeline->previewPositionChanged=[this](double s){updateTimeLabel(s,duration);}; updateScaleButtons(); updateClipButton(); updateAutoPlayButton(); mpvWidget->setMaxVideoScale(maxVideoScale); mpvWidget->setClipVideoToScale(clipVideoToScale); updateMuteVolumeButton(); updateMoveButtons(); loadPlaylistState(); loadMoveConfig(); updatePlaylistSummary(); QTimer::singleShot(900,this,[this]{restoringPlaybackState=false;}); probeMissingMetadata(); generateMissingThumbnails(); }

