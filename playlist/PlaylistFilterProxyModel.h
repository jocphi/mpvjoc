#pragma once
#include <QSortFilterProxyModel>
#include <QString>

class PlaylistFilterProxyModel: public QSortFilterProxyModel{
public:
    explicit PlaylistFilterProxyModel(QObject*parent=nullptr);
    void setFilterText(const QString&text);
    QString filterText()const;
protected:
    bool filterAcceptsRow(int sourceRow,const QModelIndex&sourceParent)const override;
private:
    QString text;
};
