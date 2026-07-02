#include "PlaylistFilterProxyModel.h"
#include "PlaylistModel.h"
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QStringList>
#include <QRegularExpression>

PlaylistFilterProxyModel::PlaylistFilterProxyModel(QObject*parent):QSortFilterProxyModel(parent){setDynamicSortFilter(true);}
void PlaylistFilterProxyModel::setFilterText(const QString&filter){const QString trimmed=filter.trimmed();if(text==trimmed)return;beginFilterChange();text=trimmed;endFilterChange();}
QString PlaylistFilterProxyModel::filterText()const{return text;}
bool PlaylistFilterProxyModel::filterAcceptsRow(int sourceRow,const QModelIndex&sourceParent)const{
    if(text.isEmpty())return true;
    QModelIndex idx=sourceModel()->index(sourceRow,0,sourceParent);
    const QString reviewedText=idx.data(PlaylistModel::ReviewedRole).toBool()?QStringLiteral("reviewed watched seen"):QString();
    const QString haystack=QStringList{
        idx.data(PlaylistModel::TitleRole).toString(),
        idx.data(PlaylistModel::PathRole).toString(),
        idx.data(PlaylistModel::ResolutionRole).toString(),
        idx.data(PlaylistModel::ContainerRole).toString(),
        idx.data(PlaylistModel::CodecRole).toString(),
        reviewedText
    }.join(QStringLiteral("\n"));
    const QStringList terms=text.split(QRegularExpression(QStringLiteral("\\s+")),Qt::SkipEmptyParts);
    for(QString term:terms){
        const bool exclude=term.startsWith(QLatin1Char('-'))&&term.size()>1;
        if(exclude)term=term.mid(1);
        const bool contains=haystack.contains(term,Qt::CaseInsensitive);
        if(exclude&&contains)return false;
        if(!exclude&&!contains)return false;
    }
    return true;
}
