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

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Move folder files to trash"));
    dialog.setModal(true);

    auto* layout = new QVBoxLayout(&dialog);
    auto* explanation = new QLabel(QStringLiteral(
        "This was the last playlist item from the dropped folder.\n"
        "Choose which files in the folder should be moved to trash."), &dialog);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    auto* folderLabel = new QLabel(QStringLiteral("Folder: %1").arg(QDir::toNativeSeparators(folderRoot)), &dialog);
    folderLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    folderLabel->setWordWrap(true);
    layout->addWidget(folderLabel);

    auto* list = new QListWidget(&dialog);
    list->setMinimumSize(680, 320);
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
        "If only some files are checked, only those files will be moved to trash and the folder will remain."), &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Move checked to trash"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return choice;

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
    if (wasCurrent)
        closeCurrentFile();

    bool ok = true;
    bool handledAsFolder = false;
    bool handledByFolderDialog = false;

    if (folderDropLastItem && !folderRoot.isEmpty()) {
        QFileInfo folderInfo(folderRoot);
        if (folderInfo.exists() && folderInfo.isDir()) {
            QVector<FolderTrashEntry> entries = folderEntriesRelativeToRoot(folderInfo.absoluteFilePath());

            if (entries.size() == 1 && QFileInfo(entries.first().absolutePath).absoluteFilePath() == selectedAbs) {
                ok = QFile::moveToTrash(folderInfo.absoluteFilePath());
                handledAsFolder = true;
            } else if (!entries.isEmpty()) {
                const FolderTrashChoice choice = chooseFolderTrashEntries(this, folderInfo.absoluteFilePath(), entries);
                if (choice.accepted) {
                    handledByFolderDialog = true;
                    if (choice.moveWholeFolder) {
                        ok = QFile::moveToTrash(folderInfo.absoluteFilePath());
                        handledAsFolder = true;
                    } else {
                        ok = true;
                        for (const QString& pathToTrash : choice.selectedPaths) {
                            if (QFileInfo::exists(pathToTrash))
                                ok = QFile::moveToTrash(pathToTrash) && ok;
                        }
                    }
                }
            }
        }
    }

    if (!handledAsFolder && !handledByFolderDialog) {
        if (QFileInfo::exists(selectedAbs))
            ok = QFile::moveToTrash(selectedAbs) && ok;
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

