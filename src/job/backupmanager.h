#ifndef BACKUPMANAGER_H
#define BACKUPMANAGER_H

#include <QObject>
#include <QTimer>
#include <QThread>
#include <QSqlQuery>

class BackupManager : public QObject
{
	Q_OBJECT

public:
	BackupManager();

signals:
	void logInfo(QString text);
	void logWarning(QString text);
	void logError(QString text);
	void logSuccess(QString text);
	void backupFinished();

public slots:
	void checkForBackups();
	void updateBackupCheckTimer();

private:
	bool copyFile(QString sourceFilePath, QString targetFilePath);

private:
	QThread thread_;
	QTimer backupCheckTimer_;

private:
	QString currentTimeFileSuffix_;

};

#endif // BACKUPMANAGER_H
