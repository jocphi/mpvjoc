#include "MpvWidget.h"
#include "PlaybackOverlayWidget.h"
#include "util/Utils.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QMetaObject>
#include <QByteArray>
#include <cmath>
#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>
#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QPolygonF>
#include <QPen>
#include <QStringList>
#include <QtGlobal>
#include <QFont>
#include <QResizeEvent>
#include <cstdint>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>
#include <QStandardPaths>


class IdleLogoOverlay : public QWidget {
public:
    explicit IdleLogoOverlay(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        hide();
    }

    void setLogoPath(const QString& path)
    {
        if (logoPath == path && !logo.isNull())
            return;

        logoPath = path;
        logo = QPixmap(logoPath);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (logo.isNull())
            return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QSize target = logo.size().scaled(size(), Qt::KeepAspectRatio);
        const QRect targetRect((width() - target.width()) / 2,
            (height() - target.height()) / 2,
            target.width(),
            target.height());

        painter.drawPixmap(targetRect, logo);
    }

private:
    QString logoPath;
    QPixmap logo;
};



QString MpvWidget::overlayScaleText()const{
    if(videoDisplayWidth<=0||videoDisplayHeight<=0)return QString();
    int percent=qMax(1,int(effectiveVideoScale()*100.0+0.5));
    return QString::number(percent)+"%";
}
QString MpvWidget::overlayDimensionsText()const{
    if(videoDisplayWidth<=0||videoDisplayHeight<=0)return QString();
    double effective=effectiveVideoScale();
    int actualW=qMax(1,int(videoDisplayWidth*effective+0.5));
    int actualH=qMax(1,int(videoDisplayHeight*effective+0.5));
    return QString("%1×%2\n%3×%4").arg(videoDisplayWidth).arg(videoDisplayHeight).arg(actualW).arg(actualH);
}
QString MpvWidget::overlayVolumeText()const{
    return lastOverlayMuted?QStringLiteral("Muted"):QStringLiteral("Vol %1%").arg(int(lastOverlayVolume+0.5));
}

QSize MpvWidget::overlayRenderedVideoSize()const{
    if(videoDisplayWidth<=0||videoDisplayHeight<=0)return QSize();
    double effective=effectiveVideoScale();
    return QSize(qMax(1,int(videoDisplayWidth*effective+0.5)),qMax(1,int(videoDisplayHeight*effective+0.5)));
}
void MpvWidget::toggleInfoOverlay(){
    infoOverlayPinned=!infoOverlayPinned;
    if(infoOverlayPinned){
        showScaleOverlay();
    }else if(playbackOverlay){
        static_cast<PlaybackOverlayWidget*>(playbackOverlay)->hideOverlay();
    }
}
void MpvWidget::showScaleOverlay(){
    if(!playbackOverlay)playbackOverlay=new PlaybackOverlayWidget(this);playbackOverlay->setGeometry(rect());static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayCells(overlayCenterCell,overlayScaleDisplayCell,overlayVolumeCell,overlayVisibilityMapCell,overlayWarpLabelCell);static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayOpacities(overlayCenterOpacity,overlayScaleDisplayOpacity,overlayVolumeOpacity,overlayVisibilityMapOpacity,overlayWarpLabelOpacity);
    const int mask=infoOverlayPinned?overlayProfilePersistentInfo:overlayProfileScaleChange;
    const bool showIcon=(mask&OverlayPlayState)!=0;
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayElementMask(mask);
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->showOverlay(playbackPaused,overlayScaleText(),overlayDimensionsText(),showIcon,infoOverlayPinned,rect().size(),overlayRenderedVideoSize());
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setVolumeText(overlayVolumeText());
}


void MpvWidget::showVolumeOverlay(double volume, bool muted)
{
    lastOverlayVolume=volume;
    lastOverlayMuted=muted;
    if(!playbackOverlay)playbackOverlay=new PlaybackOverlayWidget(this);
    playbackOverlay->setGeometry(rect());
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayCells(
        overlayCenterCell, overlayScaleDisplayCell, overlayVolumeCell, overlayVisibilityMapCell, overlayWarpLabelCell);
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayOpacities(
        overlayCenterOpacity, overlayScaleDisplayOpacity, overlayVolumeOpacity, overlayVisibilityMapOpacity, overlayWarpLabelOpacity);
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayElementMask(overlayProfileVolumeChange);
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->showVolumeOverlay(
        playbackPaused, overlayVolumeText(), overlayScaleText(), overlayDimensionsText(), rect().size(), overlayRenderedVideoSize());
    if (infoOverlayPinned)
        QTimer::singleShot(750, this, [this] { if (infoOverlayPinned) showScaleOverlay(); });
}





void MpvWidget::setOverlayCells(int playStateCell, int scaleDisplayCell, int volumeOverlayCell, int visibilityMapCell, int warpLabelCell)
{
    overlayCenterCell = qBound(1, playStateCell, 9);
    overlayScaleDisplayCell = qBound(1, scaleDisplayCell, 9);
    overlayVolumeCell = qBound(1, volumeOverlayCell, 9);
    overlayVisibilityMapCell = qBound(1, visibilityMapCell, 9);
    overlayWarpLabelCell = qBound(1, warpLabelCell, 9);
    if (playbackOverlay)
        static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayCells(
            overlayCenterCell, overlayScaleDisplayCell, overlayVolumeCell, overlayVisibilityMapCell, overlayWarpLabelCell);
}




void MpvWidget::setOverlayOpacities(int playStateOpacity, int scaleDisplayOpacity, int volumeOverlayOpacity, int visibilityMapOpacity, int warpLabelOpacity)
{
    overlayCenterOpacity = qBound(0, playStateOpacity, 100);
    overlayScaleDisplayOpacity = qBound(0, scaleDisplayOpacity, 100);
    overlayVolumeOpacity = qBound(0, volumeOverlayOpacity, 100);
    overlayVisibilityMapOpacity = qBound(0, visibilityMapOpacity, 100);
    overlayWarpLabelOpacity = qBound(0, warpLabelOpacity, 100);
    if (playbackOverlay)
        static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayOpacities(
            overlayCenterOpacity, overlayScaleDisplayOpacity, overlayVolumeOpacity, overlayVisibilityMapOpacity, overlayWarpLabelOpacity);
}



void MpvWidget::setOverlayProfiles(int persistentInfo, int playStateChange, int volumeChange, int warpMode, int scaleChange)
{
    auto clampMask=[](int mask,int fallback){mask&=OverlayAll;return mask?mask:fallback;};
    overlayProfilePersistentInfo=clampMask(persistentInfo,OverlayAll);
    overlayProfilePlayStateChange=clampMask(playStateChange,OverlayPlayState|OverlayScaleDisplay);
    overlayProfileVolumeChange=clampMask(volumeChange,OverlayVolume);
    overlayProfileWarpMode=clampMask(warpMode,OverlayWarpLabel);
    overlayProfileScaleChange=clampMask(scaleChange,OverlayScaleDisplay);
}

void MpvWidget::showPlaybackOverlay(bool paused){
    playbackPaused=paused;
    if(!playbackOverlay)playbackOverlay=new PlaybackOverlayWidget(this);playbackOverlay->setGeometry(rect());static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayCells(overlayCenterCell,overlayScaleDisplayCell,overlayVolumeCell,overlayVisibilityMapCell,overlayWarpLabelCell);static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayOpacities(overlayCenterOpacity,overlayScaleDisplayOpacity,overlayVolumeOpacity,overlayVisibilityMapOpacity,overlayWarpLabelOpacity);
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayElementMask(overlayProfilePlayStateChange);
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->showOverlay(paused,overlayScaleText(),overlayDimensionsText(),(overlayProfilePlayStateChange&OverlayPlayState)!=0,false,rect().size(),overlayRenderedVideoSize());
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setVolumeText(overlayVolumeText());
    if(infoOverlayPinned)QTimer::singleShot(750,this,[this]{if(infoOverlayPinned)showScaleOverlay();});
}



void MpvWidget::setWarpOverlay(bool active,int factor){
    if(!playbackOverlay)playbackOverlay=new PlaybackOverlayWidget(this);playbackOverlay->setGeometry(rect());static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayCells(overlayCenterCell,overlayScaleDisplayCell,overlayVolumeCell,overlayVisibilityMapCell,overlayWarpLabelCell);static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayOpacities(overlayCenterOpacity,overlayScaleDisplayOpacity,overlayVolumeOpacity,overlayVisibilityMapOpacity,overlayWarpLabelOpacity);
    const int mask=active?overlayProfileWarpMode:overlayProfileScaleChange;
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayElementMask(mask);
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setWarpState(active,factor);
    if(active){
        static_cast<PlaybackOverlayWidget*>(playbackOverlay)->showOverlay(playbackPaused,overlayScaleText(),overlayDimensionsText(),(mask&OverlayPlayState)!=0,true,rect().size(),overlayRenderedVideoSize());
        static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setVolumeText(overlayVolumeText());
    }
}


void MpvWidget::showWarpFeedback(int factor){
    if(!playbackOverlay)playbackOverlay=new PlaybackOverlayWidget(this);playbackOverlay->setGeometry(rect());static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayCells(overlayCenterCell,overlayScaleDisplayCell,overlayVolumeCell,overlayVisibilityMapCell,overlayWarpLabelCell);static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayOpacities(overlayCenterOpacity,overlayScaleDisplayOpacity,overlayVolumeOpacity,overlayVisibilityMapOpacity,overlayWarpLabelOpacity);
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setOverlayElementMask(overlayProfileWarpMode);
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->showWarpFeedback(factor);
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setVolumeText(overlayVolumeText());
}



QString MpvWidget::resolvedIdleLogoPath() const
{
    if (!idleLogoPath.isEmpty() && QFileInfo::exists(idleLogoPath))
        return idleLogoPath;

    const QString envPath = qEnvironmentVariable("MPVJOC_IDLE_LOGO");
    if (!envPath.isEmpty() && QFileInfo::exists(envPath))
        return envPath;

    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QStringList suffixes { QStringLiteral("svg"), QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg") };
    if (!configDir.isEmpty()) {
        const QDir dir(configDir);
        for (const QString& suffix : suffixes) {
            const QString candidate = dir.filePath(QStringLiteral("idle-logo.%1").arg(suffix));
            if (QFileInfo::exists(candidate))
                return candidate;
        }
    }

    const QDir appDir(QCoreApplication::applicationDirPath());
    const QStringList candidates {
        appDir.filePath(QStringLiteral("../assets/mpvjoc.svg")),
        appDir.filePath(QStringLiteral("assets/mpvjoc.svg")),
        QDir::current().filePath(QStringLiteral("assets/mpvjoc.svg")),
    };

    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return QFileInfo(candidate).absoluteFilePath();
    }

    return QString();
}

void MpvWidget::ensureIdleLogoOverlay()
{
    if (!idleLogoOverlay)
        idleLogoOverlay = new IdleLogoOverlay(this);

    idleLogoOverlay->setGeometry(rect());
    static_cast<IdleLogoOverlay*>(idleLogoOverlay)->setLogoPath(resolvedIdleLogoPath());
}

void MpvWidget::updateIdleLogoOverlay()
{
    if (!cur.isEmpty()) {
        if (idleLogoOverlay)
            idleLogoOverlay->hide();
        return;
    }

    ensureIdleLogoOverlay();
    idleLogoOverlay->setGeometry(rect());
    idleLogoOverlay->show();
    idleLogoOverlay->raise();
    idleLogoOverlay->update();
}

void MpvWidget::setIdleLogoPath(const QString& path)
{
    idleLogoPath = path;
    if (idleLogoOverlay)
        static_cast<IdleLogoOverlay*>(idleLogoOverlay)->setLogoPath(resolvedIdleLogoPath());
    updateIdleLogoOverlay();
}

MpvWidget::MpvWidget(QWidget*p):QOpenGLWidget(p){forceCLocaleForMpv();setMinimumSize(640,360);setFocusPolicy(Qt::StrongFocus); mpv=mpv_create(); if(!mpv)qFatal("no mpv"); check_mpv_error(mpv_set_option_string(mpv,"terminal","yes")); check_mpv_error(mpv_set_option_string(mpv,"msg-level","all=warn,libmpv_render=fatal")); check_mpv_error(mpv_set_option_string(mpv,"hwdec","auto-safe")); check_mpv_error(mpv_set_option_string(mpv,"video-unscaled","yes")); check_mpv_error(mpv_set_option_string(mpv,"vo","libmpv")); check_mpv_error(mpv_set_option_string(mpv,"input-default-bindings","no")); check_mpv_error(mpv_set_option_string(mpv,"osc","no")); check_mpv_error(mpv_initialize(mpv)); mpv_observe_property(mpv,1,"time-pos",MPV_FORMAT_DOUBLE); mpv_observe_property(mpv,2,"duration",MPV_FORMAT_DOUBLE); mpv_observe_property(mpv,3,"pause",MPV_FORMAT_FLAG); mpv_observe_property(mpv,4,"path",MPV_FORMAT_STRING); mpv_observe_property(mpv,5,"volume",MPV_FORMAT_DOUBLE); mpv_observe_property(mpv,6,"mute",MPV_FORMAT_FLAG); mpv_observe_property(mpv,7,"dwidth",MPV_FORMAT_INT64); mpv_observe_property(mpv,8,"dheight",MPV_FORMAT_INT64); mpv_set_wakeup_callback(mpv,wakeup,this); updateIdleLogoOverlay();} 
MpvWidget::~MpvWidget(){makeCurrent(); if(ctx)mpv_render_context_free(ctx); doneCurrent(); if(mpv)mpv_terminate_destroy(mpv);} 
void MpvWidget::loadFile(const QString&p){if(idleLogoOverlay)idleLogoOverlay->hide();cur=p; QByteArray b=p.toUtf8(); const char*cmd[]={"loadfile",b.constData(),"replace",nullptr}; check_mpv_error(mpv_command_async(mpv,0,cmd));} void MpvWidget::stopPlayback(){cur.clear(); updateIdleLogoOverlay(); const char*cmd[]={"stop",nullptr}; check_mpv_error(mpv_command_async(mpv,0,cmd)); update();} QString MpvWidget::currentFilePath()const{return cur;} void MpvWidget::togglePause(){const char*cmd[]={"cycle","pause",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));} void MpvWidget::toggleMute(){const char*cmd[]={"cycle","mute",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));} void MpvWidget::changeVolume(double d){QByteArray v=QByteArray::number(d,'f',1); const char*cmd[]={"add","volume",v.constData(),nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));} void MpvWidget::setVolume(double v){QByteArray b=QByteArray::number(qBound(0.0,v,130.0),'f',1); const char*cmd[]={"set","volume",b.constData(),nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));} void MpvWidget::setMute(bool m){const char*cmd[]={"set","mute",m?"yes":"no",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));} void MpvWidget::seekAbsolute(double s){QByteArray b=QByteArray::number(s,'f',3); const char*cmd[]={"seek",b.constData(),"absolute","exact",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));} void MpvWidget::seekRelative(double s){QByteArray b=QByteArray::number(s,'f',3); const char*cmd[]={"seek",b.constData(),"relative","exact",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));}
double MpvWidget::effectiveVideoScale()const{
    if(clipVideoToScale)return maxVideoScale;
    if(videoDisplayWidth<=0||videoDisplayHeight<=0||width()<=0||height()<=0)return maxVideoScale;
    double fit=qMin(width()/double(videoDisplayWidth),height()/double(videoDisplayHeight));
    return qMin(maxVideoScale,fit);
}
void MpvWidget::applyVideoScale(){
    double effective=qBound(0.01,effectiveVideoScale(),8.0);
    double zoom=std::log2(effective);
    QByteArray z=QByteArray::number(zoom,'f',3);
    const char*cmd[]={"set","video-zoom",z.constData(),nullptr};
    check_mpv_error(mpv_command_async(mpv,0,cmd));
}
void MpvWidget::setClipVideoToScale(bool crop){
    clipVideoToScale=crop;
    applyVideoScale();
    if(infoOverlayPinned)showScaleOverlay();
}
void MpvWidget::setPause(bool paused){const char*cmd[]={"set","pause",paused?"yes":"no",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));}
void MpvWidget::frameStepForward(){const char*cmd[]={"frame-step",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));}
void MpvWidget::frameStepBackward(){const char*cmd[]={"frame-back-step",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));}
void MpvWidget::seekAbsoluteKeyframe(double s){QByteArray b=QByteArray::number(s,'f',3); const char*cmd[]={"seek",b.constData(),"absolute","keyframes",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));}
void MpvWidget::seekRelativeKeyframe(double s){QByteArray b=QByteArray::number(s,'f',3); const char*cmd[]={"seek",b.constData(),"relative","keyframes",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));}
void MpvWidget::setMaxVideoScale(double scale){ double clamped=qBound(0.5,scale,2.0); maxVideoScale=clamped; applyVideoScale();}
void MpvWidget::resizeEvent(QResizeEvent*e){QOpenGLWidget::resizeEvent(e); applyVideoScale(); if(playbackOverlay)playbackOverlay->setGeometry(rect()); if(idleLogoOverlay){idleLogoOverlay->setGeometry(rect());updateIdleLogoOverlay();}}
void MpvWidget::initializeGL(){mpv_opengl_init_params gl{getProc,nullptr}; mpv_render_param ps[]={{MPV_RENDER_PARAM_API_TYPE,const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},{MPV_RENDER_PARAM_OPENGL_INIT_PARAMS,&gl},{MPV_RENDER_PARAM_INVALID,nullptr}}; check_mpv_error(mpv_render_context_create(&ctx,mpv,ps)); mpv_render_context_set_update_callback(ctx,render,this);} 
void MpvWidget::paintGL(){if(!ctx)return; if(cur.isEmpty()){auto*f=QOpenGLContext::currentContext()->functions(); f->glClearColor(0.0f,0.0f,0.0f,1.0f); f->glClear(GL_COLOR_BUFFER_BIT); return;} qreal s=devicePixelRatioF(); mpv_opengl_fbo f{int(defaultFramebufferObject()),int(width()*s),int(height()*s),0}; int flip=1; mpv_render_param ps[]={{MPV_RENDER_PARAM_OPENGL_FBO,&f},{MPV_RENDER_PARAM_FLIP_Y,&flip},{MPV_RENDER_PARAM_INVALID,nullptr}}; mpv_render_context_render(ctx,ps);} 
void*MpvWidget::getProc(void*,const char*n){auto*c=QOpenGLContext::currentContext();return c?reinterpret_cast<void*>(c->getProcAddress(QByteArray(n))):nullptr;} void MpvWidget::render(void*o){auto*s=static_cast<MpvWidget*>(o);QMetaObject::invokeMethod(s,[s]{s->update();},Qt::QueuedConnection);} void MpvWidget::wakeup(void*o){auto*s=static_cast<MpvWidget*>(o);QMetaObject::invokeMethod(s,[s]{s->events();},Qt::QueuedConnection);} void MpvWidget::events(){while(mpv){auto*e=mpv_wait_event(mpv,0); if(!e||e->event_id==MPV_EVENT_NONE)break; if(e->event_id==MPV_EVENT_PROPERTY_CHANGE){prop(static_cast<mpv_event_property*>(e->data));} else if(e->event_id==MPV_EVENT_END_FILE){auto*ef=static_cast<mpv_event_end_file*>(e->data); if(!ef||ef->reason==MPV_END_FILE_REASON_EOF||ef->reason==MPV_END_FILE_REASON_STOP){if(onEndFile)onEndFile();}}}} void MpvWidget::prop(mpv_event_property*p){if(!p||!p->data)return; QString n=p->name; if(n=="time-pos"&&p->format==MPV_FORMAT_DOUBLE){if(onPosition)onPosition(*static_cast<double*>(p->data));} else if(n=="duration"&&p->format==MPV_FORMAT_DOUBLE){if(onDuration)onDuration(*static_cast<double*>(p->data));} else if(n=="pause"&&p->format==MPV_FORMAT_FLAG){if(onPause)onPause(*static_cast<int*>(p->data)!=0);} else if(n=="volume"&&p->format==MPV_FORMAT_DOUBLE){lastOverlayVolume=*static_cast<double*>(p->data);if(onVolume)onVolume(lastOverlayVolume);} else if(n=="mute"&&p->format==MPV_FORMAT_FLAG){lastOverlayMuted=*static_cast<int*>(p->data)!=0;if(onMute)onMute(lastOverlayMuted);} else if(n=="dwidth"&&p->format==MPV_FORMAT_INT64){videoDisplayWidth=*static_cast<int64_t*>(p->data);applyVideoScale();} else if(n=="dheight"&&p->format==MPV_FORMAT_INT64){videoDisplayHeight=*static_cast<int64_t*>(p->data);applyVideoScale();} else if(n=="path"&&p->format==MPV_FORMAT_STRING){char*v=*static_cast<char**>(p->data);cur=QString::fromUtf8(v?v:""); updateIdleLogoOverlay(); if(onFile)onFile(cur);}}
