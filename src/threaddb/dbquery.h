#ifndef DBQUERY_H
#define DBQUERY_H

#include <QSharedPointer>
#include <QVariant>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QHash>

class DBManager;

class DBQuery {

public:
	using Arg = QVariant;
	using Args = QVector<QVariant>;

	using AssocArg = QPair<QString,QVariant>;
	using AssocArgs = QVector<AssocArg>;

public:
	DBQuery();
	DBQuery(DBManager *manager);
	DBQuery(DBManager *manager, QSharedPointer<QSqlQuery> query);

public:
	void prepare(const QString &query);

	void execAssoc(const AssocArgs &args);
	void exec(const Args &args);

	void execAssocAsync(const AssocArgs &args);
	void execAsync(const Args &args);

public:
	bool isValid() const;

	bool next();
	bool seek(int pos);

	QVariant value(int pos) const;
	QVariant value(const QString &fieldName) const;

	const QSqlRecord &record() const;

	int rowCount() const;
	int columnCount() const;

private:
	QSharedPointer<QSqlQuery> query_;
	QSqlRecord rec_;
	DBManager *manager_ = nullptr;
	int rowCount_ = -1;

};

#endif // DBQUERY_H
