#include "dbquery.h"

#include <QSqlError>

#include "dbmanager.h"

DBQuery::DBQuery()
{

}

DBQuery::DBQuery(DBManager *manager)
{
	manager_ = manager;
	manager_->jobThread_.executeBlocking([this]{
		query_.reset( new QSqlQuery(manager_->db_) );
	});
}

DBQuery::DBQuery(DBManager *manager, QSharedPointer<QSqlQuery> query)
{
	query_ = query;
	manager_ = manager;

	rec_ = query_->record();

	query_->last();
	rowCount_ = qMax(0, query_->at() + 1);
	query_->seek(-1);
}

void DBQuery::prepare(const QString &query)
{
	manager_->jobThread_.executeNonblocking([this,query]{
		query_->prepare(query);
	});
}

void DBQuery::execAssoc(const DBQuery::AssocArgs &args)
{
	manager_->jobThread_.executeBlocking([this,args]{
		for(AssocArg arg : args)
			query_->bindValue(arg.first, arg.second);

		if( !query_->exec() )
			emit manager_->sigQueryError(manager_->queryDesc(*query_, args), query_->lastError().text());

		if( query_->isSelect() ) {
			query_->last();
			rowCount_ = qMax(0, query_->at() + 1);
			query_->seek(-1);

		} else
			rowCount_ = -1;
	});
}

void DBQuery::exec(const DBQuery::Args &args)
{
	manager_->jobThread_.executeBlocking([this,args]{
		for(int i = 0; i < args.length(); i ++)
			query_->bindValue(i, args[i]);

		if( !query_->exec() )
			emit manager_->sigQueryError(manager_->queryDesc(*query_, args), query_->lastError().text());

		if( query_->isSelect() ) {
			query_->last();
			rowCount_ = qMax(0, query_->at() + 1);
			query_->seek(-1);

		} else
			rowCount_ = -1;
	});
}

void DBQuery::execAssocAsync(const DBQuery::AssocArgs &args)
{
	manager_->jobThread_.executeNonblocking([this,args]{
		for(AssocArg arg : args)
			query_->bindValue(arg.first, arg.second);

		if( !query_->exec() )
			emit manager_->sigQueryError(manager_->queryDesc(*query_, args), query_->lastError().text());
	});
}

void DBQuery::execAsync(const DBQuery::Args &args)
{
	manager_->jobThread_.executeNonblocking([this,args]{
		for(int i = 0; i < args.length(); i ++)
			query_->bindValue(i, args[i]);

		if( !query_->exec() )
			emit manager_->sigQueryError(manager_->queryDesc(*query_, args), query_->lastError().text());
	});
}

bool DBQuery::isValid() const
{
	return !query_.isNull();
}

bool DBQuery::next()
{
	bool result;
	manager_->jobThread_.executeBlocking([&]{
		result = query_->next();
		rec_ = query_->record();
	});
	return result;
}

bool DBQuery::seek(int pos)
{
	bool result;
	manager_->jobThread_.executeBlocking([&]{
		result = query_->seek(pos);
		rec_ = query_->record();
	});
	return result;
}

QVariant DBQuery::value(int i) const
{
	return rec_.value(i);
}

QVariant DBQuery::value(const QString &fieldName) const
{
	return rec_.value(fieldName);
}

const QSqlRecord &DBQuery::record() const
{
	return rec_;
}

int DBQuery::rowCount() const
{
	return rowCount_;
}

int DBQuery::columnCount() const
{
	return rec_.count();
}
