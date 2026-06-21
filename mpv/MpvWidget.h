#pragma once
#include <QOpenGLWidget>
#include <functional>
struct mpv_handle;
struct mpv_render_context;
struct mpv_event_property;

class QWidget;
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
    void toggleMute();
    void changeVolume(double d);
    void setVolume(double v);
    void setMute(bool m);
    void seekAbsolute(double s);
    void seekRelative(double s);
    void setMaxVideoScale(double scale);
    void showPlaybackOverlay(bool paused);
protected:
    void initializeGL()override;
    void paintGL()override;
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
};
