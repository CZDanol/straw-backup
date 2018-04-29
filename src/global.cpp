#include "global.h"

#include <QStandardPaths>
#include <QDir>
#include <QApplication>
#include <QMessageBox>
#include <QFileInfo>
#include <QSqlQuery>

#include "gui/mainwindow.h"
#include "gui/backupdirectoryeditdialog.h"
#include "gui/aboutdialog.h"
#include "job/backupmanager.h"

Global *global;

Global::Global()
{

}

void Global::init()
{
	initDb();

	mainWindow = new MainWindow();
	backupDirectoryEditDialog = new BackupDirectoryEditDialog(mainWindow);
	aboutDialog = new AboutDialog(mainWindow);
	trayIcon = new QSystemTrayIcon();
	backupManager = new BackupManager();

	{
		trayIcon->setIcon(QIcon(":/16/icons8_Database_16px.png"));
		trayIcon->setToolTip(tr("Straw Backup"));
		connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(onTrayIconActivated(QSystemTrayIcon::ActivationReason)));
		connect(backupManager, SIGNAL(logError(QString)), this, SLOT(onLogError()));
		connect(trayIcon, SIGNAL(messageClicked()), mainWindow, SLOT(show()));
	}

	mainWindow->init();

	trayIcon->show();

	QMetaObject::invokeMethod(backupManager, "checkForBackups");
}

void Global::uninit()
{
	delete mainWindow;
	delete trayIcon;
	delete backupManager;
}

void Global::initDb()
{
	QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	if( !QDir(dbPath).mkpath(".") ) {
		QMessageBox::critical(nullptr, tr("Kritická chyba"), tr("Nepodařilo se vytvořit složku pro databázi: %1").arg(dbPath));
		QApplication::quit();
		return;
	}

	QString dbFilepath = QDir(dbPath).absoluteFilePath("db.sqlite");
	bool dbExists = QFileInfo(dbFilepath).exists();

	db = QSqlDatabase::addDatabase("QSQLITE");
	db.setDatabaseName(dbFilepath);

	if( !db.open() ) {
		QMessageBox::critical(nullptr, tr("Kritická chyba"), tr("Nepodařilo se vytvořit/načíst databázi: %1").arg(dbFilepath));
		exit(1);
	}

	if(!dbExists) {
		QSqlQuery q;
		q.exec("CREATE TABLE settings ("
					 "key VARCHAR(64) PRIMARY KEY,"
					 "value TEXT"
					 ")");
		q.exec("INSERT INTO settings(key, value) VALUES('dbVersion', '2')");

		q.exec("CREATE TABLE backupDirectories ("
					 "id INTEGER PRIMARY KEY,"
					 "sourceDir TEXT,"
					 "remoteDir TEXT,"
					 "lastFinishedBackup INTEGER,"
					 "backupInterval INTEGER,"
					 "keepHistoryDuration INTEGER,"
					 "excludeFilter TEXT"
					 ")");

		q.exec("CREATE TABLE files ("
					 "id INTEGER PRIMARY KEY,"
					 "backupDirectory INTEGER,"
					 "filePath TEXT,"
					 "lastChecked INTEGER,"
					 "remoteVersion INTEGER" // Modified time of the backed up file
					 ")");

		q.exec("CREATE TABLE history ("
					 "id INTEGER PRIMARY KEY,"
					 "backupDirectory INTEGER,"
					 "remoteFilePath TEXT,"
					 "originalFilePath TEXT,"
					 "version INTEGER"
					 ")");

		q.exec("CREATE INDEX i_files_backupDirectory_filePath ON files (backupDirectory, filePath)");
		q.exec("CREATE INDEX i_files_backupDirectory_lastChecked ON files (backupDirectory, lastChecked)");
		q.exec("CREATE INDEX i_history_backupDirectory_version ON history (backupDirectory, version)");

	} else {
		QSqlQuery qVersion("SELECT value FROM settings WHERE key = 'dbVersion'");
		qVersion.next();

		if(qVersion.value(0).toString() == '1') {
			QSqlQuery q;
			q.exec("ALTER TABLE backupDirectories ADD COLUMN excludeFilter TEXT");

			q.exec("CREATE INDEX i_files_backupDirectory_filePath ON files (backupDirectory, filePath)");
			q.exec("CREATE INDEX i_files_backupDirectory_lastChecked ON files (backupDirectory, lastChecked)");
			q.exec("CREATE INDEX i_history_backupDirectory_version ON history (backupDirectory, version)");

			q.exec("UPDATE settings SET value = '2' WHERE key = 'dbVersion'");
			emit backupManager->logWarning(tr("Verze databáze aktualizovaná na verzi 2."));

			qVersion.exec();
			qVersion.next();
		}

		if(qVersion.value(0).toString() != '2') {
			QMessageBox::critical(nullptr, tr("Kritická chyba"), tr("Nepodporovaná verze databáze (%1)").arg(qVersion.value(0).toString()));
			exit(1);
		}
	}
}

void Global::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
	if(reason == QSystemTrayIcon::Context || reason == QSystemTrayIcon::DoubleClick) {
		mainWindow->show();
		mainWindow->activateWindow();
		mainWindow->raise();
	}
}

void Global::onLogError()
{
	if(!mainWindow->isVisible())
		trayIcon->showMessage(tr("Chyba"), tr("Během zálohování nastala chyba. Kliknutím na tuto zprávu otevřete okno aplikace."), QSystemTrayIcon::Critical);
}
