#include "PlaylistTabWidget.h"
#include <QColor>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionTab>
#include <QTabBar>
#include <QToolButton>
#include <QTimer>
#include <QShowEvent>
#include <QResizeEvent>
#include <QVariant>

namespace {
class PlaylistTabBar : public QTabBar {
public:
    explicit PlaylistTabBar(QWidget* parent = nullptr)
        : QTabBar(parent)
    {
        setUsesScrollButtons(false);
        setElideMode(Qt::ElideRight);
        setStyleSheet(QStringLiteral(
            "QToolButton#playlistTabScrollLeft, QToolButton#playlistTabScrollRight {"
            " background:#303030; color:#eeeeee; border:1px solid #666;"
            " border-radius:3px; min-width:24px; padding:1px; }"
            "QToolButton#playlistTabScrollLeft:hover, QToolButton#playlistTabScrollRight:hover {"
            " background:#454545; border-color:#999; }"));
    }

protected:
    void showEvent(QShowEvent* event) override
    {
        QTabBar::showEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QTabBar::resizeEvent(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        QTabBar::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        for (int index = 0; index < count(); ++index) {
            if (!tabData(index).toBool())
                continue;

            QStyleOptionTab option;
            initStyleOption(&option, index);
            QRect rect = tabRect(index).adjusted(1, 1, -1, -1);

            painter.save();
            painter.setPen(QPen(QColor(185, 65, 65), 1));
            painter.setBrush(QColor(82, 24, 24));
            painter.drawRoundedRect(rect, 3, 3);

            // Leave room for Qt's tab close button on the right.
            const int closeExtent = style()->pixelMetric(QStyle::PM_TabCloseIndicatorWidth, nullptr, this);
            QRect textRect = rect.adjusted(8, 0, -(closeExtent + 8), 0);
            painter.setPen(QColor(255, 185, 185));
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                             fontMetrics().elidedText(tabText(index), Qt::ElideRight, textRect.width()));
            painter.restore();
        }
    }
private:
    void updateScrollButtons()
    {
        const auto buttons=findChildren<QToolButton*>(QString(),Qt::FindDirectChildrenOnly);
        for(QToolButton*button:buttons){
            const QString name=button->objectName();
            const bool left=name.contains(QStringLiteral("Left"),Qt::CaseInsensitive)
                || name.contains(QStringLiteral("Previous"),Qt::CaseInsensitive);
            const bool right=name.contains(QStringLiteral("Right"),Qt::CaseInsensitive)
                || name.contains(QStringLiteral("Next"),Qt::CaseInsensitive);
            if(!left&&!right)continue;
            button->setObjectName(left?QStringLiteral("playlistTabScrollLeft")
                                      :QStringLiteral("playlistTabScrollRight"));
            // Use explicit text symbols instead of theme-painted arrows. Some
            // KDE styles leave QTabBar's primitive arrows visually empty.
            button->setArrowType(Qt::NoArrow);
            button->setToolButtonStyle(Qt::ToolButtonTextOnly);
            button->setText(left?QString::fromUtf8("◀"):QString::fromUtf8("▶"));
            button->setToolTip(left?QStringLiteral("Show previous playlist tabs")
                                   :QStringLiteral("Show next playlist tabs"));
            button->setAutoRaise(false);
            button->setCursor(Qt::PointingHandCursor);
            button->setMinimumWidth(26);
            button->style()->unpolish(button);
            button->style()->polish(button);
            button->update();
            button->setAccessibleName(left?QStringLiteral("Previous playlist tabs")
                                          :QStringLiteral("Next playlist tabs"));
        }
    }
};
}

PlaylistTabWidget::PlaylistTabWidget(QWidget* parent)
    : QTabWidget(parent)
{
    setTabBar(new PlaylistTabBar(this));
    setDocumentMode(false);
}
