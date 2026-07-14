#include "PlaylistTabRenameController.h"
#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QTabBar>
#include <QTimer>
#include <QtGlobal>

PlaylistTabRenameController::PlaylistTabRenameController(
    QTabBar* tabBar, std::function<void()> nameChanged, QObject* parent)
    : QObject(parent), bar(tabBar), changedCallback(std::move(nameChanged))
{
    holdTimer = new QTimer(this);
    holdTimer->setSingleShot(true);
    holdTimer->setInterval(650);
    connect(holdTimer, &QTimer::timeout, this, [this] {
        if (pressedIndex >= 0)
            beginEditing(pressedIndex);
    });
    if (bar) {
        bar->installEventFilter(this);
        bar->setMouseTracking(true);
    }
}

void PlaylistTabRenameController::editTab(int index)
{
    beginEditing(index);
}

void PlaylistTabRenameController::cancelPendingEdit()
{
    pressedIndex = -1;
    if (holdTimer)
        holdTimer->stop();
}

void PlaylistTabRenameController::updateEditorGeometry()
{
    if (!bar || !editor || editingIndex < 0 || editingIndex >= bar->count())
        return;
    editor->setGeometry(bar->tabRect(editingIndex).adjusted(3, 2, -3, -2));
    editor->raise();
}

void PlaylistTabRenameController::beginEditing(int index)
{
    cancelPendingEdit();
    if (!bar || index < 0 || index >= bar->count() || editor)
        return;

    editingIndex = index;
    originalText = bar->tabText(index);
    editor = new QLineEdit(originalText, bar);
    editor->setObjectName(QStringLiteral("playlistTabNameEditor"));
    editor->setAlignment(Qt::AlignCenter);
    editor->setFrame(true);
    editor->setStyleSheet(QStringLiteral(
        "QLineEdit#playlistTabNameEditor {"
        " background: #202020; color: #ffffff; border: 1px solid #66ccff;"
        " padding: 0px 4px; selection-background-color: #356a84; }"));
    editor->installEventFilter(this);
    connect(editor, &QLineEdit::editingFinished, this, [this] {
        if (!finishing)
            finishEditing(true);
    });
    updateEditorGeometry();
    editor->show();
    editor->setFocus(Qt::MouseFocusReason);
    editor->selectAll();
}

void PlaylistTabRenameController::finishEditing(bool accept)
{
    if (!editor || finishing)
        return;
    finishing = true;

    QString newText = editor->text().trimmed();
    if (!accept || newText.isEmpty())
        newText = originalText;

    const bool changed = accept && !newText.isEmpty() && newText != originalText;
    if (bar && editingIndex >= 0 && editingIndex < bar->count())
        bar->setTabText(editingIndex, newText);

    QLineEdit* oldEditor = editor;
    editor = nullptr;
    editingIndex = -1;
    originalText.clear();
    oldEditor->removeEventFilter(this);
    oldEditor->deleteLater();
    finishing = false;

    if (changed && changedCallback)
        changedCallback();
}

bool PlaylistTabRenameController::eventFilter(QObject* object, QEvent* event)
{
    if (object == bar) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton) {
                pressPosition = mouse->position().toPoint();
                pressedIndex = bar->tabAt(pressPosition);
                if (pressedIndex >= 0)
                    holdTimer->start();
            }
            break;
        }
        case QEvent::MouseMove: {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (pressedIndex >= 0
                && (mouse->position().toPoint() - pressPosition).manhattanLength()
                    >= QApplication::startDragDistance())
                cancelPendingEdit();
            break;
        }
        case QEvent::MouseButtonRelease:
        case QEvent::Leave:
            cancelPendingEdit();
            break;
        case QEvent::Resize:
        case QEvent::Move:
        case QEvent::LayoutRequest:
            updateEditorGeometry();
            break;
        default:
            break;
        }
    } else if (object == editor && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape) {
            finishEditing(false);
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            finishEditing(true);
            return true;
        }
    }
    return QObject::eventFilter(object, event);
}
