#ifndef GLOBAL_H
#define GLOBAL_H

#include <QSystemTrayIcon>
#include <QObject>
#include <QMenu>

#include "threaddb/dbmanager.h"

class MainWindow;
class BackupDirectoryEditDialog;
class AboutDialog;
class BackupManager;

class Global : public QObject
{
	Q_OBJECT

public:
	Global();

public:
	void init();
	void uninit();

public:
	MainWindow *mainWindow;
	BackupDirectoryEditDialog *backupDirectoryEditDialog;
	AboutDialog *aboutDialog;

public:
	QSystemTrayIcon *trayIcon;
	QMenu *trayIconMenu;

public:
	BackupManager *backupManager;
	DBManager *db;

private:
	void initDb();

private slots:
	void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
	void onLogError();
	void onDbQueryError(QString query, QString err);
	void onDbOpenError(QString err);

};

extern Global *global;

#endif // GLOBAL_H
