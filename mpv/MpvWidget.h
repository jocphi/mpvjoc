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
    void setCropVideoToScale(bool crop);
    void showPlaybackOverlay(bool paused);
    void showScaleOverlay();
    void showVolumeOverlay(double volume,bool muted);
    void toggleInfoOverlay();
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
    bool infoOverlayPinned=false;
    bool playbackPaused=false;
    qint64 videoDisplayWidth=0;
    qint64 videoDisplayHeight=0;
    double maxVideoScale=1.0;
    bool cropVideoToScale=true;
    double effectiveVideoScale()const;
    void applyVideoScale();
    QString overlayScaleText()const;
    QString overlayDimensionsText()const;
    QSize overlayRenderedVideoSize()const;
};
