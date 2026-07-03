#pragma once
#include <QSize>
#include <QString>
#include <QWidget>

class PlaybackOverlayWidget : public QWidget {
public:
    explicit PlaybackOverlayWidget(QWidget* parent = nullptr);

    void showOverlay(bool paused,
                     const QString& scale,
                     const QString& details = QString(),
                     bool showIcon = true,
                     bool persistent = false,
                     const QSize& viewport = QSize(),
                     const QSize& rendered = QSize());
    void hideOverlay();

    void setWarpState(bool active, int factor);
    void showWarpFeedback(int factor);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QRectF overlayCellRect(int cell) const;
    void drawCenterOverlay(QPainter& p, const QRectF& cell);
    void drawVisibilityMap(QPainter& p, const QRectF& cell);
    void drawWarpOverlay(QPainter& p);

    bool pauseIcon = false;
    bool iconVisible = true;
    QString scaleText;
    QString detailsText;
    QSize viewportSize;
    QSize renderedSize;
    int generation = 0;

    bool warpActive = false;
    int warpFactor = 1;
    int warpFeedbackGeneration = 0;
    QString warpFeedbackText;

    // Default grid layout. These become user-configurable in a later patch.
    int centerOverlayCell = 5;
    int visibilityMapCell = 9;
    int warpLabelCell = 3;
};
