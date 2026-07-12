#pragma once
#include <QOpenGLWidget>
#include <functional>
#include <QtGlobal>
#include <QSize>
struct mpv_handle;
struct mpv_render_context;
struct mpv_event_property;

class QWidget;
class QResizeEvent;
class MpvWidget: public QOpenGLWidget{
public:
    explicit MpvWidget(QWidget*p=nullptr);
    ~MpvWidget();
    std::function<void(double)> onPosition,onDuration,onVolume;
    std::function<void(bool)> onPause,onMute;
    std::function<void(QString)> onFile;
    std::function<void()> onEndFile;
    void loadFile(const QString&p);
    void stopPlayback();
    void setIdleLogoPath(const QString& path);
    QString currentFilePath()const;
    void togglePause();
    void setPause(bool paused);
    void toggleMute();
    void changeVolume(double d);
    void setVolume(double v);
    void setMute(bool m);
    void seekAbsolute(double s);
    void seekRelative(double s);
    void frameStepForward();
    void frameStepBackward();
    void seekAbsoluteKeyframe(double s);
    void seekRelativeKeyframe(double s);
    void setMaxVideoScale(double scale);
    void setClipVideoToScale(bool crop);
    QString currentScalePercentText()const;
    void showPlaybackOverlay(bool paused);
    void showScaleOverlay();
    void showVolumeOverlay(double volume,bool muted);
    void setOverlayCells(int playStateCell, int scaleDisplayCell, int volumeOverlayCell, int visibilityMapCell, int warpLabelCell);
    void setOverlayOpacities(int playStateOpacity, int scaleDisplayOpacity, int volumeOverlayOpacity, int visibilityMapOpacity, int warpLabelOpacity);
    void setOverlayProfiles(int persistentInfo, int playStateChange, int volumeChange, int warpMode, int scaleChange);
    void toggleInfoOverlay();
    void setWarpOverlay(bool active,int factor);
    void showWarpFeedback(int factor);
protected:
    void initializeGL()override;
    void paintGL()override;
    void resizeEvent(QResizeEvent*e)override;
private:
    static void*getProc(void*,const char*n);
    static void render(void*o);
    static void wakeup(void*o);
    void events();
    void prop(mpv_event_property*p);
    mpv_handle*mpv=nullptr;
    mpv_render_context*ctx=nullptr;
    QString cur;
    QWidget* playbackOverlay=nullptr;
    QWidget* idleLogoOverlay=nullptr;
    QString idleLogoPath;
    int overlayCenterCell=5; int overlayScaleDisplayCell=8; int overlayVolumeCell=6; int overlayVisibilityMapCell=9; int overlayWarpLabelCell=3;
    int overlayCenterOpacity=100; int overlayScaleDisplayOpacity=100; int overlayVolumeOpacity=100; int overlayVisibilityMapOpacity=100; int overlayWarpLabelOpacity=100; static constexpr int OverlayPlayState=1<<0; static constexpr int OverlayScaleDisplay=1<<1; static constexpr int OverlayVolume=1<<2; static constexpr int OverlayVisibilityMap=1<<3; static constexpr int OverlayWarpLabel=1<<4; static constexpr int OverlayAll=OverlayPlayState|OverlayScaleDisplay|OverlayVolume|OverlayVisibilityMap|OverlayWarpLabel; int overlayProfilePersistentInfo=OverlayAll; int overlayProfilePlayStateChange=OverlayPlayState|OverlayScaleDisplay; int overlayProfileVolumeChange=OverlayVolume; int overlayProfileWarpMode=OverlayWarpLabel; int overlayProfileScaleChange=OverlayScaleDisplay;
    bool infoOverlayPinned=false;
    bool playbackPaused=false;
    double lastOverlayVolume=100.0;
    bool lastOverlayMuted=false;
    qint64 videoDisplayWidth=0;
    qint64 videoDisplayHeight=0;
    double maxVideoScale=1.0;
    bool clipVideoToScale=true;
    double effectiveVideoScale()const;
    void applyVideoScale();
    QString resolvedIdleLogoPath()const;
    void ensureIdleLogoOverlay();
    void updateIdleLogoOverlay();
    QString overlayScaleText()const;
    QString overlayDimensionsText()const;
    QString overlayVolumeText()const;
    QSize overlayRenderedVideoSize()const;
};
