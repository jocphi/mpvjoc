#include "PlaylistDelegate.h"
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

struct PlaylistKeywordRule {
    QString keyword;
    QColor color;
};
static QString playlistKeywordConfigPath()
{
    QString d = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (d.isEmpty())
        d = QDir::homePath() + "/.config/mpvjoc";
    QDir().mkpath(d);
    return QDir(d).filePath("playlist-keywords.conf");
}
static QColor playlistKeywordColor(QString c)
{
    c = c.trimmed();
    QString lc = c.toLower();
    if (lc == "red")
        return QColor(220, 70, 70);
    if (lc == "green")
        return QColor(70, 180, 95);
    if (lc == "orange")
        return QColor(255, 150, 40);
    if (lc == "blue")
        return QColor(80, 140, 220);
    if (lc == "lila" || lc == "purple")
        return QColor(170, 110, 220);
    QColor qc(c);
    return qc.isValid() ? qc : QColor(220, 70, 70);
}
static QVector<PlaylistKeywordRule> playlistKeywordRules()
{
    static QVector<PlaylistKeywordRule> rules;
    static qint64 lastMtime = -2;
    QString path = playlistKeywordConfigPath();
    QFileInfo info(path);
    if (!info.exists()) {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            out << "# mpvjoc playlist keyword colors\n";
            out << "# One rule per line: keyword=color\n";
            out << "# Colors can be red, green, orange, blue, lila/purple, or #RRGGBB\n";
            out << "# Examples:\n";
            out << "# sample=red\n";
            out << "# complete=green\n";
        }
        info.refresh();
    }
    qint64 mt = info.exists() ? info.lastModified().toMSecsSinceEpoch() : -1;
    if (mt == lastMtime)
        return rules;
    lastMtime = mt;
    rules.clear();
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!f.atEnd()) {
            QString line = QString::fromUtf8(f.readLine()).trimmed();
            if (line.isEmpty() || line.startsWith('#'))
                continue;
            int eq = line.indexOf('=');
            if (eq <= 0)
                continue;
            QString key = line.left(eq).trimmed();
            QString col = line.mid(eq + 1).trimmed();
            if (!key.isEmpty())
                rules.push_back({ key, playlistKeywordColor(col) });
        }
    }
    return rules;
}
static void drawPlaylistHighlightedTitle(QPainter* p, const QRect& rect, const QString& title)
{
    QString text = p->fontMetrics().elidedText(title, Qt::ElideRight, rect.width());
    QVector<QColor> colors(text.size());
    QRegularExpression dateRe(QStringLiteral("(?:^|[^0-9])((?:\\d{4}|\\d{2})[.-]\\d{2}[.-]\\d{2})(?!\\d)"));
    auto it = dateRe.globalMatch(text);
    while (it.hasNext()) {
        auto m = it.next();
        int a = m.capturedStart(1), b = m.capturedLength(1);
        for (int n = a; n < a + b && n < colors.size(); ++n)
            colors[n] = QColor(255, 150, 40);
    }
    for (const auto& r : playlistKeywordRules()) {
        int pos = 0;
        while (!r.keyword.isEmpty() && (pos = text.indexOf(r.keyword, pos, Qt::CaseInsensitive)) >= 0) {
            for (int n = pos; n < pos + r.keyword.size() && n < colors.size(); ++n)
                if (!colors[n].isValid())
                    colors[n] = r.color;
            pos += qMax(1, r.keyword.size());
        }
    }
    p->save();
    p->setClipRect(rect);
    int x = rect.left();
    int baseY = rect.top() + (rect.height() + p->fontMetrics().ascent() - p->fontMetrics().descent()) / 2;
    int pos = 0;
    while (pos < text.size()) {
        QColor c = colors[pos].isValid() ? colors[pos] : QColor(Qt::white);
        int end = pos + 1;
        while (end < text.size() && ((colors[end].isValid() ? colors[end] : QColor(Qt::white)) == c))
            ++end;
        QString part = text.mid(pos, end - pos);
        p->setPen(c);
        p->drawText(x, baseY, part);
        x += p->fontMetrics().horizontalAdvance(part);
        pos = end;
    }
    p->restore();
}

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
    QColor codecColor(72, 72, 72);
    auto codecKey = codec.toUpper();
    if (codecKey == "H.264" || codecKey == "AVC")
        codecColor = QColor(38, 96, 132);
    else if (codecKey == "HEVC" || codecKey == "H.265")
        codecColor = QColor(84, 70, 150);
    else if (codecKey == "AV1")
        codecColor = QColor(142, 64, 112);
    else if (codecKey == "VP9")
        codecColor = QColor(46, 116, 86);
    else if (codecKey == "VP8")
        codecColor = QColor(82, 124, 58);
    else if (codecKey == "MPEG4" || codecKey == "MPEG-4")
        codecColor = QColor(138, 92, 42);
    else if (codecKey == "AAC")
        codecColor = QColor(118, 88, 38);
    else if (codecKey == "MP3")
        codecColor = QColor(116, 64, 38);
    else if (codecKey == "FLAC")
        codecColor = QColor(38, 112, 120);
    else if (codecKey == "OPUS")
        codecColor = QColor(98, 76, 130);
    int vh = 0;
    QStringList rp = res.split('x');
    if (rp.size() == 2)
        vh = rp[1].toInt();
    if (vh > 0) {
        int tier = vh >= 2160 ? 2160 : (vh >= 1440 ? 1440 : (vh >= 1080 ? 1080 : (vh >= 720 ? 720 : 480)));
        heightBadge = QString::number(tier) + "p";
        auto lerp = [](int a, int b, double t) { return int(a + (b - a) * qBound(0.0, t, 1.0) + 0.5); };
        auto mix = [&](QColor a, QColor b, double t) {
            return QColor(lerp(a.red(), b.red(), t), lerp(a.green(), b.green(), t), lerp(a.blue(), b.blue(), t));
        };
        if (vh < 700) {
            double t = (qBound(80, vh, 699) - 80) / double(699 - 80);
            double peak = (480 - 80) / double(699 - 80);
            heightColor = t <= peak ? mix(QColor(70, 0, 0), QColor(255, 0, 0), t / peak)
                                    : mix(QColor(255, 0, 0), QColor(255, 150, 150), (t - peak) / (1.0 - peak));
        } else if (vh < 1000) {
            double t = (vh - 700) / double(999 - 700);
            double peak = (720 - 700) / double(999 - 700);
            heightColor = t <= peak ? mix(QColor(0, 70, 0), QColor(0, 255, 0), t / peak)
                                    : mix(QColor(0, 255, 0), QColor(170, 255, 170), (t - peak) / (1.0 - peak));
        } else if (vh < 2000) {
            double t = (vh - 1000) / double(1999 - 1000);
            double peak = (1080 - 1000) / double(1999 - 1000);
            heightColor = t <= peak ? mix(QColor(0, 30, 90), QColor(0, 170, 255), t / peak)
                                    : mix(QColor(0, 170, 255), QColor(170, 220, 255), (t - peak) / (1.0 - peak));
        } else {
            int capped = qMin(vh, 4320);
            double t = (capped - 2000) / double(4320 - 2000);
            double peak = (2160 - 2000) / double(4320 - 2000);
            heightColor = t <= peak ? mix(QColor(65, 0, 90), QColor(210, 90, 255), t / peak)
                                    : mix(QColor(210, 90, 255), QColor(235, 190, 255), (t - peak) / (1.0 - peak));
        }
    }
    QRect titleRect(x, r.top() + 7, r.width() - x + r.left() - 90, 20);
    drawPlaylistHighlightedTitle(p, titleRect, title);
    QRect durationRect(r.right() - 86, r.top() + 7, 78, 20);
    auto roundedDuration
        = [&](const QModelIndex& idx) { return int(idx.data(PlaylistModel::DurationRole).toDouble() + 0.5); };
    int thisDuration = dk ? int(dur + 0.5) : -1;
    int runStart = i.row(), runEnd = i.row();
    if (dk && thisDuration >= 0) {
        while (runStart > 0) {
            auto prevIdx = i.sibling(runStart - 1, 0);
            if (!prevIdx.isValid() || !prevIdx.data(PlaylistModel::DurationKnownRole).toBool()
                || roundedDuration(prevIdx) != thisDuration)
                break;
            --runStart;
        }
        while (true) {
            auto nextIdx = i.sibling(runEnd + 1, 0);
            if (!nextIdx.isValid() || !nextIdx.data(PlaylistModel::DurationKnownRole).toBool()
                || roundedDuration(nextIdx) != thisDuration)
                break;
            ++runEnd;
        }
    }
    int runLength = runEnd - runStart + 1;
    if (dk && runLength >= 2) {
        int duplicateRunIndex = 0;
        int scan = 0;
        while (scan < runStart) {
            auto scanIdx = i.sibling(scan, 0);
            if (!scanIdx.isValid()) {
                ++scan;
                continue;
            }
            bool scanKnown = scanIdx.data(PlaylistModel::DurationKnownRole).toBool();
            int scanDur = scanKnown ? roundedDuration(scanIdx) : -1;
            int scanEnd = scan;
            if (scanKnown) {
                while (true) {
                    auto nextIdx = i.sibling(scanEnd + 1, 0);
                    if (!nextIdx.isValid() || !nextIdx.data(PlaylistModel::DurationKnownRole).toBool()
                        || roundedDuration(nextIdx) != scanDur)
                        break;
                    ++scanEnd;
                }
            }
            if (scanKnown && scanEnd > scan)
                ++duplicateRunIndex;
            scan = scanEnd + 1;
        }
        p->setPen(Qt::NoPen);
        p->setBrush((duplicateRunIndex % 2) == 0 ? QColor(18, 45, 82) : QColor(58, 42, 86));
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
    p->restore();
}
