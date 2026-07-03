#pragma once
#include <QColor>
#include <QWidget>
#include <functional>

class TimelineWidget: public QWidget{
public:
    void setTimelineNormalHue(const QColor& color);
    void setTimelineFocusHue(const QColor& color);
    void setTimelineHues(const QColor& normalColor,const QColor& focusColor);
    void setTimelineNormalDarkTonePercent(int percent);
    void setTimelineFocusDarkTonePercent(int percent);
    void setTimelineDarkTonePercents(int normalPercent,int focusPercent);
    explicit TimelineWidget(QWidget*p=nullptr);
    void setPlaylistFocusMode(bool active);
    std::function<void(double)> seekRequested;
    std::function<void(double)> previewPositionChanged;
    void setPosition(double s);
    void setDuration(double s);
    void setTitle(const QString&t);
protected:
    void paintEvent(QPaintEvent*)override;
    void mousePressEvent(QMouseEvent*e)override;
    void mouseMoveEvent(QMouseEvent*e)override;
    void mouseReleaseEvent(QMouseEvent*e)override;
private:
    QColor timelineHueTone(int r,int g,int b,int a,bool focusTone)const;
    QColor timelineNormalHue=QColor(64,220,130);
    QColor timelineFocusHue=QColor(230,60,60);
    int timelineNormalDarkTonePercent=80;
    int timelineFocusDarkTonePercent=80;
    bool playlistFocusMode=false;
    void upd(double x);
    double pos=0,dur=0,prev=0;
    bool drag=false;
    QString title;
};
