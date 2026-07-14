#include "MainWindow.h"
#include "mpv/MpvWidget.h"
#include "playlist/PlaylistModel.h"
#include <QCloseEvent>
#include <algorithm>
#include <QAbstractItemView>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QDialog>
#include <QWidget>
#include <QStringList>
#include <QMessageBox>
#include <QDirIterator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QListView>
#include <QMainWindow>
#include <QtGlobal>
#include <QSize>
#include <QPoint>
#include <QTimer>
#include <QScreen>
#include <QGuiApplication>
#include <QEventLoop>
#include <QFrame>
#include <QShortcut>
#include <QKeySequence>
#include <QTableWidget>
#include <QHeaderView>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QSettings>

namespace {
QPoint centeredDialogPos(const QDialog& dialog, const QSize& desiredSize)
{
    QWidget* parent = dialog.parentWidget();
    if (parent) {
        QWidget* top = parent->window();
        const QRect parentFrame = top ? top->frameGeometry() : parent->frameGeometry();
        const QPoint center = parentFrame.center();
        return QPoint(center.x() - desiredSize.width() / 2, center.y() - desiredSize.height() / 2);
    }

    if (QScreen* screen = dialog.screen()) {
        const QRect screenRect = screen->availableGeometry();
        const QPoint center = screenRect.center();
        return QPoint(center.x() - desiredSize.width() / 2, center.y() - desiredSize.height() / 2);
    }

    return dialog.pos();
}

void applyFixedFolderTrashDialogSize(QDialog& dialog)
{
    // KZones can match mpvjoc by window class and try to resize modal child
    // dialogs into the same zone as the main window. Fixed size alone may be
    // applied before KZones touches the window, so re-apply size and global
    // position several times after the dialog is shown.
    const QSize desiredSize(760, 520);

    auto enforceGeometry = [&dialog, desiredSize] {
        dialog.setMinimumSize(desiredSize);
        dialog.setMaximumSize(desiredSize);
        dialog.resize(desiredSize);
        dialog.move(centeredDialogPos(dialog, desiredSize));
    };

    dialog.setModal(true);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setSizeGripEnabled(false);
    dialog.setWindowFlag(Qt::Dialog, true);
    dialog.setWindowFlag(Qt::Tool, true);
    enforceGeometry();

    // KZones may act after map/show. These delayed passes keep the dialog
    // fixed-size and centered over mpvjoc without changing the app window class.
    QTimer::singleShot(0, &dialog, enforceGeometry);
    QTimer::singleShot(50, &dialog, enforceGeometry);
    QTimer::singleShot(150, &dialog, enforceGeometry);
    QTimer::singleShot(350, &dialog, enforceGeometry);
    QTimer::singleShot(800, &dialog, enforceGeometry);
}
}

MainWindow::~MainWindow(){savePlaylistState();}




namespace {
struct FolderTrashEntry {
    QString absolutePath;
    QString relativePath;
    qint64 size = 0;
};

QString formatFolderTrashSize(qint64 bytes)
{
    if (bytes < 0)
        return QStringLiteral("?");

    const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    double value = double(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }

    if (unit == 0)
        return QStringLiteral("%1 B").arg(bytes);

    return QStringLiteral("%1 %2").arg(value, 0, 'f', value >= 100.0 ? 0 : 1).arg(QString::fromLatin1(units[unit]));
}

QVector<FolderTrashEntry> folderEntriesRelativeToRoot(const QString& folderRoot)
{
    QVector<FolderTrashEntry> entries;
    QFileInfo rootInfo(folderRoot);
    if (!rootInfo.exists() || !rootInfo.isDir())
        return entries;

    QDir rootDir(rootInfo.absoluteFilePath());
    QDirIterator it(rootInfo.absoluteFilePath(),
        QDir::Files | QDir::Hidden | QDir::System,
        QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        FolderTrashEntry entry;
        entry.absolutePath = info.absoluteFilePath();
        entry.relativePath = rootDir.relativeFilePath(info.absoluteFilePath());
        entry.size = info.size();
        entries << entry;
    }

    std::sort(entries.begin(), entries.end(), [](const FolderTrashEntry& a, const FolderTrashEntry& b) {
        return QString::compare(a.relativePath, b.relativePath, Qt::CaseInsensitive) < 0;
    });
    return entries;
}

struct FolderTrashChoice {
    bool accepted = false;


class MovableResizableTrashPanel : public QFrame {
public:
    explicit MovableResizableTrashPanel(QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setMouseTracking(true);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && gripRect().contains(event->pos())) {
            resizing = true;
            resizeStartGlobal = event->globalPosition().toPoint();
            resizeStartSize = size();
            event->accept();
            return;
        }

        if (event->button() == Qt::LeftButton && dragRect().contains(event->pos())) {
            moving = true;
            moveStartGlobal = event->globalPosition().toPoint();
            moveStartPos = pos();
            setCursor(Qt::SizeAllCursor);
            event->accept();
            return;
        }

        QFrame::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (resizing) {
            const QPoint delta = event->globalPosition().toPoint() - resizeStartGlobal;
            QSize requested(resizeStartSize.width() + delta.x(), resizeStartSize.height() + delta.y());
            requested = requested.expandedTo(minimumSize());
            if (parentWidget())
                requested = requested.boundedTo(parentWidget()->size() - QSize(24, 24));
            resize(requested);
            event->accept();
            return;
        }

        if (moving) {
            const QPoint delta = event->globalPosition().toPoint() - moveStartGlobal;
            move(boundedPanelPosition(moveStartPos + delta));
            event->accept();
            return;
        }

        if (gripRect().contains(event->pos()))
            setCursor(Qt::SizeFDiagCursor);
        else if (dragRect().contains(event->pos()))
            setCursor(Qt::SizeAllCursor);
        else
            setCursor(Qt::ArrowCursor);

        QFrame::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if ((resizing || moving) && event->button() == Qt::LeftButton) {
            resizing = false;
            moving = false;
            if (gripRect().contains(event->pos()))
                setCursor(Qt::SizeFDiagCursor);
            else if (dragRect().contains(event->pos()))
                setCursor(Qt::SizeAllCursor);
            else
                unsetCursor();
            event->accept();
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        if (!resizing && !moving)
            unsetCursor();
        QFrame::leaveEvent(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        QFrame::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        painter.setPen(QPen(QColor(90, 90, 90), 1));
        painter.drawLine(12, 42, width() - 12, 42);

        painter.setPen(QPen(QColor(150, 150, 150), 1));
        const int right = width() - 10;
        const int bottom = height() - 10;
        painter.drawLine(right - 18, bottom, right, bottom - 18);
        painter.drawLine(right - 12, bottom, right, bottom - 12);
        painter.drawLine(right - 6, bottom, right, bottom - 6);
    }

private:
    QRect gripRect() const
    {
        return QRect(width() - 28, height() - 28, 28, 28);
    }

    QRect dragRect() const
    {
        return QRect(0, 0, width(), 44);
    }

    QPoint boundedPanelPosition(const QPoint& requested) const
    {
        if (!parentWidget())
            return requested;

        const QSize parentSize = parentWidget()->size();
        const int margin = 12;
        const int minX = margin - width();
        const int minY = margin - height();
        const int maxX = parentSize.width() - margin;
        const int maxY = parentSize.height() - margin;
        return QPoint(qBound(minX, requested.x(), maxX), qBound(minY, requested.y(), maxY));
    }

    bool resizing = false;
    bool moving = false;
    QPoint resizeStartGlobal;
    QSize resizeStartSize;
    QPoint moveStartGlobal;
    QPoint moveStartPos;
};

    bool moveWholeFolder = false;
    QStringList selectedPaths;
};


class FolderTrashPanelWidget : public QFrame {
public:
    explicit FolderTrashPanelWidget(QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setMouseTracking(true);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && gripRect().contains(event->pos())) {
            resizing = true;
            resizeStartGlobal = event->globalPosition().toPoint();
            resizeStartSize = size();
            event->accept();
            return;
        }

        if (event->button() == Qt::LeftButton && dragRect().contains(event->pos())) {
            moving = true;
            moveStartGlobal = event->globalPosition().toPoint();
            moveStartPos = pos();
            setCursor(Qt::SizeAllCursor);
            event->accept();
            return;
        }

        QFrame::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (resizing) {
            const QPoint delta = event->globalPosition().toPoint() - resizeStartGlobal;
            QSize requested(resizeStartSize.width() + delta.x(), resizeStartSize.height() + delta.y());
            requested = requested.expandedTo(minimumSize());
            if (parentWidget())
                requested = requested.boundedTo(parentWidget()->size() - QSize(24, 24));
            resize(requested);
            event->accept();
            return;
        }

        if (moving) {
            const QPoint delta = event->globalPosition().toPoint() - moveStartGlobal;
            move(boundedPanelPosition(moveStartPos + delta));
            event->accept();
            return;
        }

        if (gripRect().contains(event->pos()))
            setCursor(Qt::SizeFDiagCursor);
        else if (dragRect().contains(event->pos()))
            setCursor(Qt::SizeAllCursor);
        else
            setCursor(Qt::ArrowCursor);

        QFrame::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if ((resizing || moving) && event->button() == Qt::LeftButton) {
            resizing = false;
            moving = false;
            if (gripRect().contains(event->pos()))
                setCursor(Qt::SizeFDiagCursor);
            else if (dragRect().contains(event->pos()))
                setCursor(Qt::SizeAllCursor);
            else
                unsetCursor();
            event->accept();
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        if (!resizing && !moving)
            unsetCursor();
        QFrame::leaveEvent(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        QFrame::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        painter.setPen(QPen(QColor(90, 90, 90), 1));
        painter.drawLine(12, 42, width() - 12, 42);

        painter.setPen(QPen(QColor(150, 150, 150), 1));
        const int right = width() - 10;
        const int bottom = height() - 10;
        painter.drawLine(right - 18, bottom, right, bottom - 18);
        painter.drawLine(right - 12, bottom, right, bottom - 12);
        painter.drawLine(right - 6, bottom, right, bottom - 6);
    }

private:
    QRect gripRect() const
    {
        return QRect(width() - 28, height() - 28, 28, 28);
    }

    QRect dragRect() const
    {
        return QRect(0, 0, width(), 44);
    }

    QPoint boundedPanelPosition(const QPoint& requested) const
    {
        if (!parentWidget())
            return requested;

        const QSize parentSize = parentWidget()->size();
        const int margin = 12;
        const int minX = margin - width();
        const int minY = margin - height();
        const int maxX = parentSize.width() - margin;
        const int maxY = parentSize.height() - margin;
        return QPoint(qBound(minX, requested.x(), maxX), qBound(minY, requested.y(), maxY));
    }

    bool resizing = false;
    bool moving = false;
    QPoint resizeStartGlobal;
    QSize resizeStartSize;
    QPoint moveStartGlobal;
    QPoint moveStartPos;
};

FolderTrashChoice chooseFolderTrashEntries(QWidget* parent, const QString& folderRoot, const QVector<FolderTrashEntry>& entries)
{
    FolderTrashChoice choice;

    QWidget* host = parent ? parent->window() : nullptr;
    if (!host)
        return choice;

    QWidget overlay(host);
    overlay.setObjectName(QStringLiteral("folderTrashOverlay"));
    overlay.setAttribute(Qt::WA_StyledBackground, true);
    overlay.setFocusPolicy(Qt::StrongFocus);
    overlay.setGeometry(host->rect());
    overlay.setStyleSheet(QStringLiteral(
        "#folderTrashOverlay { background-color: rgba(0, 0, 0, 150); }"));

    auto* panel = new FolderTrashPanelWidget(&overlay);
    panel->setObjectName(QStringLiteral("folderTrashPanel"));
    panel->setFrameShape(QFrame::StyledPanel);
    panel->setMinimumSize(680, 420);
    QSettings trashPanelSettings;
    const QSize defaultTrashPanelSize(820, 560);
    QSize rememberedTrashPanelSize = trashPanelSettings.value(QStringLiteral("folderTrashPanel/size"), defaultTrashPanelSize).toSize();
    rememberedTrashPanelSize = rememberedTrashPanelSize.expandedTo(panel->minimumSize());
    rememberedTrashPanelSize = rememberedTrashPanelSize.boundedTo(host->size() - QSize(24, 24));
    if (!rememberedTrashPanelSize.isValid())
        rememberedTrashPanelSize = defaultTrashPanelSize;
    panel->resize(rememberedTrashPanelSize);
    const bool hasRememberedTrashPanelPos = trashPanelSettings.contains(QStringLiteral("folderTrashPanel/pos"));
    const QPoint rememberedTrashPanelPos = trashPanelSettings.value(QStringLiteral("folderTrashPanel/pos")).toPoint();
    panel->setToolTip(QStringLiteral("Drag the title area to move. Drag the lower-right corner to resize."));
    panel->setStyleSheet(QStringLiteral(
        "#folderTrashPanel {"
        "  background-color: rgb(32, 32, 32);"
        "  border: 1px solid rgb(130, 130, 130);"
        "  border-radius: 8px;"
        "}"
        "#folderTrashPanel QLabel { color: rgb(235, 235, 235); }"
        "#folderTrashPanel QTableWidget {"
        "  background-color: rgb(20, 20, 20);"
        "  color: rgb(235, 235, 235);"
        "  border: 1px solid rgb(85, 85, 85);"
        "  gridline-color: rgb(55, 55, 55);"
        "}"
        "#folderTrashPanel QHeaderView::section {"
        "  background-color: rgb(45, 45, 45);"
        "  color: rgb(235, 235, 235);"
        "  padding: 3px;"
        "  border: 0px;"
        "  border-right: 1px solid rgb(70, 70, 70);"
        "}"
        "#folderTrashPanel QPushButton { padding: 4px 12px; }"));

    auto boundedTrashPanelPos = [&](const QPoint& requested) {
        const int margin = 12;
        const int minX = margin - panel->width();
        const int minY = margin - panel->height();
        const int maxX = overlay.width() - margin;
        const int maxY = overlay.height() - margin;
        return QPoint(qBound(minX, requested.x(), maxX), qBound(minY, requested.y(), maxY));
    };

    auto centerPanel = [&] {
        overlay.setGeometry(host->rect());
        const QPoint centered((overlay.width() - panel->width()) / 2,
            (overlay.height() - panel->height()) / 2);
        panel->move(boundedTrashPanelPos(hasRememberedTrashPanelPos ? rememberedTrashPanelPos : centered));
    };

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);

    auto* title = new QLabel(QStringLiteral("Move folder files to trash"), panel);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 2.0);
    title->setFont(titleFont);
    title->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    layout->addWidget(title);

    auto* explanation = new QLabel(QStringLiteral(
        "This was the last playlist item from the dropped folder.\n"
        "Choose which files in the folder should be moved to trash."), panel);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    auto* folderLabel = new QLabel(QStringLiteral("Folder: %1").arg(QDir::toNativeSeparators(folderRoot)), panel);
    folderLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    folderLabel->setWordWrap(true);
    layout->addWidget(folderLabel);

    auto* table = new QTableWidget(panel);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels(QStringList { QStringLiteral(""), QStringLiteral("Filename"), QStringLiteral("Size") });
    table->setRowCount(entries.size());
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    qint64 allBytes = 0;
    for (int row = 0; row < entries.size(); ++row) {
        const FolderTrashEntry& entry = entries[row];
        allBytes += qMax<qint64>(0, entry.size);

        auto* checkItem = new QTableWidgetItem;
        checkItem->setFlags((checkItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        checkItem->setCheckState(Qt::Checked);
        checkItem->setTextAlignment(Qt::AlignCenter);
        checkItem->setData(Qt::UserRole, entry.absolutePath);
        checkItem->setData(Qt::UserRole + 1, entry.size);
        table->setItem(row, 0, checkItem);

        auto* nameItem = new QTableWidgetItem(entry.relativePath);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setToolTip(entry.relativePath);
        table->setItem(row, 1, nameItem);

        auto* sizeItem = new QTableWidgetItem(formatFolderTrashSize(entry.size));
        sizeItem->setFlags(sizeItem->flags() & ~Qt::ItemIsEditable);
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(row, 2, sizeItem);
    }

    layout->addWidget(table, 1);

    auto* totalLabel = new QLabel(panel);
    totalLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(totalLabel);

    auto updateTotalLabel = [&] {
        qint64 checkedBytes = 0;
        int checkedCount = 0;
        for (int row = 0; row < table->rowCount(); ++row) {
            QTableWidgetItem* item = table->item(row, 0);
            if (item && item->checkState() == Qt::Checked) {
                ++checkedCount;
                checkedBytes += item->data(Qt::UserRole + 1).toLongLong();
            }
        }
        totalLabel->setText(QStringLiteral("Selected: %1/%2 files  •  %3 / %4")
                                .arg(checkedCount)
                                .arg(table->rowCount())
                                .arg(formatFolderTrashSize(checkedBytes), formatFolderTrashSize(allBytes)));
    };

    QObject::connect(table, &QTableWidget::itemChanged, &overlay, [updateTotalLabel](QTableWidgetItem*) {
        updateTotalLabel();
    });
    updateTotalLabel();

    auto* note = new QLabel(QStringLiteral(
        "All files are checked by default.\n"
        "If all files are checked, the whole folder will be moved to trash.\n"
        "If only some files are checked, only those files will be moved to trash and the folder will remain."), panel);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, panel);
    QPushButton* confirmButton = buttons->button(QDialogButtonBox::Ok);
    confirmButton->setText(QStringLiteral("Move checked to trash"));
    confirmButton->setDefault(true);
    confirmButton->setAutoDefault(true);
    layout->addWidget(buttons);

    QEventLoop loop;
    auto acceptChoice = [&] {
        choice.accepted = true;
        int checked = 0;
        for (int row = 0; row < table->rowCount(); ++row) {
            QTableWidgetItem* item = table->item(row, 0);
            if (item && item->checkState() == Qt::Checked) {
                ++checked;
                choice.selectedPaths << item->data(Qt::UserRole).toString();
            }
        }
        choice.moveWholeFolder = checked == table->rowCount();
        loop.quit();
    };
    auto rejectChoice = [&] {
        choice = FolderTrashChoice();
        loop.quit();
    };

    QObject::connect(buttons, &QDialogButtonBox::accepted, &overlay, acceptChoice);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &overlay, rejectChoice);

    auto* returnShortcut = new QShortcut(QKeySequence(Qt::Key_Return), &overlay);
    returnShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(returnShortcut, &QShortcut::activated, &overlay, acceptChoice);

    auto* enterShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), &overlay);
    enterShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(enterShortcut, &QShortcut::activated, &overlay, acceptChoice);

    centerPanel();
    overlay.show();
    overlay.raise();
    panel->show();
    panel->raise();
    table->setFocus(Qt::OtherFocusReason);
    overlay.grabKeyboard();

    loop.exec();

    overlay.releaseKeyboard();
    trashPanelSettings.setValue(QStringLiteral("folderTrashPanel/size"), panel->size());
    trashPanelSettings.setValue(QStringLiteral("folderTrashPanel/pos"), panel->pos());
    overlay.hide();
    return choice;
}


} // namespace

void MainWindow::moveSelectedFileToTrash()
{if(currentPlaylistLocked())return;
    int r = currentRow();
    if (r < 0 || r >= playlistModel->count())
        return;

    const QString p = playlistModel->pathAt(r);
    if (p.isEmpty())
        return;

    const QFileInfo selectedInfo(p);
    const QString selectedAbs = selectedInfo.absoluteFilePath();
    const QString folderRoot = playlistModel->folderDropRootAt(r);
    const bool folderDropLastItem = playlistModel->isLastItemInFolderDropGroup(r);

    const bool wasCurrent = QFileInfo(mpvWidget->currentFilePath()).absoluteFilePath() == selectedAbs;

    bool ok = true;
    bool playbackClosed = false;
    auto closePlaybackIfNeeded = [&] {
        if (!playbackClosed && wasCurrent) {
            closeCurrentFile();
            playbackClosed = true;
        }
    };
    bool handledAsFolder = false;
    bool handledByFolderDialog = false;

    if (folderDropLastItem && !folderRoot.isEmpty()) {
        QFileInfo folderInfo(folderRoot);
        if (folderInfo.exists() && folderInfo.isDir()) {
            QVector<FolderTrashEntry> entries = folderEntriesRelativeToRoot(folderInfo.absoluteFilePath());

            if (entries.size() == 1 && QFileInfo(entries.first().absolutePath).absoluteFilePath() == selectedAbs) {
                closePlaybackIfNeeded();
                ok = QFile::moveToTrash(folderInfo.absoluteFilePath());
                handledAsFolder = true;
            } else if (!entries.isEmpty()) {
                const FolderTrashChoice choice = chooseFolderTrashEntries(this, folderInfo.absoluteFilePath(), entries);
                handledByFolderDialog = true;
                if (!choice.accepted)
                    return;

                if (choice.moveWholeFolder) {
                    closePlaybackIfNeeded();
                    ok = QFile::moveToTrash(folderInfo.absoluteFilePath());
                    handledAsFolder = true;
                } else {
                    ok = true;
                    if (choice.selectedPaths.contains(selectedAbs))
                        closePlaybackIfNeeded();
                    for (const QString& pathToTrash : choice.selectedPaths) {
                        if (QFileInfo::exists(pathToTrash))
                            ok = QFile::moveToTrash(pathToTrash) && ok;
                    }
                }
            }
        }
    }

    if (!handledAsFolder && !handledByFolderDialog) {
        if (QFileInfo::exists(selectedAbs)) {
            closePlaybackIfNeeded();
            ok = QFile::moveToTrash(selectedAbs) && ok;
        }
    }

    const bool selectedNoLongerExists = !QFileInfo::exists(selectedAbs);
    const bool folderNoLongerExists = handledAsFolder && !QFileInfo::exists(folderRoot);

    if (selectedNoLongerExists || folderNoLongerExists) {
        playlistModel->removeRowAt(r);
        if (playlistModel->count() > 0) {
            int next = qBound(0, r, playlistModel->count() - 1);
            setPlaylistCurrentSourceRow(next);
            playPlaylistRow(next);
        } else {
            savePlaylistState();
        }
    }
}




void MainWindow::closeEvent(QCloseEvent*e){savePlaylistState();QMainWindow::closeEvent(e);}

