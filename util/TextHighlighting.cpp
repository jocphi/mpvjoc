#include "TextHighlighting.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QtGlobal>

struct KeywordHighlightRule {
    QString keyword;
    QColor color;
};

static QString keywordConfigPath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath() + "/.config/mpvjoc";
    }
    QDir().mkpath(dir);
    return QDir(dir).filePath("playlist-keywords.conf");
}

static QColor keywordColor(QString colorName)
{
    colorName = colorName.trimmed();
    const QString lower = colorName.toLower();
    if (lower == "red") return QColor(220, 70, 70);
    if (lower == "green") return QColor(70, 180, 95);
    if (lower == "orange") return QColor(255, 150, 40);
    if (lower == "blue") return QColor(80, 140, 220);
    if (lower == "lila" || lower == "purple") return QColor(170, 110, 220);
    const QColor parsed(colorName);
    return parsed.isValid() ? parsed : QColor(220, 70, 70);
}

static QVector<KeywordHighlightRule> keywordRules()
{
    static QVector<KeywordHighlightRule> rules;
    static qint64 lastMtime = -2;
    const QString path = keywordConfigPath();
    QFileInfo info(path);

    if (!info.exists()) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "# mpvjoc playlist keyword colors\n";
            out << "# One rule per line: keyword=color\n";
            out << "# Colors can be red, green, orange, blue, lila/purple, or #RRGGBB\n";
            out << "# Examples:\n";
            out << "# sample=red\n";
            out << "# complete=green\n";
        }
        info.refresh();
    }

    const qint64 mtime = info.exists() ? info.lastModified().toMSecsSinceEpoch() : -1;
    if (mtime == lastMtime) return rules;
    lastMtime = mtime;
    rules.clear();

    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!file.atEnd()) {
            const QString line = QString::fromUtf8(file.readLine()).trimmed();
            if (line.isEmpty() || line.startsWith('#')) continue;
            const int equalsPos = line.indexOf('=');
            if (equalsPos <= 0) continue;
            const QString keyword = line.left(equalsPos).trimmed();
            const QString color = line.mid(equalsPos + 1).trimmed();
            if (!keyword.isEmpty()) rules.push_back({ keyword, keywordColor(color) });
        }
    }
    return rules;
}

void drawHighlightedTitle(QPainter *painter, const QRect &rect, const QString &title, const QColor &defaultColor)
{
    const QString text = painter->fontMetrics().elidedText(title, Qt::ElideRight, rect.width());
    QVector<QColor> colors(text.size());
    const QRegularExpression dateRe(QStringLiteral("(?:^|[^0-9])((?:\\d{4}|\\d{2})[.-]\\d{2}[.-]\\d{2})(?!\\d)"));

    auto it = dateRe.globalMatch(text);
    while (it.hasNext()) {
        const auto match = it.next();
        const int start = match.capturedStart(1);
        const int length = match.capturedLength(1);
        for (int n = start; n < start + length && n < colors.size(); ++n) colors[n] = QColor(255, 150, 40);
    }

    for (const auto &rule : keywordRules()) {
        int pos = 0;
        while (!rule.keyword.isEmpty() && (pos = text.indexOf(rule.keyword, pos, Qt::CaseInsensitive)) >= 0) {
            for (int n = pos; n < pos + rule.keyword.size() && n < colors.size(); ++n) {
                if (!colors[n].isValid()) colors[n] = rule.color;
            }
            pos += qMax(1, rule.keyword.size());
        }
    }

    painter->save();
    painter->setClipRect(rect);
    int x = rect.left();
    const int baseY = rect.top() + (rect.height() + painter->fontMetrics().ascent() - painter->fontMetrics().descent()) / 2;
    int pos = 0;
    while (pos < text.size()) {
        const QColor color = colors[pos].isValid() ? colors[pos] : defaultColor;
        int end = pos + 1;
        while (end < text.size() && ((colors[end].isValid() ? colors[end] : defaultColor) == color)) ++end;
        const QString part = text.mid(pos, end - pos);
        painter->setPen(color);
        painter->drawText(x, baseY, part);
        x += painter->fontMetrics().horizontalAdvance(part);
        pos = end;
    }
    painter->restore();
}
