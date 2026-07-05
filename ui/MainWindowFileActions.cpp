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
    bool moveWholeFolder = false;
    QStringList selectedPaths;
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

    auto* panel = new QFrame(&overlay);
    panel->setObjectName(QStringLiteral("folderTrashPanel"));
    panel->setFrameShape(QFrame::StyledPanel);
    panel->setFixedSize(760, 520);
    panel->setStyleSheet(QStringLiteral(
        "#folderTrashPanel {"
        "  background-color: rgb(32, 32, 32);"
        "  border: 1px solid rgb(130, 130, 130);"
        "  border-radius: 8px;"
        "}"
        "#folderTrashPanel QLabel { color: rgb(235, 235, 235); }"
        "#folderTrashPanel QListWidget {"
        "  background-color: rgb(20, 20, 20);"
        "  color: rgb(235, 235, 235);"
        "  border: 1px solid rgb(85, 85, 85);"
        "}"
        "#folderTrashPanel QPushButton { padding: 4px 12px; }"));

    auto centerPanel = [&] {
        overlay.setGeometry(host->rect());
        panel->move((overlay.width() - panel->width()) / 2,
            (overlay.height() - panel->height()) / 2);
    };

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);

    auto* title = new QLabel(QStringLiteral("Move folder files to trash"), panel);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 2.0);
    title->setFont(titleFont);
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

    auto* list = new QListWidget(panel);
    list->setSelectionMode(QAbstractItemView::NoSelection);
    for (const FolderTrashEntry& entry : entries) {
        const QString label = QStringLiteral("%1    %2").arg(entry.relativePath, formatFolderTrashSize(entry.size));
        auto* item = new QListWidgetItem(label, list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        item->setData(Qt::UserRole, entry.absolutePath);
    }
    layout->addWidget(list, 1);

    auto* note = new QLabel(QStringLiteral(
        "All files are checked by default.\n"
        "If all files are checked, the whole folder will be moved to trash.\n"
        "If only some files are checked, only those files will be moved to trash and the folder will remain."), panel);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, panel);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Move checked to trash"));
    layout->addWidget(buttons);

    QEventLoop loop;
    auto acceptChoice = [&] {
        choice.accepted = true;
        int checked = 0;
        for (int row = 0; row < list->count(); ++row) {
            QListWidgetItem* item = list->item(row);
            if (item->checkState() == Qt::Checked) {
                ++checked;
                choice.selectedPaths << item->data(Qt::UserRole).toString();
            }
        }
        choice.moveWholeFolder = checked == list->count();
        loop.quit();
    };
    auto rejectChoice = [&] {
        choice = FolderTrashChoice();
        loop.quit();
    };

    QObject::connect(buttons, &QDialogButtonBox::accepted, &overlay, acceptChoice);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &overlay, rejectChoice);

    centerPanel();
    overlay.show();
    overlay.raise();
    panel->show();
    panel->raise();
    list->setFocus(Qt::OtherFocusReason);
    overlay.grabKeyboard();

    loop.exec();

    overlay.releaseKeyboard();
    overlay.hide();
    return choice;
}

} // namespace

void MainWindow::moveSelectedFileToTrash()
{
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

