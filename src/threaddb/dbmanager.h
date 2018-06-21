#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QVector>
#include <QPair>
#include <QVariant>
#include <QSqlDatabase>
#include <QSqlRecord>

#include "job/jobthread.h"
#include "dbquery.h"

class DBManager : public QObject
{
	Q_OBJECT

public:
	friend class DBQuery;

	using Arg = QVariant;
	using Args = QVector<QVariant>;

	using AssocArg = QPair<QString,QVariant>;
	using AssocArgs = QVector<AssocArg>;

	using ManipFunc = std::function<void(QSqlQuery&)>;
	using QueryOpFunc = std::function<void(QSqlDatabase&)>;

public:
	DBManager();
	~DBManager();

public:
	void openSQLITE(const QString &filename);

public:
	/// Nonblocking query
	void execAssoc(QString query, const AssocArgs &args = AssocArgs());
	void exec(QString query, const Args &args = Args());

	void blockingExecAssoc(const QString &query, const AssocArgs &args = AssocArgs());
	void blockingExec(const QString &query, const Args &args = Args());

	/// Blocking query, returns insert id
	QVariant insertAssoc(const QString &query, const AssocArgs &args = AssocArgs());
	QVariant insert(const QString &query, const Args &args = Args());

	/// Blocking query, returns first row selected
	QSqlRecord selectRowAssoc(const QString &query, const AssocArgs &args = AssocArgs());
	QSqlRecord selectRow(const QString &query, const Args &args = Args());

	/// Blocking query, returns first row selected (returns def if no rows)
	QSqlRecord selectRowDefAssoc(const QString &query, const AssocArgs &args = AssocArgs(), QSqlRecord def = QSqlRecord());
	QSqlRecord selectRowDef(const QString &query, const Args &args = Args(), QSqlRecord def = QSqlRecord());

	/// Blocking query, returns first value of the first row
	QVariant selectValueAssoc(const QString &query, const AssocArgs &args = AssocArgs());
	QVariant selectValue(const QString &query, const Args &args = Args());

	/// Blocking query, returns the query
	DBQuery selectQueryAssoc(const QString &query, const AssocArgs &args = AssocArgs());
	DBQuery selectQuery(const QString &query, const Args &args = Args());

	/// Creates a query on the db thread and calls opFunc on it
	void customQueryOperation(const QueryOpFunc &opFunc);

	/// Blocks the calling thread untill all queued queries are executed
	void waitJobDone();

public:
	QString queryDesc(const QSqlQuery &q, const Args &args);
	QString queryDesc(const QSqlQuery &q, const AssocArgs &args);

signals:
	void sigQueryError(QString query, QString error);
	void sigOpenError(QString error);

private:
	void blockingExecAssoc(const QString &query, const AssocArgs &args, ManipFunc manF);
	void blockingExec(const QString &query, const Args &args, ManipFunc manF);

private:
	JobThread jobThread_;
	QSqlDatabase db_;

};

#endif // DBMANAGER_H
