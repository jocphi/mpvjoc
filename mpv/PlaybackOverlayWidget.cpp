#include "PlaybackOverlayWidget.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QTimer>
#include <QtGlobal>

PlaybackOverlayWidget::PlaybackOverlayWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    hide();
}

void PlaybackOverlayWidget::setOverlayElementMask(int mask)
{
    overlayElementMask = mask & AllOverlayElements;
    update();
}


void PlaybackOverlayWidget::setVolumeText(const QString& text)
{
    volumeText = text;
    update();
}

void PlaybackOverlayWidget::showOverlay(bool paused,
                                        const QString& scale,
                                        const QString& details,
                                        bool showIcon,
                                        bool persistent,
                                        const QSize& viewport,
                                        const QSize& rendered)
{
    pauseIcon = paused;
    scaleText = scale;
    volumeText.clear();
    detailsText = details;
    iconVisible = showIcon;
    viewportSize = viewport;
    renderedSize = rendered;

    const int g = ++generation;
    show();
    raise();
    update();

    if (persistent)
        return;

    QTimer::singleShot(700, this, [this, g] {
        if (g == generation) {
            if (warpActive) {
                scaleText.clear();
                volumeText.clear();
                detailsText.clear();
                iconVisible = false;
                update();
            } else {
                hide();
            }
        }
    });
}


void PlaybackOverlayWidget::showVolumeOverlay(bool paused,
                                              const QString& text,
                                              const QString& scale,
                                              const QString& details,
                                              const QSize& viewport,
                                              const QSize& rendered)
{
    pauseIcon = paused;
    iconVisible = true;
    scaleText = scale;
    volumeText = text;
    detailsText = details;
    viewportSize = viewport;
    renderedSize = rendered;

    const int g = ++generation;
    show();
    raise();
    update();

    QTimer::singleShot(700, this, [this, g] {
        if (g == generation) {
            if (warpActive) {
                scaleText.clear();
                volumeText.clear();
                detailsText.clear();
                iconVisible = false;
                update();
            } else {
                hide();
            }
        }
    });
}



void PlaybackOverlayWidget::hideOverlay()
{
    hide();
}

void PlaybackOverlayWidget::setOverlayCells(int playStateCellValue, int scaleDisplayCellValue, int volumeOverlayCellValue, int visibilityMapCellValue, int warpLabelCellValue)
{
    playStateCell = qBound(1, playStateCellValue, 9);
    scaleDisplayCell = qBound(1, scaleDisplayCellValue, 9);
    volumeOverlayCell = qBound(1, volumeOverlayCellValue, 9);
    visibilityMapCell = qBound(1, visibilityMapCellValue, 9);
    warpLabelCell = qBound(1, warpLabelCellValue, 9);
    update();
}

void PlaybackOverlayWidget::setOverlayOpacities(int playStateOpacityValue,
                                                int scaleDisplayOpacityValue,
                                                int volumeOverlayOpacityValue,
                                                int visibilityMapOpacityValue,
                                                int warpLabelOpacityValue)
{
    playStateOpacity = qBound(0, playStateOpacityValue, 100);
    scaleDisplayOpacity = qBound(0, scaleDisplayOpacityValue, 100);
    volumeOverlayOpacity = qBound(0, volumeOverlayOpacityValue, 100);
    visibilityMapOpacity = qBound(0, visibilityMapOpacityValue, 100);
    warpLabelOpacity = qBound(0, warpLabelOpacityValue, 100);
    update();
}

void PlaybackOverlayWidget::setWarpState(bool active, int factor)
{
    warpActive = active;
    warpFactor = qBound(1, factor, 9);

    if (warpActive) {
        show();
        raise();
    } else if (scaleText.isEmpty() && volumeText.isEmpty() && detailsText.isEmpty()) {
        hide();
    }

    update();
}


void PlaybackOverlayWidget::showWarpFeedback(int factor)
{
    const int f = qBound(1, factor, 9);
    warpFeedbackText = QStringLiteral("Warp %1 · +%2s").arg(f).arg(f * 10);
    show();
    raise();
    update();

    const int g = ++warpFeedbackGeneration;
    QTimer::singleShot(900, this, [this, g] {
        if (g == warpFeedbackGeneration) {
            warpFeedbackText.clear();
            update();
        }
    });
}

QRectF PlaybackOverlayWidget::overlayCellRect(int cell) const
{
    cell = qBound(1, cell, 9);
    const int zeroBased = cell - 1;
    const int col = zeroBased % 3;
    const int row = zeroBased / 3;
    const qreal cellW = width() / 3.0;
    const qreal cellH = height() / 3.0;
    return QRectF(col * cellW, row * cellH, cellW, cellH);
}

void PlaybackOverlayWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (warpActive && (overlayElementMask & WarpLabelElement))
        drawWarpOverlay(p);

    if (iconVisible && (overlayElementMask & PlayStateElement))
        drawPlayStateOverlay(p, overlayCellRect(playStateCell));

    if (!scaleText.isEmpty() && (overlayElementMask & ScaleDisplayElement))
        drawTextOverlay(p, overlayCellRect(scaleDisplayCell), scaleText, scaleDisplayOpacity);

    if (!volumeText.isEmpty() && (overlayElementMask & VolumeOverlayElement))
        drawTextOverlay(p, overlayCellRect(volumeOverlayCell), volumeText, volumeOverlayOpacity);

    if (!detailsText.isEmpty() && viewportSize.isValid() && renderedSize.isValid() && (overlayElementMask & VisibilityMapElement))
        drawVisibilityMap(p, overlayCellRect(visibilityMapCell));
}


void PlaybackOverlayWidget::drawWarpOverlay(QPainter& p)
{
    p.save();
    p.setOpacity(warpLabelOpacity / 100.0);
    p.setPen(QPen(QColor(255, 0, 0, 235), 3));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(1, 1, -2, -2));

    const QRectF cell = overlayCellRect(warpLabelCell).adjusted(8, 8, -8, -8);
    QString warpText = warpFeedbackText.isEmpty() ? QStringLiteral("Warp %1").arg(warpFactor) : warpFeedbackText;

    QFont font = p.font();
    font.setBold(true);
    font.setPointSizeF(qBound(11.0, qMin(cell.width(), cell.height()) * 0.13, 24.0));
    p.setFont(font);

    QFontMetricsF fm(font);
    const qreal textW = fm.horizontalAdvance(warpText) + 18.0;
    const qreal textH = fm.height() + 8.0;
    QRectF bg(cell.right() - textW, cell.top(), textW, textH);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 145));
    p.drawRoundedRect(bg, 6, 6);
    p.setPen(QColor(255, 70, 70, 245));
    p.drawText(bg, Qt::AlignCenter, warpText);
    p.restore();
}

void PlaybackOverlayWidget::drawPlayStateOverlay(QPainter& p, const QRectF& cell)
{
    p.save();
    p.setOpacity(playStateOpacity / 100.0);

    const qreal side = qMin(cell.width(), cell.height());
    const qreal icon = qBound(38.0, side * 0.44, 120.0);
    const QPointF c(cell.center());
    const qreal bgW = qMin(cell.width() * 0.78, icon * 1.55);
    const qreal bgH = qMin(cell.height() * 0.72, icon * 1.40);
    const QRectF bgRect(c.x() - bgW / 2.0, c.y() - bgH / 2.0, bgW, bgH);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 145));
    p.drawRoundedRect(bgRect, bgW * 0.14, bgW * 0.14);

    p.setBrush(QColor(255, 255, 255, 235));
    if (pauseIcon) {
        const qreal bw = icon * 0.22;
        const qreal bh = icon * 0.72;
        const qreal gap = icon * 0.18;
        const qreal top = c.y() - bh / 2.0;
        const qreal lx = c.x() - gap / 2.0 - bw;
        const qreal rx = c.x() + gap / 2.0;
        p.drawRoundedRect(QRectF(lx, top, bw, bh), bw * 0.28, bw * 0.28);
        p.drawRoundedRect(QRectF(rx, top, bw, bh), bw * 0.28, bw * 0.28);
    } else {
        QPolygonF tri;
        tri << QPointF(c.x() - icon * 0.24, c.y() - icon * 0.36)
            << QPointF(c.x() - icon * 0.24, c.y() + icon * 0.36)
            << QPointF(c.x() + icon * 0.38, c.y());
        p.drawPolygon(tri);
    }

    p.restore();
}

void PlaybackOverlayWidget::drawTextOverlay(QPainter& p, const QRectF& rawCell, const QString& text, int opacity)
{
    if (text.isEmpty())
        return;

    p.save();
    p.setOpacity(qBound(0, opacity, 100) / 100.0);

    const QRectF cell = rawCell.adjusted(10, 10, -10, -10);
    const qreal side = qMin(cell.width(), cell.height());
    QFont font = p.font();
    font.setBold(true);
    font.setPointSizeF(qBound(11.0, side * 0.16, 28.0));
    p.setFont(font);
    QFontMetricsF fm(font);

    const qreal textW = qMin(cell.width(), fm.horizontalAdvance(text) + 26.0);
    const qreal textH = fm.height() + 12.0;
    const QRectF bg(cell.center().x() - textW / 2.0, cell.center().y() - textH / 2.0, textW, textH);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 145));
    p.drawRoundedRect(bg, 8, 8);
    p.setPen(QColor(255, 255, 255, 235));
    p.drawText(bg, Qt::AlignHCenter | Qt::AlignVCenter, text);

    p.restore();
}

void PlaybackOverlayWidget::drawVisibilityMap(QPainter& p, const QRectF& rawCell)
{
    QStringList dimensionLines = detailsText.split(QStringLiteral("\n"));
    QString sourceText = dimensionLines.value(0);
    QString actualText = dimensionLines.value(1);
    if (actualText.isEmpty())
        actualText = detailsText;

    const QRectF cell = rawCell.adjusted(10, 8, -10, -10);
    if (cell.width() <= 10 || cell.height() <= 10)
        return;

    p.save();
    p.setOpacity(visibilityMapOpacity / 100.0);

    QFont font = p.font();
    font.setBold(false);
    font.setPointSizeF(qBound(8.0, qMin(cell.width(), cell.height()) * 0.075, 15.0));
    p.setFont(font);
    QFontMetricsF fm(font);

    const qreal textH = fm.height();
    const qreal gap = 4.0;
    QRectF sourceRect(cell.left(), cell.top(), cell.width(), textH);
    QRectF actualRect(cell.left(), cell.bottom() - textH, cell.width(), textH);
    QRectF mapArea(cell.left(), sourceRect.bottom() + gap, cell.width(), cell.height() - 2.0 * textH - 2.0 * gap);
    if (mapArea.height() <= 8) {
        p.restore();
        return;
    }

    p.setPen(QColor(255, 255, 255, 235));
    p.drawText(sourceRect, Qt::AlignHCenter | Qt::AlignVCenter, sourceText);
    p.drawText(actualRect, Qt::AlignHCenter | Qt::AlignVCenter, actualText);

    const qreal vw = viewportSize.width();
    const qreal vh = viewportSize.height();
    const qreal rw = renderedSize.width();
    const qreal rh = renderedSize.height();
    if (vw <= 0 || vh <= 0 || rw <= 0 || rh <= 0) {
        p.restore();
        return;
    }

    const QRectF viewportRect(0, 0, vw, vh);
    const QRectF videoRect((vw - rw) / 2.0, (vh - rh) / 2.0, rw, rh);
    const QRectF visibleRect = viewportRect.intersected(videoRect);
    const QRectF unionRect = viewportRect.united(videoRect);

    const qreal mapScale = qMin(mapArea.width() / unionRect.width(), mapArea.height() / unionRect.height());
    const QPointF mapCenter(mapArea.center());

    auto mapRect = [&](const QRectF& r) {
        return QRectF(mapCenter.x() + (r.left() - unionRect.center().x()) * mapScale,
                      mapCenter.y() + (r.top() - unionRect.center().y()) * mapScale,
                      r.width() * mapScale,
                      r.height() * mapScale);
    };

    const QRectF redRect = mapRect(videoRect);
    const QRectF greenRect = mapRect(visibleRect);
    const QRectF whiteRect = mapRect(viewportRect);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 0, 0, 220));
    p.drawRect(redRect);
    p.setBrush(QColor(40, 255, 0, 235));
    p.drawRect(greenRect);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 245), qMax(1.0, qMin(cell.width(), cell.height()) * 0.008)));
    p.drawRect(whiteRect);

    p.restore();
}
