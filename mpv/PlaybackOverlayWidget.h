#pragma once
#include <QSize>
#include <QString>
#include <QWidget>

class PlaybackOverlayWidget : public QWidget {
public:
    enum OverlayElement {
        PlayStateElement = 1 << 0,
        ScaleDisplayElement = 1 << 1,
        VolumeOverlayElement = 1 << 2,
        VisibilityMapElement = 1 << 3,
        WarpLabelElement = 1 << 4,
        AllOverlayElements = PlayStateElement | ScaleDisplayElement | VolumeOverlayElement | VisibilityMapElement | WarpLabelElement
    };

    explicit PlaybackOverlayWidget(QWidget* parent = nullptr);

    void showOverlay(bool paused,
                     const QString& scale,
                     const QString& details = QString(),
                     bool showIcon = true,
                     bool persistent = false,
                     const QSize& viewport = QSize(),
                     const QSize& rendered = QSize());
    void showVolumeOverlay(bool paused,
                           const QString& volumeText,
                           const QString& scaleText = QString(),
                           const QString& details = QString(),
                           const QSize& viewport = QSize(),
                           const QSize& rendered = QSize());
    void hideOverlay();

    void setOverlayElementMask(int mask);
    void setVolumeText(const QString& text);
    void setWarpState(bool active, int factor);
    void showWarpFeedback(int factor);
    void setOverlayCells(int playStateCell, int scaleDisplayCell, int volumeOverlayCell, int visibilityMapCell, int warpLabelCell);
    void setOverlayOpacities(int playStateOpacity, int scaleDisplayOpacity, int volumeOverlayOpacity, int visibilityMapOpacity, int warpLabelOpacity);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QRectF overlayCellRect(int cell) const;
    void drawPlayStateOverlay(QPainter& p, const QRectF& cell);
    void drawTextOverlay(QPainter& p, const QRectF& cell, const QString& text, int opacity);
    void drawVisibilityMap(QPainter& p, const QRectF& cell);
    void drawWarpOverlay(QPainter& p);

    bool pauseIcon = false;
    bool iconVisible = true;
    QString scaleText;
    QString volumeText;
    QString detailsText;
    QSize viewportSize;
    QSize renderedSize;
    int generation = 0;

    bool warpActive = false;
    int warpFactor = 1;
    int warpFeedbackGeneration = 0;
    QString warpFeedbackText;

    int overlayElementMask = AllOverlayElements;

    int playStateCell = 5;
    int scaleDisplayCell = 8;
    int volumeOverlayCell = 6;
    int visibilityMapCell = 9;
    int warpLabelCell = 3;

    int playStateOpacity = 100;
    int scaleDisplayOpacity = 100;
    int volumeOverlayOpacity = 100;
    int visibilityMapOpacity = 100;
    int warpLabelOpacity = 100;
};
