#include "global.h"

#include <QStandardPaths>
#include <QDir>
#include <QApplication>
#include <QMessageBox>
#include <QFileInfo>

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

		trayIconMenu = new QMenu();
		trayIconMenu->addAction(mainWindow->getUI()->actionShowMainWindow);
		trayIconMenu->addAction(mainWindow->getUI()->actionBackupAll);
		trayIconMenu->addAction(mainWindow->getUI()->actionExit);
		trayIcon->setContextMenu(trayIconMenu);
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
	delete db;
}

void Global::initDb()
{
	db = new DBManager();
	connect(db, SIGNAL(sigQueryError(QString, QString)), this, SLOT(onDbQueryError(QString, QString)));
	connect(db, SIGNAL(sigOpenError(QString)), this, SLOT(onDbOpenError(QString)));

	QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	if( !QDir(dbPath).mkpath(".") ) {
		QMessageBox::critical(nullptr, tr("Kritická chyba"), tr("Nepodařilo se vytvořit složku pro databázi: %1").arg(dbPath));
		QApplication::quit();
		return;
	}

	QString dbFilepath = QDir(dbPath).absoluteFilePath("db.sqlite");
	bool dbExists = QFileInfo(dbFilepath).exists();

	db->openSQLITE(dbFilepath);

	if(!dbExists) {
		db->execAssoc("CREATE TABLE settings ("
					 "key VARCHAR(64) PRIMARY KEY,"
					 "value TEXT"
					 ")");
		db->execAssoc("INSERT INTO settings(key, value) VALUES('dbVersion', '2')");

		db->execAssoc("CREATE TABLE backupDirectories ("
					 "id INTEGER PRIMARY KEY,"
					 "sourceDir TEXT,"
					 "remoteDir TEXT,"
					 "lastFinishedBackup INTEGER,"
					 "backupInterval INTEGER,"
					 "keepHistoryDuration INTEGER,"
					 "excludeFilter TEXT"
					 ")");

		db->execAssoc("CREATE TABLE files ("
					 "id INTEGER PRIMARY KEY,"
					 "backupDirectory INTEGER,"
					 "filePath TEXT,"
					 "lastChecked INTEGER,"
					 "remoteVersion INTEGER" // Modified time of the backed up file
					 ")");

		db->execAssoc("CREATE TABLE history ("
					 "id INTEGER PRIMARY KEY,"
					 "backupDirectory INTEGER,"
					 "remoteFilePath TEXT,"
					 "originalFilePath TEXT,"
					 "version INTEGER"
					 ")");

		db->execAssoc("CREATE INDEX i_files_backupDirectory_filePath ON files (backupDirectory, filePath)");
		db->execAssoc("CREATE INDEX i_files_backupDirectory_lastChecked ON files (backupDirectory, lastChecked)");
		db->execAssoc("CREATE INDEX i_history_backupDirectory_version ON history (backupDirectory, version)");

	} else {
		QString version = db->selectValueAssoc("SELECT value FROM settings WHERE key = 'dbVersion'").toString();

		if(version == "1") {
			db->execAssoc("ALTER TABLE backupDirectories ADD COLUMN excludeFilter TEXT");

			db->execAssoc("CREATE INDEX i_files_backupDirectory_filePath ON files (backupDirectory, filePath)");
			db->execAssoc("CREATE INDEX i_files_backupDirectory_lastChecked ON files (backupDirectory, lastChecked)");
			db->execAssoc("CREATE INDEX i_history_backupDirectory_version ON history (backupDirectory, version)");

			db->execAssoc("UPDATE settings SET value = '2' WHERE key = 'dbVersion'");
			emit backupManager->logWarning(tr("Verze databáze aktualizovaná na verzi 2."));

			version = "2";
		}

		if(version != "2") {
			QMessageBox::critical(nullptr, tr("Kritická chyba"), tr("Nepodporovaná verze databáze (%1)").arg(version));
			exit(1);
		}
	}

	/*// REMOVEME
	db->exec("DELETE FROM files");
	db->exec("DELETE FROM history");*/
}

void Global::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
	if(reason == QSystemTrayIcon::DoubleClick)
		mainWindow->getUI()->actionShowMainWindow->trigger();
}

void Global::onLogError()
{
	if(!mainWindow->isVisible())
		trayIcon->showMessage(tr("Chyba"), tr("Během zálohování nastala chyba. Kliknutím na tuto zprávu otevřete okno aplikace."), QSystemTrayIcon::Critical);
}

void Global::onDbQueryError(QString query, QString err)
{
	QMessageBox::critical(nullptr, tr("Chyba databáze"), tr("Chyba databáze '%1' u dotazu '%2'").arg(err, query));
}

void Global::onDbOpenError(QString err)
{
	QMessageBox::critical(nullptr, tr("Kritická chyba"), tr("Nepodařilo se vytvořit/načíst databázi: %1").arg(err));
	exit(1);
}
