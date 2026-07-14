#include "family/FamilyMoveEvaluator.h"
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
#include <QSettings>
#include <QSortFilterProxyModel>


namespace {
QVector<FamilyDestination> gFamilyDestinations;

QColor familyFolderIndicatorColor(FamilyMoveEvaluation::FolderState state)
{
    switch(state){
    case FamilyMoveEvaluation::FolderState::Online:return QColor(24,168,72);
    case FamilyMoveEvaluation::FolderState::Offline:return QColor(230,138,0);
    case FamilyMoveEvaluation::FolderState::Missing:return QColor(210,47,47);
    }
    return QColor(210,47,47);
}

QColor familyMoveIndicatorColor(FamilyMoveEvaluation::MoveState state)
{
    switch(state){
    case FamilyMoveEvaluation::MoveState::Ready:return QColor(24,168,72);
    case FamilyMoveEvaluation::MoveState::Offline:return QColor(230,138,0);
    case FamilyMoveEvaluation::MoveState::Blocked:return QColor(210,47,47);
    }
    return QColor(210,47,47);
}
}

void PlaylistDelegate::setFamilyDestinations(const QVector<FamilyDestination>&destinations)
{
    gFamilyDestinations=destinations;
}

static PlaylistSizeUnit gPlaylistSizeUnit = PlaylistSizeUnit::Megabytes;
static bool gPlaylistSizeUnitLoaded = false;

static PlaylistSizeUnit currentPlaylistSizeUnit()
{
    if (!gPlaylistSizeUnitLoaded) {
        gPlaylistSizeUnit = playlistSizeUnitFromInt(QSettings().value(QStringLiteral("playlist/sizeUnit"), 2).toInt());
        gPlaylistSizeUnitLoaded = true;
    }
    return gPlaylistSizeUnit;
}

static void setCurrentPlaylistSizeUnit(PlaylistSizeUnit unit)
{
    gPlaylistSizeUnit = unit;
    gPlaylistSizeUnitLoaded = true;
    QSettings().setValue(QStringLiteral("playlist/sizeUnit"), playlistSizeUnitToInt(gPlaylistSizeUnit));
}

static void emitAllPlaylistRowsChanged(QAbstractItemModel* model)
{
    if (!model || model->rowCount() <= 0)
        return;

    emit model->dataChanged(model->index(0, 0), model->index(model->rowCount() - 1, 0));

    if (auto* proxy = qobject_cast<QSortFilterProxyModel*>(model)) {
        QAbstractItemModel* source = proxy->sourceModel();
        if (source && source->rowCount() > 0)
            emit source->dataChanged(source->index(0, 0), source->index(source->rowCount() - 1, 0));
    }
}
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
                            playlistSizeWidthSample(currentPlaylistSizeUnit()))
                + 6;
            QRect sizeRect(o.rect.right() - rightPad - sizeW, metaY, sizeW, lineH);
            if (sizeRect.contains(me->position().toPoint())) {
                setCurrentPlaylistSizeUnit(nextPlaylistSizeUnit(currentPlaylistSizeUnit()));
                emitAllPlaylistRowsChanged(model);
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
    bool reviewed = i.data(PlaylistModel::ReviewedRole).toBool();
    p->fillRect(r,
        (o.state & QStyle::State_Selected) ? QColor(38, 82, 48)
                                           : (i.row() % 2 ? QColor(36, 36, 36) : QColor(24, 24, 24)));
    if (reviewed)
        p->fillRect(r, QColor(0, 0, 0, 48));
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
    const FamilyMoveEvaluation familyEvaluation =
        FamilyMoveEvaluator::evaluate(i.data(PlaylistModel::PathRole).toString(), gFamilyDestinations);
    const int familyIndicatorWidth=8;
    const int familyIndicatorGap=2;
    const QRect familyFolderRect(x,r.top()+8,familyIndicatorWidth,18);
    const QRect familyMoveRect(familyFolderRect.right()+familyIndicatorGap+1,r.top()+8,familyIndicatorWidth,18);
    p->setPen(QColor(20,20,20));
    p->setBrush(familyFolderIndicatorColor(familyEvaluation.folderState));
    p->drawRect(familyFolderRect);
    p->setBrush(familyMoveIndicatorColor(familyEvaluation.moveState));
    p->drawRect(familyMoveRect);
    const int titleX=familyMoveRect.right()+7;
    double dur = i.data(PlaylistModel::DurationRole).toDouble();
    bool dk = i.data(PlaylistModel::DurationKnownRole).toBool();
    QString codec = i.data(PlaylistModel::CodecRole).toString();
    QString res = i.data(PlaylistModel::ResolutionRole).toString();
    QString cont = i.data(PlaylistModel::ContainerRole).toString();
    qint64 sizeBytes = i.data(PlaylistModel::SizeRole).toLongLong();
    QString sizeText = formatEuroSize(sizeBytes, currentPlaylistSizeUnit());
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
    QRect titleRect(titleX, r.top() + 7, r.right() - 90 - titleX, 20);
    drawHighlightedTitle(p, titleRect, title);
    const int familyMoveProgress=i.data(PlaylistModel::FamilyMoveProgressRole).toInt();
    if(familyMoveProgress>=0){
        // Empty progress is a dark track. Completed progress grows from left
        // to right in green, rather than covering a green line with black.
        const QRect progressTrack(titleRect.left(),titleRect.bottom()+1,qMax(20,titleRect.width()),5);
        p->setPen(QPen(QColor(8,24,12),1));
        p->setBrush(QColor(1,12,5));
        p->drawRect(progressTrack.adjusted(0,0,-1,-1));
        const int filled=qBound(0,(progressTrack.width()*familyMoveProgress)/100,progressTrack.width());
        if(filled>0){
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(48,150,72));
            p->drawRect(QRect(progressTrack.left(),progressTrack.top(),filled,progressTrack.height()));
        }
    }
    if (reviewed) {
        QString reviewedText = QStringLiteral("reviewed");
        int reviewedW = p->fontMetrics().horizontalAdvance(reviewedText) + 14;
        QRect reviewedBadgeRect(r.right() - 86 - reviewedW - 8, r.top() + 8, reviewedW, 18);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(80, 120, 90));
        p->drawRoundedRect(reviewedBadgeRect, 3, 3);
        p->setPen(Qt::white);
        p->drawText(reviewedBadgeRect, Qt::AlignCenter, reviewedText);
    }
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
                    playlistSizeWidthSample(currentPlaylistSizeUnit()))
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
