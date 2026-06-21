#include "MpvWidget.h"
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


namespace {
class PlaybackOverlayWidget : public QWidget {
public:
    explicit PlaybackOverlayWidget(QWidget* parent=nullptr):QWidget(parent){
        setAttribute(Qt::WA_TransparentForMouseEvents,true);
        setAttribute(Qt::WA_TranslucentBackground,true);
        hide();
    }
    void showOverlay(bool paused,const QString& scale,const QString& details=QString(),bool showIcon=true,bool persistent=false,const QSize& viewport=QSize(),const QSize& rendered=QSize()){
        pauseIcon=paused;
        scaleText=scale;
        detailsText=details;
        iconVisible=showIcon;
        viewportSize=viewport;
        renderedSize=rendered;
        if(warpActive){scaleText.clear();detailsText.clear();iconVisible=false;viewportSize=QSize();renderedSize=QSize();}
        int g=++generation;
        show();
        raise();
        update();
        if(persistent)return;
        QTimer::singleShot(700,this,[this,g]{ if(g==generation){if(warpActive){scaleText.clear();detailsText.clear();iconVisible=false;update();}else hide();} });
    }
    void hideOverlay(){hide();}
    void setWarpState(bool active,int factor){
        warpActive=active;
        warpFactor=qBound(1,factor,9);
        if(warpActive){show();raise();}else if(scaleText.isEmpty()&&detailsText.isEmpty())hide();
        update();
    }
protected:
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing,true);
        if(warpActive){
            p.setPen(QPen(QColor(255,0,0,235),3));
            p.setBrush(Qt::NoBrush);
            p.drawRect(rect().adjusted(2,2,-3,-3));
            QFont wf=p.font();
            wf.setBold(true);
            wf.setPointSizeF(qBound(12.0,qMin(width(),height())*0.035,28.0));
            p.setFont(wf);
            p.setPen(QColor(255,80,80,245));
            QString warpText=QString("Warp %1").arg(warpFactor);
            p.drawText(QRectF(0,8,width()-12,40),Qt::AlignRight|Qt::AlignVCenter,warpText);
        }
        int icon=qBound(64,qMin(width(),height())/5,150);
        QPointF c(width()/2.0,height()/2.0-icon*0.10);
        const bool centerOverlayVisible=iconVisible||!scaleText.isEmpty()||!detailsText.isEmpty();
        qreal bg=icon*1.35;
        QRectF bgRect(c.x()-bg/2.0,c.y()-bg/2.0,bg,bg);
        if(centerOverlayVisible){
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0,0,0,145));
            p.drawRoundedRect(bgRect,bg*0.18,bg*0.18);
            p.setBrush(QColor(255,255,255,235));
        }
        if(iconVisible&&pauseIcon){
            qreal bw=icon*0.22,bh=icon*0.72,gap=icon*0.18;
            qreal top=c.y()-bh/2.0,lx=c.x()-gap/2.0-bw,rx=c.x()+gap/2.0;
            p.drawRoundedRect(QRectF(lx,top,bw,bh),bw*0.28,bw*0.28);
            p.drawRoundedRect(QRectF(rx,top,bw,bh),bw*0.28,bw*0.28);
        }else if(iconVisible){
            QPolygonF tri;
            tri<<QPointF(c.x()-icon*0.24,c.y()-icon*0.36)<<QPointF(c.x()-icon*0.24,c.y()+icon*0.36)<<QPointF(c.x()+icon*0.38,c.y());
            p.drawPolygon(tri);
        }

        if(!scaleText.isEmpty()){
            QFont font=p.font();
            font.setBold(true);
            font.setPointSizeF(qBound(12.0,icon*0.18,28.0));
            p.setFont(font);
            p.setPen(QColor(255,255,255,235));
            qreal textY=iconVisible?(height()/2.0+icon*0.30):(height()/2.0-icon*0.16);
            QRectF textRect(0,textY,width(),icon*0.32);
            p.drawText(textRect,Qt::AlignHCenter|Qt::AlignVCenter,scaleText);

            if(!detailsText.isEmpty()){
                QFont small=font;
                small.setBold(false);
                small.setPointSizeF(qBound(9.0,icon*0.11,18.0));
                p.setFont(small);
                p.setPen(QColor(255,255,255,235));

                QStringList dimensionLines=detailsText.split(QStringLiteral("\n"));
                QString sourceText=dimensionLines.value(0);
                QString actualText=dimensionLines.value(1);
                if(actualText.isEmpty())actualText=detailsText;

                const qreal margin=qMax(12.0,icon*0.12);
                const qreal maxMapW=icon*1.55;
                const qreal maxMapH=icon*0.72;
                QFontMetricsF fm(small);
                const qreal textHeight=fm.height();

                qreal mapW=maxMapW;
                qreal mapH=maxMapH;
                QRectF viewportRect;
                QRectF videoRect;
                QRectF visibleRect;
                QRectF unionRect;
                bool hasMap=false;

                if(viewportSize.isValid()&&renderedSize.isValid()){
                    const qreal vw=viewportSize.width();
                    const qreal vh=viewportSize.height();
                    const qreal rw=renderedSize.width();
                    const qreal rh=renderedSize.height();
                    viewportRect=QRectF(0,0,vw,vh);
                    videoRect=QRectF((vw-rw)/2.0,(vh-rh)/2.0,rw,rh);
                    visibleRect=viewportRect.intersected(videoRect);
                    unionRect=viewportRect.united(videoRect);
                    const qreal mapScale=qMin(maxMapW/unionRect.width(),maxMapH/unionRect.height());
                    mapW=unionRect.width()*mapScale;
                    mapH=unionRect.height()*mapScale;
                    hasMap=true;
                }

                const qreal textW=qMax(fm.horizontalAdvance(sourceText),fm.horizontalAdvance(actualText));
                const qreal groupW=qMax(mapW,textW)+8.0;
                const qreal groupRight=width()-margin;
                const qreal groupBottom=height()-margin;
                const qreal sourceY=groupBottom-textHeight-3.0-mapH-3.0-textHeight;
                const QRectF sourceRect(groupRight-groupW,sourceY,groupW,textHeight);
                const QRectF actualRect(groupRight-groupW,sourceY+textHeight+3.0+mapH+3.0,groupW,textHeight);
                const QPointF mapCenter(groupRight-groupW/2.0,sourceY+textHeight+3.0+mapH/2.0);

                p.drawText(sourceRect,Qt::AlignHCenter|Qt::AlignVCenter,sourceText);

                if(hasMap){
                    const qreal mapScale=mapW/unionRect.width();
                    auto mapRect=[&](const QRectF&r){
                        return QRectF(mapCenter.x()+(r.left()-unionRect.center().x())*mapScale,
                                      mapCenter.y()+(r.top()-unionRect.center().y())*mapScale,
                                      r.width()*mapScale,
                                      r.height()*mapScale);
                    };
                    QRectF redRect=mapRect(videoRect);
                    QRectF greenRect=mapRect(visibleRect);
                    QRectF whiteRect=mapRect(viewportRect);
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor(255,0,0,220));
                    p.drawRect(redRect);
                    p.setBrush(QColor(40,255,0,235));
                    p.drawRect(greenRect);
                    p.setBrush(Qt::NoBrush);
                    p.setPen(QPen(QColor(255,255,255,245),qMax(1.0,icon*0.015)));
                    p.drawRect(whiteRect);
                    p.setPen(QColor(255,255,255,235));
                }

                p.drawText(actualRect,Qt::AlignHCenter|Qt::AlignVCenter,actualText);
            }
}
    }
private:
    bool pauseIcon=false;
    bool iconVisible=true;
    QString scaleText;
    QString detailsText;
    QSize viewportSize;
    QSize renderedSize;
    bool warpActive=false;
    int warpFactor=1;
    int generation=0;
};
}

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
    if(!playbackOverlay)playbackOverlay=new PlaybackOverlayWidget(this);
    playbackOverlay->setGeometry(rect());
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->showOverlay(infoOverlayPinned?playbackPaused:false,overlayScaleText(),overlayDimensionsText(),infoOverlayPinned,infoOverlayPinned,rect().size(),overlayRenderedVideoSize());
}
void MpvWidget::showVolumeOverlay(double volume,bool muted){
    if(!playbackOverlay)playbackOverlay=new PlaybackOverlayWidget(this);
    playbackOverlay->setGeometry(rect());
    QString label=muted?QStringLiteral("Muted"):QString("Vol %1%").arg(int(volume+0.5));
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->showOverlay(false,label,overlayDimensionsText(),false,false,rect().size(),overlayRenderedVideoSize());
    if(infoOverlayPinned)QTimer::singleShot(750,this,[this]{if(infoOverlayPinned)showScaleOverlay();});
}
void MpvWidget::showPlaybackOverlay(bool paused){
    playbackPaused=paused;
    if(!playbackOverlay)playbackOverlay=new PlaybackOverlayWidget(this);
    playbackOverlay->setGeometry(rect());
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->showOverlay(paused,overlayScaleText(),overlayDimensionsText(),true,false,rect().size(),overlayRenderedVideoSize());
    if(infoOverlayPinned)QTimer::singleShot(750,this,[this]{if(infoOverlayPinned)showScaleOverlay();});
}

void MpvWidget::setWarpOverlay(bool active,int factor){
    if(!playbackOverlay)playbackOverlay=new PlaybackOverlayWidget(this);
    playbackOverlay->setGeometry(rect());
    static_cast<PlaybackOverlayWidget*>(playbackOverlay)->setWarpState(active,factor);
}
MpvWidget::MpvWidget(QWidget*p):QOpenGLWidget(p){forceCLocaleForMpv();setMinimumSize(640,360);setFocusPolicy(Qt::StrongFocus); mpv=mpv_create(); if(!mpv)qFatal("no mpv"); check_mpv_error(mpv_set_option_string(mpv,"terminal","yes")); check_mpv_error(mpv_set_option_string(mpv,"msg-level","all=warn,libmpv_render=fatal")); check_mpv_error(mpv_set_option_string(mpv,"hwdec","auto-safe")); check_mpv_error(mpv_set_option_string(mpv,"video-unscaled","yes")); check_mpv_error(mpv_set_option_string(mpv,"vo","libmpv")); check_mpv_error(mpv_set_option_string(mpv,"input-default-bindings","no")); check_mpv_error(mpv_set_option_string(mpv,"osc","no")); check_mpv_error(mpv_initialize(mpv)); mpv_observe_property(mpv,1,"time-pos",MPV_FORMAT_DOUBLE); mpv_observe_property(mpv,2,"duration",MPV_FORMAT_DOUBLE); mpv_observe_property(mpv,3,"pause",MPV_FORMAT_FLAG); mpv_observe_property(mpv,4,"path",MPV_FORMAT_STRING); mpv_observe_property(mpv,5,"volume",MPV_FORMAT_DOUBLE); mpv_observe_property(mpv,6,"mute",MPV_FORMAT_FLAG); mpv_observe_property(mpv,7,"dwidth",MPV_FORMAT_INT64); mpv_observe_property(mpv,8,"dheight",MPV_FORMAT_INT64); mpv_set_wakeup_callback(mpv,wakeup,this);} 
MpvWidget::~MpvWidget(){makeCurrent(); if(ctx)mpv_render_context_free(ctx); doneCurrent(); if(mpv)mpv_terminate_destroy(mpv);} 
void MpvWidget::loadFile(const QString&p){cur=p; QByteArray b=p.toUtf8(); const char*cmd[]={"loadfile",b.constData(),"replace",nullptr}; check_mpv_error(mpv_command_async(mpv,0,cmd));} void MpvWidget::stopPlayback(){cur.clear(); const char*cmd[]={"stop",nullptr}; check_mpv_error(mpv_command_async(mpv,0,cmd)); update();} QString MpvWidget::currentFilePath()const{return cur;} void MpvWidget::togglePause(){const char*cmd[]={"cycle","pause",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));} void MpvWidget::toggleMute(){const char*cmd[]={"cycle","mute",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));} void MpvWidget::changeVolume(double d){QByteArray v=QByteArray::number(d,'f',1); const char*cmd[]={"add","volume",v.constData(),nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));} void MpvWidget::setVolume(double v){QByteArray b=QByteArray::number(qBound(0.0,v,130.0),'f',1); const char*cmd[]={"set","volume",b.constData(),nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));} void MpvWidget::setMute(bool m){const char*cmd[]={"set","mute",m?"yes":"no",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));} void MpvWidget::seekAbsolute(double s){QByteArray b=QByteArray::number(s,'f',3); const char*cmd[]={"seek",b.constData(),"absolute","exact",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));} void MpvWidget::seekRelative(double s){QByteArray b=QByteArray::number(s,'f',3); const char*cmd[]={"seek",b.constData(),"relative","exact",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));}
double MpvWidget::effectiveVideoScale()const{
    if(cropVideoToScale)return maxVideoScale;
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
void MpvWidget::setCropVideoToScale(bool crop){
    cropVideoToScale=crop;
    applyVideoScale();
    if(infoOverlayPinned)showScaleOverlay();
}
void MpvWidget::setPause(bool paused){const char*cmd[]={"set","pause",paused?"yes":"no",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));}
void MpvWidget::frameStepForward(){const char*cmd[]={"frame-step",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));}
void MpvWidget::frameStepBackward(){const char*cmd[]={"frame-back-step",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));}
void MpvWidget::seekAbsoluteKeyframe(double s){QByteArray b=QByteArray::number(s,'f',3); const char*cmd[]={"seek",b.constData(),"absolute","keyframes",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));}
void MpvWidget::seekRelativeKeyframe(double s){QByteArray b=QByteArray::number(s,'f',3); const char*cmd[]={"seek",b.constData(),"relative","keyframes",nullptr};check_mpv_error(mpv_command_async(mpv,0,cmd));}
void MpvWidget::setMaxVideoScale(double scale){ double clamped=qBound(0.5,scale,2.0); maxVideoScale=clamped; applyVideoScale();}
void MpvWidget::resizeEvent(QResizeEvent*e){QOpenGLWidget::resizeEvent(e); applyVideoScale(); if(playbackOverlay)playbackOverlay->setGeometry(rect());}
void MpvWidget::initializeGL(){mpv_opengl_init_params gl{getProc,nullptr}; mpv_render_param ps[]={{MPV_RENDER_PARAM_API_TYPE,const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},{MPV_RENDER_PARAM_OPENGL_INIT_PARAMS,&gl},{MPV_RENDER_PARAM_INVALID,nullptr}}; check_mpv_error(mpv_render_context_create(&ctx,mpv,ps)); mpv_render_context_set_update_callback(ctx,render,this);} 
void MpvWidget::paintGL(){if(!ctx)return; if(cur.isEmpty()){auto*f=QOpenGLContext::currentContext()->functions(); f->glClearColor(0.0f,0.0f,0.0f,1.0f); f->glClear(GL_COLOR_BUFFER_BIT); return;} qreal s=devicePixelRatioF(); mpv_opengl_fbo f{int(defaultFramebufferObject()),int(width()*s),int(height()*s),0}; int flip=1; mpv_render_param ps[]={{MPV_RENDER_PARAM_OPENGL_FBO,&f},{MPV_RENDER_PARAM_FLIP_Y,&flip},{MPV_RENDER_PARAM_INVALID,nullptr}}; mpv_render_context_render(ctx,ps);} 
void*MpvWidget::getProc(void*,const char*n){auto*c=QOpenGLContext::currentContext();return c?reinterpret_cast<void*>(c->getProcAddress(QByteArray(n))):nullptr;} void MpvWidget::render(void*o){auto*s=static_cast<MpvWidget*>(o);QMetaObject::invokeMethod(s,[s]{s->update();},Qt::QueuedConnection);} void MpvWidget::wakeup(void*o){auto*s=static_cast<MpvWidget*>(o);QMetaObject::invokeMethod(s,[s]{s->events();},Qt::QueuedConnection);} void MpvWidget::events(){while(mpv){auto*e=mpv_wait_event(mpv,0); if(!e||e->event_id==MPV_EVENT_NONE)break; if(e->event_id==MPV_EVENT_PROPERTY_CHANGE){prop(static_cast<mpv_event_property*>(e->data));} else if(e->event_id==MPV_EVENT_END_FILE){auto*ef=static_cast<mpv_event_end_file*>(e->data); if(!ef||ef->reason==MPV_END_FILE_REASON_EOF||ef->reason==MPV_END_FILE_REASON_STOP){if(onEndFile)onEndFile();}}}} void MpvWidget::prop(mpv_event_property*p){if(!p||!p->data)return; QString n=p->name; if(n=="time-pos"&&p->format==MPV_FORMAT_DOUBLE){if(onPosition)onPosition(*static_cast<double*>(p->data));} else if(n=="duration"&&p->format==MPV_FORMAT_DOUBLE){if(onDuration)onDuration(*static_cast<double*>(p->data));} else if(n=="pause"&&p->format==MPV_FORMAT_FLAG){if(onPause)onPause(*static_cast<int*>(p->data)!=0);} else if(n=="volume"&&p->format==MPV_FORMAT_DOUBLE){if(onVolume)onVolume(*static_cast<double*>(p->data));} else if(n=="mute"&&p->format==MPV_FORMAT_FLAG){if(onMute)onMute(*static_cast<int*>(p->data)!=0);} else if(n=="dwidth"&&p->format==MPV_FORMAT_INT64){videoDisplayWidth=*static_cast<int64_t*>(p->data);applyVideoScale();} else if(n=="dheight"&&p->format==MPV_FORMAT_INT64){videoDisplayHeight=*static_cast<int64_t*>(p->data);applyVideoScale();} else if(n=="path"&&p->format==MPV_FORMAT_STRING){char*v=*static_cast<char**>(p->data);cur=QString::fromUtf8(v?v:""); if(onFile)onFile(cur);}}
