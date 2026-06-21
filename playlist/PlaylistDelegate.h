#pragma once
#include <QStyledItemDelegate>
#include <QHash>
#include <QPixmap>

class PlaylistDelegate: public QStyledItemDelegate{
public:
    explicit PlaylistDelegate(QObject*p=nullptr);
    QSize sizeHint(const QStyleOptionViewItem&,const QModelIndex&)const override;
    void paint(QPainter*p,const QStyleOptionViewItem&o,const QModelIndex&i)const override;
    bool editorEvent(QEvent*event,QAbstractItemModel*model,const QStyleOptionViewItem&option,const QModelIndex&index)override;
private:
    mutable QHash<QString,QPixmap>cache;
};
