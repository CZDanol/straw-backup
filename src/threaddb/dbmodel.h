#ifndef DBMODEL_H
#define DBMODEL_H

#include <QAbstractTableModel>

#include "dbquery.h"

class DBModel : public QAbstractTableModel
{
	Q_OBJECT

public:
		explicit DBModel(QObject *parent = 0);
		virtual ~DBModel();

		void setQuery(DBQuery &query);

		int rowCount(const QModelIndex &parent = QModelIndex()) const;
		int columnCount(const QModelIndex &parent = QModelIndex()) const;

		QVariant data(const QModelIndex &item, int role = Qt::DisplayRole) const;
		QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

private:
		DBQuery query_;

};

#endif // DBMODEL_H
