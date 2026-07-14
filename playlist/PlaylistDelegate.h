#include "family/FamilyDestinationRepository.h"
#pragma once
#include <QStyledItemDelegate>
#include <QHash>
#include <QPixmap>

class PlaylistDelegate: public QStyledItemDelegate{
public:
    static void setFamilyDestinations(const QVector<FamilyDestination>&destinations);
    explicit PlaylistDelegate(QObject*p=nullptr);
    QSize sizeHint(const QStyleOptionViewItem&,const QModelIndex&)const override;
    void paint(QPainter*p,const QStyleOptionViewItem&o,const QModelIndex&i)const override;
    bool editorEvent(QEvent*event,QAbstractItemModel*model,const QStyleOptionViewItem&option,const QModelIndex&index)override;
private:
    mutable QHash<QString,QPixmap>cache;
};
