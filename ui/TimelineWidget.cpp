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
void TimelineWidget::paintEvent(QPaintEvent*){QPainter p(this);p.fillRect(rect(),QColor(16,16,16)); QRect bar(8,5,qMax(1,width()-16),7); p.fillRect(bar,QColor(16,48,16)); double ep=drag?prev:pos; QRect fill=bar; fill.setWidth(int(bar.width()*(dur>0?qBound(0.0,ep/dur,1.0):0))); p.fillRect(fill,QColor(0,204,51)); QString timeText=formatHMS(ep)+" / "+formatHMS(dur); int timeW=p.fontMetrics().horizontalAdvance(timeText)+14; QRect timeRect(8,18,timeW,18); p.setPen(QColor(0,204,51)); p.drawText(timeRect,Qt::AlignLeft|Qt::AlignVCenter,timeText); p.setPen(Qt::white); int titleX=timeRect.right()+8; drawHighlightedTitle(&p,QRect(titleX,18,qMax(1,width()-titleX-8),18),title.isEmpty()?"No file loaded":title);}
void TimelineWidget::mousePressEvent(QMouseEvent*e){if(e->button()==Qt::LeftButton){drag=true;upd(e->position().x());if(previewPositionChanged)previewPositionChanged(prev);update();}} void TimelineWidget::mouseMoveEvent(QMouseEvent*e){if(drag){upd(e->position().x());if(previewPositionChanged)previewPositionChanged(prev);update();}} void TimelineWidget::mouseReleaseEvent(QMouseEvent*e){if(drag&&e->button()==Qt::LeftButton){upd(e->position().x());drag=false;pos=prev;if(seekRequested)seekRequested(pos);update();}}
void TimelineWidget::upd(double x){prev=dur*qBound(0.0,(x-8)/qMax(1,width()-16),1.0);}
