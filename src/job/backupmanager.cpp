#include "backupmanager.h"

#include <QDateTime>
#include <QVariant>
#include <QApplication>
#include <QDirIterator>
#include <QRegExp>
#include <QSqlQuery>
#include <QElapsedTimer>
#include <QSqlError>

#include "global.h"

BackupManager::BackupManager()
{
	connect(qApp, &QApplication::aboutToQuit, this, [this]{
		thread_.requestInterruption();
		thread_.quit();
	});

	thread_.start();

	backupCheckTimer_ = new QTimer();
	connect(backupCheckTimer_, SIGNAL(timeout()), this, SLOT(checkForBackups()));
	backupCheckTimer_->moveToThread(&thread_);

	connect(this, SIGNAL(logError(QString)), this, SLOT(updateLastLogTime()));
	connect(this, SIGNAL(logInfo(QString)), this, SLOT(updateLastLogTime()));
	connect(this, SIGNAL(logWarning(QString)), this, SLOT(updateLastLogTime()));

	moveToThread(&thread_);
}

BackupManager::~BackupManager()
{

}

void BackupManager::checkForBackups()
{
	const qlonglong currentTime = QDateTime::currentSecsSinceEpoch();
	currentTimeFileSuffix_ = QDateTime::fromSecsSinceEpoch(currentTime).toString("yyyyMMddhhmmss");

	emit logInfo(tr("Kontroluji zálohy..."));

	DBQuery findFileQuery(global->db);
	findFileQuery.prepare("SELECT * FROM files WHERE (backupDirectory = :backupDirectory) AND (filePath = :filePath)");

	/*DBQuery updateLastCheckedQuery(global->db);
	updateLastCheckedQuery.prepare("UPDATE files SET lastChecked = :lastChecked WHERE id = :id");*/

	size_t filesChecked = 0;

	auto backupDirectory = global->db->selectQueryAssoc("SELECT * FROM backupDirectories WHERE IFNULL(lastFinishedBackup+backupInterval, 0) <= :time", {{":time", currentTime}});
	while( backupDirectory.next() ) {
		if( thread_.isInterruptionRequested() )
			break;

		const QString sourceDir = backupDirectory.value("sourceDir").toString();
		const QString remoteDir = backupDirectory.value("remoteDir").toString();
		const qlonglong dirId = backupDirectory.value("id").toLongLong();

		if(sourceDir.isEmpty() || remoteDir.isEmpty()) {
			emit logError(tr("Vnitřní chyba systému (dir.isEmpty)"));
			lastLogTime_ = QDateTime::currentDateTime();
			continue;
		}

		const QDir sourceQDir(sourceDir);
		const QDir remoteQDir(remoteDir);

		const QStringList excludeFilters = backupDirectory.value("excludeFilter").toString().split('\n', QString::SkipEmptyParts);
		QVector<QRegExp> excludeRegexes;

		for(const QString &filter : excludeFilters)
			excludeRegexes.append(QRegExp(filter, Qt::CaseInsensitive, QRegExp::WildcardUnix));

		QVector<qlonglong> unchangedFileIds;

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
		QDirIterator iter(sourceDir, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
		while(iter.hasNext()) {
			if(thread_.isInterruptionRequested())
				return;

			if(!remoteQDir.exists()) {
				emit logError(tr("Složka '%1' přestala být dostupná.").arg(remoteDir));
				break;
			}

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

			if(lastLogTime_.msecsTo(QDateTime::currentDateTime()) >= 10000)
				emit logInfo(tr("Zálohuji '%1'; zkontrolováno souborů: %2").arg(sourceDir).arg(filesChecked));

			filesChecked ++;

			findFileQuery.execAssoc({{":backupDirectory", dirId}, {":filePath", filePath}});

			// File is not in the database -> copy it and create record
			if(!findFileQuery.next()) {
				const QString sourceFilePath = sourceQDir.absoluteFilePath(filePath);
				const QString remoteFilePath = remoteQDir.absoluteFilePath(filePath);
				const QString remotePath = QFileInfo(remoteFilePath).absolutePath();

				emit logInfo(tr("Zálohuji nový soubor '%1'...").arg(sourceFilePath));

				if( !QDir().mkpath(remotePath) ) {
					emit logError(tr("Nepodařilo se vytvořit cestu '%1'!").arg(remotePath));
					continue;
				}

				if( !copyFile(sourceFilePath, remoteFilePath) )
					continue;

				global->db->execAssoc(
							"INSERT INTO files (backupDirectory, filePath, lastChecked, remoteVersion) VALUES (:backupDirectory, :filePath, :lastChecked, :remoteVersion)",
							{
								{":lastChecked", currentTime},
								{":backupDirectory", dirId},
								{":filePath", filePath},
								{":remoteVersion", fileInfo.lastModified().toSecsSinceEpoch()}
							});

			// File in the database is older -> create a backup of it and copy a new version
			} else if(fileInfo.lastModified().toSecsSinceEpoch() != findFileQuery.value("remoteVersion").toLongLong()) {
				const QString sourceFilePath = sourceQDir.absoluteFilePath(filePath);
				const QString remoteFilePath = remoteQDir.absoluteFilePath(filePath);
				const QString remotePath = QFileInfo(remoteFilePath).absolutePath();

				emit logInfo(tr("Soubor '%1' změněn, vytvářím zálohu...").arg(sourceFilePath));

				QString newFilePath = QDir(remotePath).absoluteFilePath( QString("%1.bkp.%2.%3").arg( fileInfo.completeBaseName(), currentTimeFileSuffix_, fileInfo.suffix() ) );
				QString newRemoteFilePath = remoteQDir.absoluteFilePath(newFilePath);

				if(!QFile(remoteFilePath).rename(newRemoteFilePath))
					emit logError(tr("Nepodařilo se vytvořit soubor historie '%1'").arg(newRemoteFilePath));

				global->db->execAssoc(
							"INSERT INTO history (backupDirectory, remoteFilePath, originalFilePath, version) VALUES (:backupDirectory, :remoteFilePath, :originalFilePath, :version)",
							{
								{":version", currentTime},
								{":backupDirectory", dirId},
								{":originalFilePath", filePath},
								{":remoteFilePath", newFilePath}
							});

				if(!copyFile(sourceFilePath, remoteFilePath))
					continue;

				global->db->execAssoc(
							"UPDATE files SET lastChecked = :lastChecked, remoteVersion = :remoteVersion WHERE id = :id",
							{
								{":lastChecked", currentTime},
								{":remoteVersion", fileInfo.lastModified().toSecsSinceEpoch()},
								{":id", findFileQuery.value("id")}
							});

			// Otherwise just update lastChecked of the file
			} else {
				//updateLastCheckedQuery.execAssoc({{":lastChecked", currentTime}, {":id", findFileQuery.value("id")}});
				unchangedFileIds.append(findFileQuery.value("id").toLongLong());
			}

			if(unchangedFileIds.size() >= 4096)
				commitUnchangedFileIds(currentTime, unchangedFileIds);
		}

		commitUnchangedFileIds(currentTime, unchangedFileIds);

		// Walk removed files and update them as backup
		auto removedFile = global->db->selectQueryAssoc(
					"SELECT * FROM files WHERE (backupDirectory = :backupDirectory) AND (lastChecked <> :lastChecked)",
					{
						{":lastChecked", currentTime},
						{":backupDirectory", dirId}
					});

		while(removedFile.next()) {
			const QString filePath = removedFile.value("filePath").toString();
			const QString sourceFilePath = sourceQDir.absoluteFilePath(filePath);
			const QString remoteFilePath = remoteQDir.absoluteFilePath(filePath);

			emit logInfo(tr("Soubor '%1' smazán, vytvářím zálohu...").arg(sourceFilePath));

			global->db->execAssoc("DELETE FROM files WHERE id = :id", {{":id", removedFile.value("id")}});

			QFileInfo fileInfo(remoteFilePath);
			QString newFilePath = QDir(fileInfo.path()).absoluteFilePath( QString("%1.bkp.%2.%3").arg( fileInfo.completeBaseName(), currentTimeFileSuffix_, fileInfo.suffix() ) );
			QString newRemoteFilePath = remoteQDir.absoluteFilePath(newFilePath);

			if(!QFile(remoteFilePath).rename(newRemoteFilePath)) {
				emit logError(tr("Nepodařilo se vytvořit soubor historie '%1'").arg(newRemoteFilePath));
				continue;
			}

			global->db->execAssoc(
						"INSERT INTO history (backupDirectory, remoteFilePath, originalFilePath, version) VALUES (:backupDirectory, :remoteFilePath, :originalFilePath, :version)",
						{
							{":version", currentTime},
							{":backupDirectory", dirId},
							{":originalFilePath", filePath},
							{":remoteFilePath", newFilePath}
						});
		}

		auto backupToRemove = global->db->selectQueryAssoc(
					"SELECT * FROM history WHERE (backupDirectory = :backupDirectory) AND (version < :version)",
					{
						{":version", currentTime - backupDirectory.value("keepHistoryDuration").toLongLong()},
						{":backupDirectory", dirId}
					});

		while(backupToRemove.next()) {
			const QString filePath = backupToRemove.value("remoteFilePath").toString();
			const QString remoteFilePath = remoteQDir.absoluteFilePath(filePath);

			emit logInfo(tr("Mažu starou zálohu '%1'.").arg(remoteFilePath));

			global->db->execAssoc("DELETE FROM history WHERE id = :id", {{":id", backupToRemove.value("id")}});

			if(!QFile(remoteFilePath).remove())
				emit logError(tr("Nepodařilo se smazat starou zálohu '%1'!").arg(remoteFilePath));

			remoteQDir.rmpath(filePath);
		}

		global->db->execAssoc("UPDATE backupDirectories SET lastFinishedBackup = :lastFinishedBackup WHERE id = :id", {{":lastFinishedBackup", currentTime}, {":id", dirId}});

		emit logSuccess(tr("Zálohování složky '%1' dokončeno.").arg(sourceDir));

		global->db->waitJobDone();
		emit backupFinished();
	}

	emit logInfo(tr("Kontrola záloh dokončena."));

	updateBackupCheckTimer();
}

void BackupManager::updateBackupCheckTimer()
{
	QVariant lastFinishedBackup = global->db->selectValueAssoc("SELECT MIN(IFNULL(lastFinishedBackup, 0) + backupInterval) FROM backupDirectories");

	if( lastFinishedBackup.isNull() ) {
		backupCheckTimer_->stop();
	} else {
		int diff = qMax<qlonglong>(60 * 10, lastFinishedBackup.toLongLong() - QDateTime::currentSecsSinceEpoch());
		backupCheckTimer_->setInterval(diff * 1000);
		backupCheckTimer_->start();
	}
}

bool BackupManager::copyFile(QString sourceFilePath, QString targetFilePath)
{
	if( QFile(targetFilePath).exists() ) {
		const QFileInfo origTargetFileInfo(targetFilePath);
		const QString newFilePath = QString("%1.orig.%2").arg( origTargetFileInfo.completeBaseName(), origTargetFileInfo.suffix() );//targetFilePath + ".orig." + currentTimeFileSuffix_;

		emit logWarning(tr("Soubor '%1' již existuje, stará verze bude přejmenována na '%2'.").arg(targetFilePath, newFilePath));

		if( !QFile(targetFilePath).rename(newFilePath) ) {
			emit logError(tr("Nepodařilo se smazat soubor '%1', který překážel záloze!").arg(targetFilePath));
			return false;
		}
	}

	/*bool result = QFile(sourceFilePath).copy(targetFilePath);
	if( !result )
		emit logError(tr("Nepodařilo se zkopírovat soubor '%1' -> '%2'!").arg(sourceFilePath, targetFilePath));*/

	{
		QFile src(sourceFilePath);
		QFile tgt(targetFilePath);

		if(!src.open(QIODevice::ReadOnly)) {
			emit logError(tr("Nepodařilo se otevřít soubor '%1' pro čtení!").arg(sourceFilePath));
			return false;
		}

		if(!tgt.open(QIODevice::WriteOnly)) {
			emit logError(tr("Nepodařilo se otevřít soubor '%1' pro zápis!").arg(targetFilePath));
			return false;
		}

		QElapsedTimer tmr;
		tmr.start();

		QByteArray buffer;
		buffer.resize(4096);

		const qint64 fileSize = src.size();
		qint64 bytesRemaining = fileSize;

		while( bytesRemaining ) {
			qint64 bytesRead = src.read(buffer.data(), qMin(bytesRemaining, (qint64) buffer.size()));

			if(bytesRead <= 0) {
				emit logError(tr("Chyba při čtení ze souboru '%1'!").arg(sourceFilePath));
				tgt.remove();
				return false;
			}

			qint64 bytesWritten = tgt.write(buffer.data(), bytesRead);
			if(bytesWritten != bytesRead) {
				emit logError(tr("Chyba při zápisu do souboru '%1'!").arg(targetFilePath));
				tgt.remove();
				return false;
			}

			bytesRemaining -= bytesRead;

			if(tmr.elapsed() >= 10000) {
				tmr.restart();
				emit logInfo(tr("%1%: Kopíruji '%2' -> '%3'").arg(100 - bytesRemaining*100/fileSize, 3).arg(sourceFilePath, targetFilePath));
			}
		}
	}

	return true;
	//return result;
}

void BackupManager::commitUnchangedFileIds(const qlonglong &currentTime, QVector<qlonglong> &unchangedFileIds)
{
	global->db->customQueryOperation([&](QSqlDatabase &db){
		QSqlQuery q(db);
		q.prepare("UPDATE files SET lastChecked = :lastChecked WHERE id = :id");
		q.bindValue(":lastChecked", currentTime);

		db.transaction();

		for(qlonglong id : unchangedFileIds) {
			q.bindValue(":id", id);
			q.exec();
		}

		db.commit();
	});

	unchangedFileIds.clear();
}

void BackupManager::updateLastLogTime()
{
	lastLogTime_ = QDateTime::currentDateTime();
}
