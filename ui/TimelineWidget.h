#pragma once
#include <QWidget>
#include <functional>

class TimelineWidget: public QWidget{
public:
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
    bool playlistFocusMode=false;
    void upd(double x);
    double pos=0,dur=0,prev=0;
    bool drag=false;
    QString title;
};
