#include "dbmanager.h"

#include <QMutexLocker>
#include <QSqlError>
#include <QSqlQuery>
#include <QPointer>

DBManager::DBManager()
{

}

DBManager::~DBManager()
{
	if(db_.isOpen())
		db_.close();

	QSqlDatabase::removeDatabase(db_.connectionName());
}

void DBManager::openSQLITE(const QString &filename)
{
	QByteArray uniqIdBytes;
	DBManager *thisPtr = this;
	uniqIdBytes.append( (const char*) &thisPtr, sizeof(thisPtr) );
	QString uniqId = QString::fromLatin1(uniqIdBytes.toHex());

	jobThread_.executeNonblocking([=] {
		db_ = QSqlDatabase::addDatabase("QSQLITE", uniqId);
		db_.setDatabaseName(filename);

		if( !db_.open() )
			emit sigOpenError(db_.lastError().text());
		else
			db_.exec("PRAGMA journal_mode = TRUNCATE");
	});
}

void DBManager::execAssoc(QString query, const AssocArgs &args)
{
	jobThread_.executeNonblocking([=] {
		QSqlQuery q(db_);

		q.prepare(query);
		for(AssocArg arg : args)
			q.bindValue(arg.first, arg.second);

		if( !q.exec() )
			emit sigQueryError(queryDesc(q, args), q.lastError().text());
	});
}

void DBManager::exec(QString query, const DBManager::Args &args)
{
	jobThread_.executeNonblocking([=] {
		QSqlQuery q(db_);

		q.prepare(query);
		for(int i = 0; i < args.length(); i ++)
			q.bindValue(i, args[i]);

		if( !q.exec() )
			emit sigQueryError(queryDesc(q, args), q.lastError().text());
	});
}

void DBManager::blockingExecAssoc(const QString &query, const DBManager::AssocArgs &args)
{
	jobThread_.executeBlocking([&] {
		QSqlQuery q(db_);

		q.prepare(query);
		for(AssocArg arg : args)
			q.bindValue(arg.first, arg.second);

		if( !q.exec() )
			emit sigQueryError(queryDesc(q, args), q.lastError().text());
	});
}

void DBManager::blockingExec(const QString &query, const DBManager::Args &args)
{
	jobThread_.executeBlocking([&] {
		QSqlQuery q(db_);

		q.prepare(query);
		for(int i = 0; i < args.length(); i ++)
			q.bindValue(i, args[i]);

		if( !q.exec() )
			emit sigQueryError(queryDesc(q, args), q.lastError().text());
	});
}

QVariant DBManager::insertAssoc(const QString &query, const DBManager::AssocArgs &args)
{
	QVariant result;
	blockingExecAssoc(query, args, [&](QSqlQuery &q){ result = q.lastInsertId(); });
	return result;
}

QVariant DBManager::insert(const QString &query, const DBManager::Args &args)
{
	QVariant result;
	blockingExec(query, args, [&](QSqlQuery &q){ result = q.lastInsertId(); });
	return result;
}

QSqlRecord DBManager::selectRowAssoc(const QString &query, const DBManager::AssocArgs &args)
{
	QSqlRecord result;
	blockingExecAssoc(query, args, [&](QSqlQuery &q){
		if(!q.next())
			emit sigQueryError(queryDesc(q, args), "No rows returned (selectRow)");
		else
			result = q.record();
	});
	return result;
}

QSqlRecord DBManager::selectRow(const QString &query, const DBManager::Args &args)
{
	QSqlRecord result;
	blockingExec(query, args, [&](QSqlQuery &q){
		if(!q.next())
			emit sigQueryError(queryDesc(q, args), "No rows returned (selectRow)");
		else
			result = q.record();
	});
	return result;
}

QSqlRecord DBManager::selectRowDefAssoc(const QString &query, const DBManager::AssocArgs &args, QSqlRecord def)
{
	QSqlRecord result;
	blockingExecAssoc(query, args, [&](QSqlQuery &q){
		if(!q.next())
			result = def;
		else
			result = q.record();
	});
	return result;
}

QSqlRecord DBManager::selectRowDef(const QString &query, const DBManager::Args &args, QSqlRecord def)
{
	QSqlRecord result;
	blockingExec(query, args, [&](QSqlQuery &q){
		if(!q.next())
			result = def;
		else
			result = q.record();
	});
	return result;
}

QVariant DBManager::selectValueAssoc(const QString &query, const DBManager::AssocArgs &args)
{
	QVariant result;
	blockingExecAssoc(query, args, [&](QSqlQuery &q){
		if(!q.next())
			emit sigQueryError(queryDesc(q, args), "No rows returned (selectValue)");
		else
			result = q.value(0);
	});
	return result;
}

QVariant DBManager::selectValue(const QString &query, const DBManager::Args &args)
{
	QVariant result;
	blockingExec(query, args, [&](QSqlQuery &q){
		if(!q.next())
			emit sigQueryError(queryDesc(q, args), "No rows returned (selectValue)");
		else
			result = q.value(0);
	});
	return result;
}

DBQuery DBManager::selectQueryAssoc(const QString &query, const DBManager::AssocArgs &args)
{
	DBQuery result;

	jobThread_.executeBlocking([&] {

		QPointer<DBManager> thisPtr(this);
		auto deleter = [thisPtr](QSqlQuery *q){
			if(thisPtr.isNull())
				delete q;
			else
				thisPtr->jobThread_.executeNonblocking([=]{
					delete q;
				});
		};

		QSharedPointer<QSqlQuery> q(new QSqlQuery(db_), deleter);

		q->prepare(query);
		for(AssocArg arg : args)
			q->bindValue(arg.first, arg.second);

		if( !q->exec() )
			emit sigQueryError(query, q->lastError().text());

		result = DBQuery(this, q);
	});
	return result;
}

DBQuery DBManager::selectQuery(const QString &query, const DBManager::Args &args)
{
	DBQuery result;
	QPointer<DBManager> thisPtr(this);

	jobThread_.executeBlocking([&] {
		auto deleter = [=](QSqlQuery *q){
			if(thisPtr.isNull())
				delete q;
			else
				jobThread_.executeNonblocking([=]{
					delete q;
				});
		};
		QSharedPointer<QSqlQuery> q(new QSqlQuery(db_), deleter);

		q->prepare(query);
		for(int i = 0; i < args.length(); i ++)
			q->bindValue(i, args[i]);

		if( !q->exec() )
			emit sigQueryError(query, q->lastError().text());

		result = DBQuery(this, q);
	});
	return result;
}

void DBManager::customQueryOperation(const DBManager::QueryOpFunc &opFunc)
{
	jobThread_.executeBlocking([&] {
		opFunc(db_);
	});
}

void DBManager::waitJobDone()
{
	jobThread_.executeBlocking([]{});
}

void DBManager::blockingExecAssoc(const QString &query, const DBManager::AssocArgs &args, DBManager::ManipFunc manF)
{
	jobThread_.executeBlocking([&] {
		QSqlQuery q(db_);

		q.prepare(query);
		for(AssocArg arg : args)
			q.bindValue(arg.first, arg.second);

		if( !q.exec() )
			emit sigQueryError(queryDesc(q, args), q.lastError().text());
		else
			manF(q);
	});
}

void DBManager::blockingExec(const QString &query, const DBManager::Args &args, DBManager::ManipFunc manF)
{
	jobThread_.executeBlocking([&] {
		QSqlQuery q(db_);

		q.prepare(query);
		for(int i = 0; i < args.length(); i ++)
			q.bindValue(i, args[i]);

		if( !q.exec() )
			emit sigQueryError(queryDesc(q, args), q.lastError().text());
		else
			manF(q);
	});
}


QString DBManager::queryDesc(const QSqlQuery &q, const Args &args)
{
	QString result = q.lastQuery() + " [";
	for(int i = 0; i < args.size(); i ++) {
		if(i)
			result += ", ";

		result += args[i].toString();
	}
	result += "]";
	return result;
}

QString DBManager::queryDesc(const QSqlQuery &q, const AssocArgs &args)
{
	QString result = q.lastQuery() + " {";
	for(int i = 0; i < args.size(); i ++) {
		if(i)
			result += ", ";

		result += args[i].first + "= " + args[i].second.toString();
	}
	result += "}";
	return result;
}
