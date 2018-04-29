#include "backupmanager.h"

#include <QSqlQuery>
#include <QDateTime>
#include <QVariant>
#include <QApplication>
#include <QDirIterator>
#include <QRegExp>

#include "global.h"

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
}

void BackupManager::checkForBackups()
{
	qlonglong currentTime = QDateTime::currentSecsSinceEpoch();
	currentTimeFileSuffix_ = QDateTime::fromSecsSinceEpoch(currentTime).toString("yyyyMMddhhmmss");

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

	QSqlQuery qCheckFile;
	qCheckFile.prepare("UPDATE files SET lastChecked = :lastChecked WHERE id = :id");
	qCheckFile.bindValue(":lastChecked", currentTime);

	QSqlQuery qInsertHistory;
	qInsertHistory.prepare("INSERT INTO history (backupDirectory, remoteFilePath, originalFilePath, version) VALUES (:backupDirectory, :remoteFilePath, :originalFilePath, :version)");
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

		const QStringList excludeFilters = qDir.value("excludeFilter").toString().split('\n', QString::SkipEmptyParts);
		QVector<QRegExp> excludeRegexes;

		for(const QString &filter : excludeFilters)
			excludeRegexes.append(QRegExp(filter, Qt::CaseInsensitive, QRegExp::WildcardUnix));

		qFile.bindValue(":backupDirectory", dirId);
		qInsertFile.bindValue(":backupDirectory", dirId);
		qInsertHistory.bindValue(":backupDirectory", dirId);

		emit logInfo(tr("Zálohuji složku '%1'...").arg(sourceDir));

		if(!sourceQDir.exists()) {
			emit logError(tr("Složka '%1' neexistuje!'").arg(sourceDir));
			continue;
		}

		if(!remoteQDir.exists()) {
			emit logError(tr("Složka pro zálohy '%1' neexistuje!'").arg(remoteDir));
			continue;
		}

		// Walk files in the sourceDir and update them eventually
		QDirIterator iter(sourceDir, QDir::Files, QDirIterator::Subdirectories);
		while(iter.hasNext()) {
			if(thread_.isInterruptionRequested())
				return;

			iter.next();

			const QFileInfo fileInfo = iter.fileInfo();
			const QString filePath = sourceQDir.relativeFilePath(fileInfo.absoluteFilePath());

			// Check exclude filter
			{
				bool isOk = true;
				for(QRegExp &regex : excludeRegexes) {
					if(regex.exactMatch(filePath)) {
						isOk = false;
						break;
					}
				}

				if(!isOk)
					continue;
			}

			qFile.bindValue(":filePath", filePath);
			qFile.exec();

			// File is not in the database -> copy it and create record
			if(!qFile.next()) {
				const QString sourceFilePath = sourceQDir.absoluteFilePath(filePath);
				const QString remoteFilePath = remoteQDir.absoluteFilePath(filePath);
				const QString remotePath = QFileInfo(remoteFilePath).absolutePath();

				emit logInfo(tr("Zálohuji nový soubor '%1'...").arg(sourceFilePath));
				if( !QDir().mkpath(remotePath) ) {
					emit logError(tr("Nepodařilo se vytvořit cestu '%1'!").arg(remotePath));
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
				const QString remotePath = QFileInfo(remoteFilePath).absolutePath();

				emit logInfo(tr("Soubor '%1' změněn, vytvářím zálohu...").arg(sourceFilePath));

				QString newFilePath = QDir(remotePath).absoluteFilePath( QString("%1.bkp.%2.%3").arg( fileInfo.completeBaseName(), currentTimeFileSuffix_, fileInfo.suffix() ) );
				QString newRemoteFilePath = remoteQDir.absoluteFilePath(newFilePath);

				if(!QFile(remoteFilePath).rename(newRemoteFilePath))
					emit logError(tr("Nepodařilo se vytvořit soubor historie '%1'").arg(newRemoteFilePath));

				qInsertHistory.bindValue(":originalFilePath", filePath);
				qInsertHistory.bindValue(":remoteFilePath", newRemoteFilePath);
				qInsertHistory.exec();

				if(!copyFile(sourceFilePath, remoteFilePath))
					continue;

				qUpdateFile.bindValue(":id", qFile.value("id"));
				qUpdateFile.bindValue(":remoteVersion", fileInfo.lastModified().toSecsSinceEpoch());
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

			qDeleteFile.bindValue(":id", qDeletedFile.value("id"));
			qDeleteFile.exec();

			QFileInfo fileInfo(remoteFilePath);
			QString newFilePath = QDir(fileInfo.path()).absoluteFilePath( QString("%1.bkp.%2.%3").arg( fileInfo.completeBaseName(), currentTimeFileSuffix_, fileInfo.suffix() ) );
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
			const QString filePath = qOutdatedHistory.value("remoteFilePath").toString();
			const QString remoteFilePath = remoteQDir.absoluteFilePath(filePath);

			emit logInfo(tr("Mažu starou zálohu '%1'.").arg(remoteFilePath));

			qDeleteOutdatedHistory.bindValue(":id", qOutdatedHistory.value("id"));
			qDeleteOutdatedHistory.exec();

			if(!QFile(remoteFilePath).remove())
				emit logError(tr("Nepodařilo se smazat starou zálohu '%1'!").arg(remoteFilePath));
		}

		qUpdateDir.bindValue(":id", dirId);
		qUpdateDir.exec();

		emit logSuccess(tr("Zálohování složky '%1' dokončeno.").arg(sourceDir));
		emit backupFinished();
	}

	updateBackupCheckTimer();
}

void BackupManager::updateBackupCheckTimer()
{
	QSqlQuery q("SELECT MIN(IFNULL(lastFinishedBackup, 0) + backupInterval) FROM backupDirectories");
	q.next();

	if( q.value(0).isNull() ) {
		backupCheckTimer_.stop();
	} else {
		int diff = qMax<qlonglong>(1, q.value(0).toLongLong() - QDateTime::currentSecsSinceEpoch());
		backupCheckTimer_.setInterval(diff * 1000);
		backupCheckTimer_.start();
	}
}

bool BackupManager::copyFile(QString sourceFilePath, QString targetFilePath)
{
	if( QFile(targetFilePath).exists() ) {
		const QString newFilePath = targetFilePath + ".orig." + currentTimeFileSuffix_;
		emit logWarning(tr("Soubor '%1' již existuje, stará verze bude přejmenována na '%2'.").arg(targetFilePath, newFilePath));

		if( !QFile(targetFilePath).rename(newFilePath) ) {
			emit logError(tr("Nepodařilo se smazat soubor '%1', který překážel záloze!").arg(targetFilePath));
			return false;
		}
	}

	bool result = QFile(sourceFilePath).copy(targetFilePath);
	if( !result )
		emit logError(tr("Nepodařilo se zkopírovat soubor '%1' -> '%2'!").arg(sourceFilePath, targetFilePath));

	return result;
}
