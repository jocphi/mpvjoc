#include "PlaylistWorkspace.h"
#include "playlist/PlaylistDelegate.h"
#include "playlist/PlaylistFilterProxyModel.h"
#include "playlist/PlaylistModel.h"
#include <QAbstractItemView>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QVBoxLayout>

PlaylistWorkspace::PlaylistWorkspace(QWidget* parent)
    : QWidget(parent)
{
    model = new PlaylistModel(this);
    proxy = new PlaylistFilterProxyModel(this);
    proxy->setSourceModel(model);

    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText(QStringLiteral("Search playlist: words, -exclude"));
    searchEdit->setToolTip(QStringLiteral(
        "Search supports multiple words as AND terms. Prefix a term with - to exclude it. "
        "Example: 1080 mkv -reviewed"));
    searchEdit->setClearButtonEnabled(true);

    summaryLabel = new QLabel(QStringLiteral("0 files  •  0,00 MB  •  00:00:00"), this);
    summaryLabel->setContentsMargins(4, 1, 4, 1);
    summaryLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    view = new QListView(this);
    view->setModel(proxy);
    view->setItemDelegate(new PlaylistDelegate(view));
    view->setUniformItemSizes(true);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    view->setDragEnabled(true);
    view->setAcceptDrops(true);
    view->setDropIndicatorShown(true);
    view->setDragDropMode(QAbstractItemView::InternalMove);
    view->setDefaultDropAction(Qt::MoveAction);
    view->setDragDropOverwriteMode(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(1);
    layout->addWidget(searchEdit);
    layout->addWidget(summaryLabel);
    layout->addWidget(view, 1);
}
