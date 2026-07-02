#pragma once
#include <QMainWindow>
#include <QVector>
#include <QStringList>
class MpvWidget; class PlaylistModel; class MetadataProbeManager; class ThumbnailManager; class QListView; class QSplitter; class TimelineWidget; class QLabel; class QPushButton; class QTabWidget; class QTextEdit; class QCloseEvent; class QDragEnterEvent; class QDropEvent; class QKeyEvent; class QTimer;

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
    void removeMissingFilesFromPlaylist();
    void replacePlaylistWithFiles(const QStringList&files);
    void addDroppedPaths(const QStringList&paths);
    void replacePlaylistWithDroppedPaths(const QStringList&paths);
    void playPlaylistRow(int r);
    void selectPath(const QString&p);
    void updateTimeLabel(double p,double d);
    void openFiles();
    void onPositionChanged(double s);
    void onDurationChanged(double s);
    void removeSelectedItem();
    void clearPlaylist();
    void moveSelectedItemUp();
    void moveSelectedItemDown();
    void showPlaylistContextMenu(const QPoint&p);
    MpvWidget*mpvWidget=nullptr; PlaylistModel*playlistModel=nullptr; MetadataProbeManager*metadataProbe=nullptr; ThumbnailManager*thumbnailManager=nullptr; QListView*playlistView=nullptr; QSplitter*playlistSplitter=nullptr; TimelineWidget*timeline=nullptr; QLabel*timeLabel=nullptr; QLabel*playlistSummaryLabel=nullptr; QLabel*shortcutHelpOverlay=nullptr; QPushButton*playPauseButton=nullptr; QPushButton*muteButton=nullptr; QPushButton*scaleHalfButton=nullptr; QPushButton*scaleOneButton=nullptr; QPushButton*scaleTwoButton=nullptr; QPushButton*clipButton=nullptr; QPushButton*autoPlayButton=nullptr; QTabWidget*rightTabs=nullptr; QTextEdit*moveLogView=nullptr; QTimer*fastPlaybackTimer=nullptr; QVector<QPushButton*> moveButtons; QStringList moveButtonNames={"Move 1","Move 2","Move 3","Move 4","Move 5","Move 6"}; QStringList moveButtonPaths={"","","","","",""}; int moveButtonCount=6; double position=0,duration=0,currentVolume=100,maxVideoScale=1.0; int warpFactor=1; bool currentMuted=false; bool clipVideoToScale=true; bool restoringPlaybackState=true; bool suppressNextEndFileAdvance=false; bool warpPlaybackMode=false; bool autoPlayNextEnabled=true;
};
