#include "PlaylistDelegate.h"
#include "util/TextHighlighting.h"
#include "PlaylistDurationRuns.h"
#include "PlaylistBadges.h"
#include "PlaylistFormatting.h"
#include "PlaylistModel.h"
#include "util/Utils.h"
#include <QAbstractItemModel>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTextStream>
#include <QtGlobal>

static bool gPlaylistSizeDisplayGB = false;
bool PlaylistDelegate::editorEvent(
    QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& o, const QModelIndex& i)
{
    if (event && event->type() == QEvent::MouseButtonRelease) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            int metaY = o.rect.top() + 31;
            int lineH = 18;
            int rightPad = 8;
            int sizeW = o.fontMetrics.horizontalAdvance(
                            gPlaylistSizeDisplayGB ? QStringLiteral("000.000,00 GB") : QStringLiteral("000.000,00 MB"))
                + 6;
            QRect sizeRect(o.rect.right() - rightPad - sizeW, metaY, sizeW, lineH);
            if (sizeRect.contains(me->position().toPoint())) {
                gPlaylistSizeDisplayGB = !gPlaylistSizeDisplayGB;
                if (model && model->rowCount() > 0)
                    emit const_cast<QAbstractItemModel*>(model)->dataChanged(
                        model->index(0, 0), model->index(model->rowCount() - 1, 0));
                return true;
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, o, i);
}
PlaylistDelegate::PlaylistDelegate(QObject* p)
    : QStyledItemDelegate(p)
{
}
QSize PlaylistDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const
{
    return { 280, 78 };
}
void PlaylistDelegate::paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& i) const
{
    p->save();
    QRect r = o.rect;
    p->fillRect(r,
        (o.state & QStyle::State_Selected) ? QColor(38, 82, 48)
                                           : (i.row() % 2 ? QColor(36, 36, 36) : QColor(24, 24, 24)));
    QRect th(r.left() + 8, r.top() + 6, 96, 54);
    p->fillRect(th, QColor(8, 8, 8));
    p->setPen(QColor(60, 60, 60));
    p->drawRect(th.adjusted(0, 0, -1, -1));
    QString tp = i.data(PlaylistModel::ThumbnailPathRole).toString();
    if (i.data(PlaylistModel::ThumbnailReadyRole).toBool() && !tp.isEmpty()) {
        if (!cache.contains(tp))
            cache.insert(tp, QPixmap(tp));
        p->drawPixmap(th, cache.value(tp));
    } else {
        p->setPen(QColor(120, 120, 120));
        p->drawText(
            th, Qt::AlignCenter, i.data(PlaylistModel::ThumbnailAttemptedRole).toBool() ? "no thumb" : "thumb…");
    }
    int x = th.right() + 10;
    QString title = i.data(PlaylistModel::TitleRole).toString();
    double dur = i.data(PlaylistModel::DurationRole).toDouble();
    bool dk = i.data(PlaylistModel::DurationKnownRole).toBool();
    QString codec = i.data(PlaylistModel::CodecRole).toString();
    QString res = i.data(PlaylistModel::ResolutionRole).toString();
    QString cont = i.data(PlaylistModel::ContainerRole).toString();
    qint64 sizeBytes = i.data(PlaylistModel::SizeRole).toLongLong();
    QString sizeText = formatEuroSize(sizeBytes, gPlaylistSizeDisplayGB);
    QString meta = QStringList { res, cont }.join("  •  ");
    QString heightBadge;
    QColor heightColor(80, 80, 80);
    QString codecBadge = codec;
    QColor codecColor = codecBadgeColor(codec);

    int vh = 0;
    QStringList rp = res.split('x');
    if (rp.size() == 2)
        vh = rp[1].toInt();
    if (vh > 0) {
        heightBadge = resolutionBadgeText(vh);
        heightColor = resolutionBadgeColor(vh);
    }
    QRect titleRect(x, r.top() + 7, r.width() - x + r.left() - 90, 20);
    drawHighlightedTitle(p, titleRect, title);
    QRect durationRect(r.right() - 86, r.top() + 7, 78, 20);
    const DuplicateDurationRunInfo duplicateRun = duplicateDurationRunInfo(i, dk, dur);
    if (duplicateRun.isDuplicate) {
        p->setPen(Qt::NoPen);
        p->setBrush(duplicateRun.color);
        p->drawRoundedRect(durationRect.adjusted(0, 1, 0, -1), 3, 3);
    }
    p->setPen(Qt::white);
    p->drawText(durationRect, Qt::AlignRight | Qt::AlignVCenter, dk ? formatHMS(dur) : "--:--:--");
    int metaY = r.top() + 31;
    int lineH = 18;
    int rightPad = 8;
    int sizeW = p->fontMetrics().horizontalAdvance(
                    gPlaylistSizeDisplayGB ? QStringLiteral("000.000,00 GB") : QStringLiteral("000.000,00 MB"))
        + 6;
    QRect sizeRect(r.right() - rightPad - sizeW, metaY, sizeW, lineH);
    int heightBadgeW = 0;
    QRect badgeRect;
    if (!heightBadge.isEmpty()) {
        heightBadgeW = p->fontMetrics().horizontalAdvance(heightBadge) + 14;
        badgeRect = QRect(sizeRect.left() - heightBadgeW - 8, metaY + 1, heightBadgeW, lineH - 2);
    }
    int codecBadgeW = 0;
    QRect codecBadgeRect;
    if (!codecBadge.isEmpty() && codecBadge != "?") {
        codecBadgeW = p->fontMetrics().horizontalAdvance(codecBadge) + 14;
        int codecRight = heightBadge.isEmpty() ? sizeRect.left() - 8 : badgeRect.left() - 8;
        codecBadgeRect = QRect(codecRight - codecBadgeW, metaY + 1, codecBadgeW, lineH - 2);
    }
    int metaRight = codecBadgeRect.isValid() ? codecBadgeRect.left() - 8
                                             : (heightBadge.isEmpty() ? sizeRect.left() - 8 : badgeRect.left() - 8);
    p->setPen(QColor(102, 204, 255));
    p->drawText(QRect(x, metaY, qMax(10, metaRight - x), lineH), Qt::AlignVCenter,
        p->fontMetrics().elidedText(meta, Qt::ElideRight, qMax(10, metaRight - x)));
    if (codecBadgeRect.isValid()) {
        p->setPen(Qt::NoPen);
        p->setBrush(codecColor);
        p->drawRoundedRect(codecBadgeRect, 3, 3);
        p->setPen(Qt::white);
        p->drawText(codecBadgeRect, Qt::AlignCenter, codecBadge);
    }
    if (!heightBadge.isEmpty()) {
        p->setPen(Qt::NoPen);
        p->setBrush(heightColor);
        p->drawRoundedRect(badgeRect, 3, 3);
        p->setPen(Qt::white);
        p->drawText(badgeRect, Qt::AlignCenter, heightBadge);
    }
    p->setPen(QColor(102, 204, 255));
    p->drawText(sizeRect, Qt::AlignRight | Qt::AlignVCenter, sizeText);
    p->setPen(QColor(130, 130, 130));
    p->drawText(QRect(x, r.top() + 52, r.right() - x - 8, 18), Qt::AlignVCenter,
        p->fontMetrics().elidedText(i.data(PlaylistModel::PathRole).toString(), Qt::ElideMiddle, r.right() - x - 8));
    if(i.data(PlaylistModel::FolderDropGroupRole).toBool()){
        const bool first=i.data(PlaylistModel::FolderDropGroupFirstRole).toBool();
        const bool last=i.data(PlaylistModel::FolderDropGroupLastRole).toBool();
        QRect gr=r.adjusted(2,1,-3,-1);
        QPen pen(QColor(205,150,0),2);
        p->setPen(pen);
        if(first)p->drawLine(gr.topLeft(),gr.topRight());
        if(last)p->drawLine(gr.bottomLeft(),gr.bottomRight());
        p->drawLine(gr.topLeft(),gr.bottomLeft());
        p->drawLine(gr.topRight(),gr.bottomRight());
    }
    if(dk){
        auto sameDurationAt=[&](int row){
            if(!i.model()||row<0||row>=i.model()->rowCount())return false;
            QModelIndex other=i.model()->index(row,0);
            if(!other.data(PlaylistModel::DurationKnownRole).toBool())return false;
            double otherDur=other.data(PlaylistModel::DurationRole).toDouble();
            return qAbs(qRound64(otherDur)-qRound64(dur))==0;
        };
        const bool samePrev=sameDurationAt(i.row()-1);
        const bool sameNext=sameDurationAt(i.row()+1);
        if(samePrev||sameNext){
            QRect linkRect=durationRect.adjusted(-3,0,3,0);
            int yTop=samePrev?r.top():linkRect.top();
            int yBottom=sameNext?r.bottom():linkRect.bottom();
            QPen linkPen(QColor(255,0,0),2);
            linkPen.setCosmetic(true);
            p->setPen(linkPen);
            p->setBrush(Qt::NoBrush);
            p->drawLine(QPoint(linkRect.left(),yTop),QPoint(linkRect.left(),yBottom));
            p->drawLine(QPoint(linkRect.right(),yTop),QPoint(linkRect.right(),yBottom));
            if(!samePrev)p->drawLine(QPoint(linkRect.left(),linkRect.top()),QPoint(linkRect.right(),linkRect.top()));
            if(!sameNext)p->drawLine(QPoint(linkRect.left(),linkRect.bottom()),QPoint(linkRect.right(),linkRect.bottom()));
        }
    }
    p->restore();
}
