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
    detailsText = details;
    iconVisible = showIcon;
    viewportSize = viewport;
    renderedSize = rendered;

    // Warp mode owns the video overlay while active. Keep only Warp border/label.
    if (warpActive) {
        scaleText.clear();
        detailsText.clear();
        iconVisible = false;
        viewportSize = QSize();
        renderedSize = QSize();
    }

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
                detailsText.clear();
                iconVisible = false;
                viewportSize = QSize();
                renderedSize = QSize();
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

void PlaybackOverlayWidget::setWarpState(bool active, int factor)
{
    warpActive = active;
    warpFactor = qBound(1, factor, 9);

    if (warpActive) {
        // Suppress normal overlay content while Warp is active.
        scaleText.clear();
        detailsText.clear();
        iconVisible = false;
        viewportSize = QSize();
        renderedSize = QSize();
        show();
        raise();
    } else if (scaleText.isEmpty() && detailsText.isEmpty()) {
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

    if (warpActive)
        drawWarpOverlay(p);

    if (iconVisible || !scaleText.isEmpty())
        drawCenterOverlay(p, overlayCellRect(centerOverlayCell));

    if (!detailsText.isEmpty() && viewportSize.isValid() && renderedSize.isValid())
        drawVisibilityMap(p, overlayCellRect(visibilityMapCell));
}

void PlaybackOverlayWidget::drawWarpOverlay(QPainter& p)
{
    p.save();
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

void PlaybackOverlayWidget::drawCenterOverlay(QPainter& p, const QRectF& cell)
{
    p.save();

    const qreal side = qMin(cell.width(), cell.height());
    const qreal icon = qBound(38.0, side * 0.34, 112.0);
    const QPointF c(cell.center().x(), cell.center().y() - icon * 0.08);
    const qreal bgW = qMin(cell.width() * 0.86, icon * 1.75);
    const qreal bgH = qMin(cell.height() * 0.76, icon * 1.55);
    QRectF bgRect(c.x() - bgW / 2.0, c.y() - bgH / 2.0, bgW, bgH);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 145));
    p.drawRoundedRect(bgRect, bgW * 0.14, bgW * 0.14);

    p.setBrush(QColor(255, 255, 255, 235));
    if (iconVisible && pauseIcon) {
        const qreal bw = icon * 0.22;
        const qreal bh = icon * 0.72;
        const qreal gap = icon * 0.18;
        const qreal top = c.y() - bh / 2.0;
        const qreal lx = c.x() - gap / 2.0 - bw;
        const qreal rx = c.x() + gap / 2.0;
        p.drawRoundedRect(QRectF(lx, top, bw, bh), bw * 0.28, bw * 0.28);
        p.drawRoundedRect(QRectF(rx, top, bw, bh), bw * 0.28, bw * 0.28);
    } else if (iconVisible) {
        QPolygonF tri;
        tri << QPointF(c.x() - icon * 0.24, c.y() - icon * 0.36)
            << QPointF(c.x() - icon * 0.24, c.y() + icon * 0.36)
            << QPointF(c.x() + icon * 0.38, c.y());
        p.drawPolygon(tri);
    }

    if (!scaleText.isEmpty()) {
        QFont font = p.font();
        font.setBold(true);
        font.setPointSizeF(qBound(10.0, side * 0.085, 24.0));
        p.setFont(font);
        p.setPen(QColor(255, 255, 255, 235));

        const qreal textY = iconVisible ? c.y() + icon * 0.42 : cell.center().y() - side * 0.07;
        QRectF textRect(cell.left() + 4, textY, cell.width() - 8, side * 0.18);
        p.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, scaleText);
    }

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
    if (mapArea.height() <= 8)
        return;

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
    const qreal mapW = unionRect.width() * mapScale;
    const qreal mapH = unionRect.height() * mapScale;
    const QPointF mapCenter(mapArea.center());

    auto mapRect = [&](const QRectF& r) {
        return QRectF(mapCenter.x() + (r.left() - unionRect.center().x()) * mapScale,
                      mapCenter.y() + (r.top() - unionRect.center().y()) * mapScale,
                      r.width() * mapScale,
                      r.height() * mapScale);
    };

    Q_UNUSED(mapW);
    Q_UNUSED(mapH);

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
