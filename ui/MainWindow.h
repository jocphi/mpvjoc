#pragma once
#include <QMainWindow>
#include <QVector>
#include <QStringList>
class MpvWidget; class PlaylistModel; class PlaylistFilterProxyModel; class MetadataProbeManager; class ThumbnailManager; class QLineEdit; class QListView; class QSplitter; class TimelineWidget; class QLabel; class QPushButton; class QTabWidget; class QTextEdit; class QCloseEvent; class QDragEnterEvent; class QDropEvent; class QKeyEvent; class QTimer;

class MainWindow: public QMainWindow{
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow();
    void addFiles(const QStringList&files);
protected:
    bool eventFilter(QObject*o,QEvent*ev)override;
    void closeEvent(QCloseEvent*e)override;
    void dragEnterEvent(QDragEnterEvent*e)override;
    void dropEvent(QDropEvent*e)override;
    void keyPressEvent(QKeyEvent*e)override;
private:
    void openSettingsDialog();
    void loadTimelineColorSettings();
    void saveTimelineColorSettings()const;
    void applyTimelineHueTheme();
    void loadOverlayCellSettings();
    void saveOverlayCellSettings();
    void applyOverlayCellSettings();
    void loadOverlayProfileSettings();
    void saveOverlayProfileSettings();
    void applyOverlayProfileSettings();
    QString stateFilePath()const;
    void updateMuteVolumeButton();
    void updatePlaylistSummary();
    void playPauseOrStartSelected();
    void closeCurrentFile();
    void moveSelectedFileToTrash();
    void playNextPlaylistFile();
    void playPreviousPlaylistFile();
    QString moveConfigFilePath()const;
    QString moveLogFilePath()const;
    void ensureMoveLists();
    void createDefaultMoveConfig();
    void loadMoveConfig();
    void saveMoveConfig()const;
    void refreshMoveLogView();
    void updateMoveButtons();
    QString uniqueMoveDestination(const QString&dir,const QString&fileName)const;
    bool moveFileToDirectory(const QString&src,const QString&targetDir,QString&destOut);
    void appendMoveLog(const QString&src,const QString&dest,const QString&buttonName);
    double normalizedMaxVideoScale(double scale)const;
    void setMaxVideoScale(double scale);
    void updateScaleButtons();
    void setClipVideoToScale(bool crop);
    void updateClipButton();
    void toggleShortcutHelpOverlay();
    QString shortcutHelpText()const;
    void setAutoPlayNextEnabled(bool enabled);
    void updateAutoPlayButton();
    void updateWarpOverlay();
    void setWarpFactor(int factor);
    void toggleWarpPlaybackMode();
    void fastPlaybackTick();
    void moveSelectedFileToTarget(int idx);
    int currentRow()const;
    QString selectedPath()const;
    void savePlaylistState();
    void loadPlaylistState();
    void probeMissingMetadata();
    void generateMissingThumbnails();
    void refreshMediaScans();
    void removeMissingFilesFromPlaylist();
    void replacePlaylistWithFiles(const QStringList&files);
    void addDroppedPaths(const QStringList&paths);
    void replacePlaylistWithDroppedPaths(const QStringList&paths);
    void playPlaylistRow(int r);
    void selectPath(const QString&p);
    QModelIndex playlistViewIndexForRow(int sourceRow)const;
    void setPlaylistCurrentSourceRow(int sourceRow,bool scroll=false);
    void ensureVisiblePlaylistSelection();
    void updateTimeLabel(double p,double d);
    void openFiles();
    void onPositionChanged(double s);
    void onDurationChanged(double s);
    void removeSelectedItem();
    void clearPlaylist();
    void moveSelectedItemUp();
    void moveSelectedItemDown();
    void showPlaylistContextMenu(const QPoint&p);
    void toggleReviewedForCurrent();
    void setPlaylistKeyboardFocus(bool focus);
    void toggleKeyboardFocusTarget();
    MpvWidget*mpvWidget=nullptr; PlaylistModel*playlistModel=nullptr; MetadataProbeManager*metadataProbe=nullptr; ThumbnailManager*thumbnailManager=nullptr; QLineEdit*playlistSearchEdit=nullptr; QListView*playlistView=nullptr; PlaylistFilterProxyModel*playlistProxyModel=nullptr; QSplitter*playlistSplitter=nullptr; TimelineWidget*timeline=nullptr; QLabel*timeLabel=nullptr; QLabel*titleStatusLabel=nullptr; QLabel*playlistSummaryLabel=nullptr; QLabel*shortcutHelpOverlay=nullptr; QPushButton*muteButton=nullptr; QPushButton*scaleStatusButton=nullptr; QPushButton*clipButton=nullptr; QPushButton*autoPlayButton=nullptr; QTabWidget*rightTabs=nullptr; QTextEdit*moveLogView=nullptr; QTimer*fastPlaybackTimer=nullptr; QVector<QPushButton*> moveButtons; QStringList moveButtonNames={"Move 1","Move 2","Move 3","Move 4","Move 5","Move 6"}; QStringList moveButtonPaths={"","","","","",""}; int moveButtonCount=6; double position=0,duration=0,currentVolume=100,maxVideoScale=1.0; int warpFactor=1; int overlayCenterCell=5; int overlayScaleDisplayCell=8; int overlayVolumeCell=6; int overlayVisibilityMapCell=9; int overlayWarpLabelCell=3; int overlayCenterOpacity=100; int overlayScaleDisplayOpacity=100; int overlayVolumeOpacity=100; int overlayVisibilityMapOpacity=100; int overlayWarpLabelOpacity=100; static constexpr int OverlayPlayState=1<<0; static constexpr int OverlayScaleDisplay=1<<1; static constexpr int OverlayVolume=1<<2; static constexpr int OverlayVisibilityMap=1<<3; static constexpr int OverlayWarpLabel=1<<4; static constexpr int OverlayAll=OverlayPlayState|OverlayScaleDisplay|OverlayVolume|OverlayVisibilityMap|OverlayWarpLabel; int overlayProfilePersistentInfo=OverlayAll; int overlayProfilePlayStateChange=OverlayPlayState|OverlayScaleDisplay; int overlayProfileVolumeChange=OverlayVolume; int overlayProfileWarpMode=OverlayWarpLabel; int overlayProfileScaleChange=OverlayScaleDisplay; bool currentMuted=false; bool clipVideoToScale=true; bool restoringPlaybackState=true; bool suppressNextEndFileAdvance=false; bool warpPlaybackMode=false; bool autoPlayNextEnabled=true; bool playlistKeyboardFocus=false;
    int timelineGreenDarkPercent=80;
    int timelineGreyDarkPercent=80;
    int timelineRedDarkPercent=80;
};
