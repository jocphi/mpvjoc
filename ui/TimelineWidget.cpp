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

TimelineWidget::TimelineWidget(QWidget*p):QWidget(p){setMinimumHeight(38);setMouseTracking(true);} 
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

QColor TimelineWidget::timelineHueTone(int r,int g,int b,int a,bool focusTone)const{
    const QColor original(r,g,b,a);
    const QColor hue=focusTone?timelineFocusHue:timelineNormalHue;
    const int lightness=original.lightness();
    const int alpha=original.alpha();
    const int sourceSat=original.hslSaturation()<0?0:original.hslSaturation();
    const int hueValue=hue.hslHue();
    const int hueSat=hue.hslSaturation()<0?0:hue.hslSaturation();
    if(hueValue<0||hueSat<8)return QColor::fromHsl(0,0,lightness,alpha);
    const int saturation=sourceSat<12?hueSat:sourceSat;
    return QColor::fromHsl(hueValue,saturation,lightness,alpha);
}

void TimelineWidget::paintEvent(QPaintEvent*){
    const QColor timelineTrackColor=playlistFocusMode?QColor(54,0,0):QColor(0,45,0);
    const QColor timelinePlayedColor=playlistFocusMode?timelineHueTone(255,0,0,255,true):timelineHueTone(0,220,40,255,false);
QPainter p(this);p.fillRect(rect(),QColor(16,16,16)); QRect bar(8,5,qMax(1,width()-16),7); p.fillRect(bar,timelineTrackColor); double ep=drag?prev:pos; QRect fill=bar; fill.setWidth(int(bar.width()*(dur>0?qBound(0.0,ep/dur,1.0):0))); p.fillRect(fill,timelinePlayedColor); QString timeText=formatHMS(ep)+" / "+formatHMS(dur); int timeW=p.fontMetrics().horizontalAdvance(timeText)+14; QRect timeRect(8,18,timeW,18); p.setPen(timelinePlayedColor); p.drawText(timeRect,Qt::AlignLeft|Qt::AlignVCenter,timeText); p.setPen(Qt::white); int titleX=timeRect.right()+8; drawHighlightedTitle(&p,QRect(titleX,18,qMax(1,width()-titleX-8),18),title.isEmpty()?"No file loaded":title);}void TimelineWidget::mousePressEvent(QMouseEvent*e){if(e->button()==Qt::LeftButton){drag=true;upd(e->position().x());if(previewPositionChanged)previewPositionChanged(prev);update();}} void TimelineWidget::mouseMoveEvent(QMouseEvent*e){if(drag){upd(e->position().x());if(previewPositionChanged)previewPositionChanged(prev);update();}} void TimelineWidget::mouseReleaseEvent(QMouseEvent*e){if(drag&&e->button()==Qt::LeftButton){upd(e->position().x());drag=false;pos=prev;if(seekRequested)seekRequested(pos);update();}}
void TimelineWidget::upd(double x){prev=dur*qBound(0.0,(x-8)/qMax(1,width()-16),1.0);}
