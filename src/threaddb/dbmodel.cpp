#include "dbmodel.h"

#include <QSqlRecord>

DBModel::DBModel(QObject *parent) : QAbstractTableModel(parent)
{

}

DBModel::~DBModel()
{

}

void DBModel::setQuery(DBQuery &query)
{
	beginResetModel();
	query_ = query;
	endResetModel();
}

int DBModel::rowCount(const QModelIndex &) const
{
	return query_.rowCount();
}

int DBModel::columnCount(const QModelIndex &) const
{
	return query_.columnCount();
}

QVariant DBModel::data(const QModelIndex &item, int role) const
{
	if (role != Qt::DisplayRole || item.row() >= query_.rowCount() || item.column() >= query_.columnCount() || item.row() < 0 || item.column() < 0)
		return QVariant();

	DBModel *m = (DBModel*) this;
	m->query_.seek(item.row());
	return query_.value(item.column());
}

QVariant DBModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(orientation != Qt::Horizontal || role != Qt::DisplayRole || query_.columnCount() <= section || section < 0)
		return QAbstractItemModel::headerData(section, orientation, role);

	return query_.record().fieldName(section);
}
