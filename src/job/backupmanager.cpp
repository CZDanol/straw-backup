#include "backupmanager.h"

#include <QSqlQuery>
#include <QDateTime>
#include <QVariant>
#include <QApplication>
#include <QDirIterator>

#include "global.h"

#include <QDebug>
#include <QSqlError>

BackupManager::BackupManager()
{
	connect(&backupCheckTimer_, SIGNAL(timeout()), this, SLOT(checkForBackups()));
	connect(qApp, &QApplication::aboutToQuit, this, [this]{
		thread_.requestInterruption();
		thread_.quit();
	});

	thread_.start();
	backupCheckTimer_.moveToThread(&thread_);
	moveToThread(&thread_);

	QMetaObject::invokeMethod(this, "checkForBackups");
}

void BackupManager::checkForBackups()
{
	qlonglong currentTime = QDateTime::currentSecsSinceEpoch();
	QString currentTimeFileSuffix = QDateTime::fromSecsSinceEpoch(currentTime).toString("ddMMyyyy_hhmmss");

	QSqlQuery qDir;
	qDir.prepare("SELECT * FROM backupDirectories WHERE IFNULL(lastFinishedBackup+backupInterval, 0) <= ?");
	qDir.bindValue(0, currentTime);

	QSqlQuery qUpdateDir;
	qUpdateDir.prepare("UPDATE backupDirectories SET lastFinishedBackup = :lastFinishedBackup WHERE id = :id");
	qUpdateDir.bindValue(":lastFinishedBackup", currentTime);

	QSqlQuery qFile;
	qFile.prepare("SELECT * FROM files WHERE (backupDirectory = :backupDirectory) AND (filePath = :filePath)");

	QSqlQuery qInsertFile;
	qInsertFile.prepare("INSERT INTO files (backupDirectory, filePath, lastChecked, remoteVersion) VALUES (:backupDirectory, :filePath, :lastChecked, :remoteVersion)");
	qInsertFile.bindValue(":lastChecked", currentTime);

	QSqlQuery qUpdateFile;
	qUpdateFile.prepare("UPDATE files SET lastChecked = :lastChecked, remoteVersion = :remoteVersion WHERE id = :id");
	qUpdateFile.bindValue(":lastChecked", currentTime);
	qUpdateFile.bindValue(":remoteVersion", currentTime);

	QSqlQuery qCheckFile;
	qCheckFile.prepare("UPDATE files SET lastChecked = :lastChecked WHERE id = :id");
	qCheckFile.bindValue(":lastChecked", currentTime);

	QSqlQuery qInsertHistory;
	qInsertHistory.prepare("INSERT INTO files (backupDirectory, remoteFilePath, originalFilePath, version) VALUES (:backupDirectory, :remoteFilePath, :originalFilePath, :version)");
	qInsertHistory.bindValue(":version", currentTime);

	QSqlQuery qDeletedFile;
	qDeletedFile.prepare("SELECT * FROM files WHERE (backupDirectory = :backupDirectory) AND (lastChecked <> :lastChecked)");
	qDeletedFile.bindValue(":lastChecked", currentTime);

	QSqlQuery qDeleteFile;
	qDeleteFile.prepare("DELETE FROM files WHERE id = :id");

	QSqlQuery qOutdatedHistory;
	qOutdatedHistory.prepare("SELECT * FROM history WHERE (backupDirectory = :backupDirectory) AND (version < :version)");

	QSqlQuery qDeleteOutdatedHistory;
	qDeleteOutdatedHistory.prepare("DELETE FROM history WHERE id = :id");

	emit logInfo(tr("Kontroluji zálohy..."));

	qDir.exec();
	while(qDir.next() && !thread_.isInterruptionRequested()) {
		const QString sourceDir = qDir.value("sourceDir").toString();
		const QString remoteDir = qDir.value("remoteDir").toString();
		const qlonglong dirId = qDir.value("id").toLongLong();

		const QDir sourceQDir(sourceDir);
		const QDir remoteQDir(remoteDir);

		qFile.bindValue(":backupDirectory", dirId);
		qInsertFile.bindValue(":backupDirectory", dirId);
		qInsertHistory.bindValue(":backupDirectory", dirId);

		emit logInfo(tr("Zálohuji složku '%1'...").arg(sourceDir));

		if(!sourceQDir.exists()) {
			emit logError(tr("Složka '%1' neexistuje!'").arg(sourceDir));
			continue;
		}

		if(!remoteQDir.mkpath(".")) {
			emit logError(tr("Nepodařilo se vytvořit složku pro zálohy '%1'!'").arg(remoteDir));
			continue;
		}

		// Walk files in the sourceDir and update them eventually
		QDirIterator iter(sourceDir, QDir::Files, QDirIterator::Subdirectories);
		while(iter.hasNext()) {
			if(thread_.isInterruptionRequested())
				return;

			iter.next();

			const QFileInfo fileInfo = iter.fileInfo();
			const QString filePath = fileInfo.filePath();

			qFile.bindValue(":filePath", filePath);
			qFile.exec();

			// File is not in the database -> copy it and create record
			if(!qFile.next()) {
				const QString sourceFilePath = sourceQDir.absoluteFilePath(filePath);

				emit logInfo(tr("Zálohuji nový soubor '%1'...").arg(sourceFilePath));
				if( !remoteQDir.mkpath(fileInfo.path()) ) {
					emit logError(tr("Nepodařilo se vytvořit cestu '%1'!").arg(remoteQDir.absoluteFilePath(fileInfo.path())));
					continue;
				}

				if( !copyFile(sourceFilePath, remoteQDir.absoluteFilePath(filePath)) )
					continue;

				qInsertFile.bindValue(":filePath", filePath);
				qInsertFile.bindValue(":remoteVersion", fileInfo.lastModified().toSecsSinceEpoch());
				qInsertFile.exec();

			// File in the database is older -> create a backup of it and copy a new version
			} else if(fileInfo.lastModified().toSecsSinceEpoch() != qFile.value("remoteVersion").toLongLong()) {
				const QString sourceFilePath = sourceQDir.absoluteFilePath(filePath);
				const QString remoteFilePath = remoteQDir.absoluteFilePath(filePath);

				emit logInfo(tr("Soubor '%1' změněn, vytvářím zálohu...").arg(sourceFilePath));

				QString newFilePath = QDir(fileInfo.dir()).absoluteFilePath( QString("%1.%2.%3").arg( fileInfo.completeBaseName(), currentTimeFileSuffix, fileInfo.suffix() ) );
				QString newRemoteFilePath = remoteQDir.absoluteFilePath(newFilePath);

				if(!QFile(remoteFilePath).rename(newRemoteFilePath))
					emit logError(tr("Nepodařilo se vytvořit soubor historie '%1'").arg(newRemoteFilePath));

				qInsertHistory.bindValue(":originalFilePath", filePath);
				qInsertHistory.bindValue(":remoteFilePath", newRemoteFilePath);
				qInsertHistory.exec();

				if(!copyFile(sourceFilePath, remoteFilePath))
					continue;

				qUpdateFile.bindValue(":id", qFile.value("id"));
				qUpdateFile.exec();

			// Otherwise just update lastChecked of the file
			} else {
				qCheckFile.bindValue(":id", qFile.value("id"));
				qCheckFile.exec();
			}
		}

		// Walk removed files and update them as backup
		qDeletedFile.bindValue(":backupDirectory", dirId);
		qDeletedFile.exec();
		while(qDeletedFile.next()) {
			const QString filePath = qDeletedFile.value("filePath").toString();
			const QString sourceFilePath = sourceQDir.absoluteFilePath(filePath);
			const QString remoteFilePath = remoteQDir.absoluteFilePath(filePath);

			emit logInfo(tr("Soubor '%1' smazán, vytvářím zálohu...").arg(sourceFilePath));

			qDeletedFile.bindValue("id", qDeletedFile.value("id"));
			qDeletedFile.exec();

			QFileInfo fileInfo(remoteFilePath);
			QString newFilePath = QDir(fileInfo.path()).absoluteFilePath( QString("%1.%2.%3").arg( fileInfo.completeBaseName(), currentTimeFileSuffix, fileInfo.suffix() ) );
			QString newRemoteFilePath = remoteQDir.absoluteFilePath(newFilePath);

			if(!QFile(remoteFilePath).rename(newRemoteFilePath)) {
				emit logError(tr("Nepodařilo se vytvořit soubor historie '%1'").arg(newRemoteFilePath));
				continue;
			}

			qInsertHistory.bindValue(":originalFilePath", filePath);
			qInsertHistory.bindValue(":remoteFilePath", newRemoteFilePath);
			qInsertHistory.exec();
		}

		qOutdatedHistory.bindValue(":version", currentTime - qDir.value("keepHistoryDuration").toLongLong());
		qOutdatedHistory.bindValue(":backupDirectory", dirId);
		qOutdatedHistory.exec();
		while(qOutdatedHistory.next()) {
			const QString filePath = qDeletedFile.value("remoteFilePath").toString();
			const QString remoteFilePath = remoteQDir.absoluteFilePath(filePath);

			emit logInfo(tr("Mažu starou zálohu '%1'.").arg(remoteFilePath));

			qDeleteOutdatedHistory.bindValue(":id", qOutdatedHistory.value("id"));
			qDeleteOutdatedHistory.exec();

			if(!QFile(remoteFilePath).remove())
				emit logError(tr("Nepodařilo se smazat starou zálohu '%1'!").arg(remoteFilePath));
		}

		qUpdateDir.bindValue(":id", dirId);
		qUpdateDir.exec();

		emit logInfo(tr("Zálohování složky '%1' dokončeno.").arg(sourceDir));
		emit backupFinished();
	}

	updateBackupCheckTimer();
}

void BackupManager::updateBackupCheckTimer()
{
	QSqlQuery q("SELECT MIN(IFNULL(lastFinishedBackup, 0) + backupInterval) FROM backupDirectories");
	q.next();

	int diff = qMax<qlonglong>(1, q.value(0).toLongLong() - QDateTime::currentSecsSinceEpoch());
	backupCheckTimer_.setInterval(diff * 1000);
	backupCheckTimer_.start();
}

bool BackupManager::copyFile(QString sourceFilePath, QString targetFilePath)
{
	if( QFile(targetFilePath).exists() ) {
		emit logWarning(tr("Soubor '%1' již existuje, stará verze bude smazána.").arg(targetFilePath));

		if( !QFile(targetFilePath).remove() ) {
			emit logError(tr("Nepodařilo se smazat soubor '%1', který překážel záloze!").arg(targetFilePath));
			return false;
		}
	}

	bool result = QFile(sourceFilePath).copy(targetFilePath);
	if( !result )
		emit logError(tr("Nepodařilo se zkopírovat soubor '%1' -> '%2'!").arg(sourceFilePath, targetFilePath));

	return result;
}
