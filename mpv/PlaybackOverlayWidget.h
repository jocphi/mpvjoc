#pragma once
#include <QWidget>
#include <QSize>
#include <QString>

class PlaybackOverlayWidget: public QWidget{
public:
    explicit PlaybackOverlayWidget(QWidget*parent=nullptr);
    void showOverlay(bool paused,const QString&scale,const QString&details=QString(),bool showIcon=true,bool persistent=false,const QSize&viewport=QSize(),const QSize&rendered=QSize());
    void hideOverlay();
    void setWarpState(bool active,int factor);
    void showWarpFeedback(int factor);
protected:
    void paintEvent(QPaintEvent*)override;
private:
    bool pauseIcon=false;
    bool iconVisible=true;
    bool warpActive=false;
    int warpFactor=1;
    int generation=0;
    int warpFeedbackGeneration=0;
    QString scaleText;
    QString detailsText;
    QString warpFeedbackText;
    QSize viewportSize;
    QSize renderedSize;
};
