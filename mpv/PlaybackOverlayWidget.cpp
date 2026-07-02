#include "PlaybackOverlayWidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QPolygonF>
#include <QPen>
#include <QFont>
#include <QFontMetricsF>
#include <QStringList>
#include <QtGlobal>

PlaybackOverlayWidget::PlaybackOverlayWidget(QWidget*parent):QWidget(parent){
    setAttribute(Qt::WA_TransparentForMouseEvents,true);
    setAttribute(Qt::WA_TranslucentBackground,true);
    hide();
}

void PlaybackOverlayWidget::showOverlay(bool paused,const QString&scale,const QString&details,bool showIcon,bool persistent,const QSize&viewport,const QSize&rendered){
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
    QTimer::singleShot(700,this,[this,g]{if(g==generation){if(warpActive){scaleText.clear();detailsText.clear();iconVisible=false;update();}else hide();}});
}

void PlaybackOverlayWidget::hideOverlay(){hide();}

void PlaybackOverlayWidget::setWarpState(bool active,int factor){
    warpActive=active;
    warpFactor=qBound(1,factor,9);
    if(warpActive){show();raise();}else if(scaleText.isEmpty()&&detailsText.isEmpty())hide();
    update();
}

void PlaybackOverlayWidget::showWarpFeedback(int factor){
    int f=qBound(1,factor,9);
    warpFeedbackText=QString("Warp %1 · +%2s").arg(f).arg(f*10);
    show();
    raise();
    update();
    int g=++warpFeedbackGeneration;
    QTimer::singleShot(900,this,[this,g]{if(g==warpFeedbackGeneration){warpFeedbackText.clear();update();}});
}

void PlaybackOverlayWidget::paintEvent(QPaintEvent*){
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
        QString warpText=warpFeedbackText.isEmpty()?QString("Warp %1").arg(warpFactor):warpFeedbackText;
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
