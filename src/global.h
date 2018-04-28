#ifndef GLOBAL_H
#define GLOBAL_H

#include <QSystemTrayIcon>
#include <QObject>
#include <QMenu>
#include <QSqlDatabase>

class MainWindow;
class BackupDirectoryEditDialog;
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

public:
	QSystemTrayIcon *trayIcon;
	QMenu *trayIconMenu;

public:
	BackupManager *backupManager;
	QSqlDatabase db;

private:
	void initDb();

private slots:
	void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);

};

extern Global *global;

#endif // GLOBAL_H
