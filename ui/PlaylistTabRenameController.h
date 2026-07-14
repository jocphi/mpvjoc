#pragma once
#include <QObject>
#include <QPoint>
#include <functional>

class QEvent;
class QLineEdit;
class QTabBar;
class QTimer;

class PlaylistTabRenameController : public QObject {
public:
    explicit PlaylistTabRenameController(QTabBar* tabBar,
                                         std::function<void()> nameChanged,
                                         QObject* parent = nullptr);
    void editTab(int index);

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    void beginEditing(int index);
    void finishEditing(bool accept);
    void cancelPendingEdit();
    void updateEditorGeometry();

    QTabBar* bar = nullptr;
    QTimer* holdTimer = nullptr;
    QLineEdit* editor = nullptr;
    std::function<void()> changedCallback;
    QPoint pressPosition;
    int pressedIndex = -1;
    int editingIndex = -1;
    QString originalText;
    bool finishing = false;
};
