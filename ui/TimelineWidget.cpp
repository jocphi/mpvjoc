#include "TimelineWidget.h"
#include "util/TextHighlighting.h"
#include "util/Utils.h"
#include <QPainter>
#include <QMouseEvent>
#include <QtGlobal>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

TimelineWidget::TimelineWidget(QWidget*p):QWidget(p){setMinimumHeight(14);setMaximumHeight(14);setMouseTracking(true);} 
void TimelineWidget::setPosition(double s){if(!drag){pos=qMax(0.0,s);update();}} void TimelineWidget::setDuration(double s){dur=qMax(0.0,s);update();} void TimelineWidget::setTitle(const QString&t){title=t;update();}
void TimelineWidget::setPlaylistFocusMode(bool active){if(playlistFocusMode==active)return;playlistFocusMode=active;update();}


void TimelineWidget::setTimelineNormalHue(const QColor&color){
    if(timelineNormalHue==color)return;
    timelineNormalHue=color;
    update();
}

void TimelineWidget::setTimelineFocusHue(const QColor&color){
    if(timelineFocusHue==color)return;
    timelineFocusHue=color;
    update();
}

void TimelineWidget::setTimelineHues(const QColor&normalColor,const QColor&focusColor){
    bool changed=false;
    if(timelineNormalHue!=normalColor){timelineNormalHue=normalColor;changed=true;}
    if(timelineFocusHue!=focusColor){timelineFocusHue=focusColor;changed=true;}
    if(changed)update();
}


void TimelineWidget::setTimelineNormalDarkTonePercent(int percent){
    percent=qBound(0,percent,100);
    if(timelineNormalDarkTonePercent==percent)return;
    timelineNormalDarkTonePercent=percent;
    update();
}

void TimelineWidget::setTimelineFocusDarkTonePercent(int percent){
    percent=qBound(0,percent,100);
    if(timelineFocusDarkTonePercent==percent)return;
    timelineFocusDarkTonePercent=percent;
    update();
}

void TimelineWidget::setTimelineDarkTonePercents(int normalPercent,int focusPercent){
    normalPercent=qBound(0,normalPercent,100);
    focusPercent=qBound(0,focusPercent,100);
    bool changed=false;
    if(timelineNormalDarkTonePercent!=normalPercent){timelineNormalDarkTonePercent=normalPercent;changed=true;}
    if(timelineFocusDarkTonePercent!=focusPercent){timelineFocusDarkTonePercent=focusPercent;changed=true;}
    if(changed)update();
}

QColor TimelineWidget::timelineHueTone(int r,int g,int b,int a,bool focusTone)const{
    const QColor original(r,g,b,a);
    const QColor hue=focusTone?timelineFocusHue:timelineNormalHue;
    const int alpha=original.alpha();
    const int percent=focusTone?timelineFocusDarkTonePercent:timelineNormalDarkTonePercent;
    const bool darkTone=qMax(original.red(),qMax(original.green(),original.blue()))<96;
    auto scaled=[percent,alpha](const QColor&c){
        return QColor(qBound(0,c.red()*percent/100,255),
                      qBound(0,c.green()*percent/100,255),
                      qBound(0,c.blue()*percent/100,255),
                      alpha);
    };
    if(darkTone)return scaled(hue);
    return QColor(hue.red(),hue.green(),hue.blue(),alpha);
}


void TimelineWidget::paintEvent(QPaintEvent*){
    const QColor timelineTrackColor=playlistFocusMode?timelineHueTone(54,0,0,255,true):timelineHueTone(0,45,0,255,false);
    const QColor timelinePlayedColor=playlistFocusMode?timelineHueTone(255,0,0,255,true):timelineHueTone(0,220,40,255,false);
    QPainter p(this);
    p.fillRect(rect(),QColor(16,16,16));
    QRect bar(4,3,qMax(1,width()-8),8);
    p.fillRect(bar,timelineTrackColor);
    const double ep=drag?prev:pos;
    QRect fill=bar;
    fill.setWidth(int(bar.width()*(dur>0?qBound(0.0,ep/dur,1.0):0)));
    p.fillRect(fill,timelinePlayedColor);
}void TimelineWidget::mousePressEvent(QMouseEvent*e){if(e->button()==Qt::LeftButton){drag=true;upd(e->position().x());if(previewPositionChanged)previewPositionChanged(prev);update();}} void TimelineWidget::mouseMoveEvent(QMouseEvent*e){if(drag){upd(e->position().x());if(previewPositionChanged)previewPositionChanged(prev);update();}} void TimelineWidget::mouseReleaseEvent(QMouseEvent*e){if(drag&&e->button()==Qt::LeftButton){upd(e->position().x());drag=false;pos=prev;if(seekRequested)seekRequested(pos);update();}}
void TimelineWidget::upd(double x){prev=dur*qBound(0.0,(x-8)/qMax(1,width()-16),1.0);}
