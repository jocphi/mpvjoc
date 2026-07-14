#pragma once
#include <QWidget>

class PlaylistModel;
class PlaylistFilterProxyModel;
class QLineEdit;
class QLabel;
class QListView;

class PlaylistWorkspace : public QWidget {
public:
    explicit PlaylistWorkspace(QWidget* parent = nullptr);

    PlaylistModel* model = nullptr;
    PlaylistFilterProxyModel* proxy = nullptr;
    QLineEdit* searchEdit = nullptr;
    QLabel* summaryLabel = nullptr;
    QListView* view = nullptr;
    bool locked = false;
};
