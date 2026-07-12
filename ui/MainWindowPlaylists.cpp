#include "MainWindow.h"
#include "PlaylistWorkspace.h"
#include "playlist/PlaylistFilterProxyModel.h"
#include "playlist/PlaylistModel.h"
#include <QAbstractItemModel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QTabWidget>

PlaylistWorkspace* MainWindow::createPlaylistWorkspace(const QString& title)
{
    auto* workspace = new PlaylistWorkspace(rightTabs);
    playlistWorkspaces << workspace;
    const int index = rightTabs->addTab(workspace, title);

    workspace->searchEdit->installEventFilter(this);
    workspace->view->viewport()->setAcceptDrops(true);
    workspace->view->viewport()->installEventFilter(this);
    workspace->view->installEventFilter(this);

    connect(workspace->searchEdit, &QLineEdit::textChanged, this,
        [this, workspace](const QString& text) {
            workspace->proxy->setFilterText(text);
            if (playlistModel == workspace->model) {
                updatePlaylistSummary();
                ensureVisiblePlaylistSelection();
            }
        });
    connect(workspace->view, &QListView::doubleClicked, this,
        [this, workspace](const QModelIndex& index) {
            const int tabIndex = playlistWorkspaces.indexOf(workspace);
            if (tabIndex >= 0 && rightTabs->currentIndex() != tabIndex)
                rightTabs->setCurrentIndex(tabIndex);
            QModelIndex source = workspace->proxy->mapToSource(index);
            playPlaylistRow(source.isValid() ? source.row() : -1);
        });
    connect(workspace->view, &QListView::customContextMenuRequested,
        this, &MainWindow::showPlaylistContextMenu);

    auto refresh = [this, workspace] {
        if (playlistModel == workspace->model) {
            updatePlaylistSummary();
            if (!restoringPlaybackState)
                savePlaylistState();
        }
    };
    connect(workspace->model, &QAbstractItemModel::rowsMoved, this, refresh);
    connect(workspace->model, &QAbstractItemModel::rowsInserted, this, refresh);
    connect(workspace->model, &QAbstractItemModel::rowsRemoved, this, refresh);
    connect(workspace->model, &QAbstractItemModel::modelReset, this, refresh);
    connect(workspace->model, &QAbstractItemModel::dataChanged, this, refresh);

    if (playlistWorkspaces.size() == 1)
        activatePlaylistWorkspace(index);
    return workspace;
}

void MainWindow::activatePlaylistWorkspace(int index)
{
    if (!rightTabs || index < 0 || index >= rightTabs->count())
        return;
    auto* workspace = static_cast<PlaylistWorkspace*>(rightTabs->widget(index));
    if (!workspace)
        return;

    playlistModel = workspace->model;
    playlistProxyModel = workspace->proxy;
    playlistSearchEdit = workspace->searchEdit;
    playlistSummaryLabel = workspace->summaryLabel;
    playlistView = workspace->view;

    updatePlaylistSummary();
    ensureVisiblePlaylistSelection();
    if (playlistKeyboardFocus)
        playlistView->setFocus(Qt::TabFocusReason);
    if(!restoringPlaybackState)savePlaylistState();
}

void MainWindow::addPlaylistWorkspace()
{
    const int number = playlistSerial++;
    auto* workspace = createPlaylistWorkspace(QStringLiteral("Playlist %1").arg(number));
    rightTabs->setCurrentWidget(workspace);
    activatePlaylistWorkspace(rightTabs->indexOf(workspace));
    workspace->searchEdit->setFocus(Qt::OtherFocusReason);
    if(!restoringPlaybackState)savePlaylistState();
}

void MainWindow::closePlaylistWorkspace(int index)
{
    if (!rightTabs || index < 0 || index >= rightTabs->count())
        return;

    if (rightTabs->count() == 1) {
        auto* only = static_cast<PlaylistWorkspace*>(rightTabs->widget(0));
        if (only) {
            only->model->clearItems();
            only->searchEdit->clear();
            activatePlaylistWorkspace(0);
        }
        return;
    }

    auto* workspace = static_cast<PlaylistWorkspace*>(rightTabs->widget(index));
    rightTabs->removeTab(index);
    playlistWorkspaces.removeOne(workspace);
    if (workspace)
        workspace->deleteLater();
    activatePlaylistWorkspace(qBound(0, rightTabs->currentIndex(), rightTabs->count() - 1));
    if (!restoringPlaybackState)
        savePlaylistState();
}
