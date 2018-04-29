#include "BackupDirectoryEditDialog.h"
#include "ui_BackupDirectoryEditDialog.h"

#include <QFileDialog>
#include <QSqlQuery>
#include <QMessageBox>
#include <QDir>

#include "global.h"
#include "gui/mainwindow.h"
#include "job/backupmanager.h"

static const QVector<qlonglong> backupIntervals{
	60 * 5,
	60 * 15,
	60 * 30,
	3600,
	3600 * 2,
	3600 * 6,
	3600 * 12,
	3600 * 24,
	3600 * 24 * 7,
	3600 * 24 * 14,
	3600 * 24 * 30
};

BackupDirectoryEditDialog::BackupDirectoryEditDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::BackupDirectoryEditDialog)
{
	ui->setupUi(this);
}

BackupDirectoryEditDialog::~BackupDirectoryEditDialog()
{
	delete ui;
}

void BackupDirectoryEditDialog::show(int rowId)
{
	rowId_ = rowId;
	const bool isNewRecord = rowId == -1;

	if(isNewRecord) {
		ui->btnBackupFolder->setText("");
		ui->btnSourceFolder->setText("");
		ui->cmbBackupInterval->setCurrentIndex(backupIntervals.indexOf(3600));
		ui->cmbBackupKeepInterval->setCurrentIndex(backupIntervals.indexOf(3600 * 24 * 7));
		ui->teExcludeFilter->setText("*.tmp");

	} else {
		QSqlQuery q;
		q.prepare("SELECT * FROM backupDirectories WHERE id = ?");
		q.bindValue(0, rowId);

		if(!q.exec() || !q.next()) {
			QMessageBox::critical(global->mainWindow, tr("Chyba"), tr("Chyba databáze (backupDirectories entry missing)"));
			return;
		}

		ui->btnBackupFolder->setText( q.value("remoteDir").toString() );
		ui->btnSourceFolder->setText( q.value("sourceDir").toString() );
		ui->cmbBackupInterval->setCurrentIndex( backupIntervals.indexOf( q.value("backupInterval").toLongLong() ) );
		ui->cmbBackupKeepInterval->setCurrentIndex( backupIntervals.indexOf( q.value("keepHistoryDuration").toLongLong() ) );
		ui->teExcludeFilter->setText(q.value("excludeFilter").toString());
	}

	ui->btnSourceFolder->setEnabled(isNewRecord);
	ui->btnBackupFolder->setEnabled(isNewRecord);

	QDialog::show();
}

void BackupDirectoryEditDialog::on_btnOk_clicked()
{
	const QString sourceFolder = ui->btnSourceFolder->text();
	const QString targetFolder = ui->btnBackupFolder->text();

	if( sourceFolder.isEmpty() || !QDir(sourceFolder).exists() ) {
		QMessageBox::critical(this, tr("Chyba"), tr("Zálovaná složka '%1' neexistuje.").arg(sourceFolder));
		return;
	}

	if( targetFolder.isEmpty() || !QDir(targetFolder).exists() ) {
		QMessageBox::critical(this, tr("Chyba"), tr("Složka na zálohy '%1' neexistuje.").arg(targetFolder));
		return;
	}

	/*if( QDir::isRelativePath(QDir(sourceFolder).relativeFilePath(targetFolder)) || QDir::isRelativePath(QDir(targetFolder).relativeFilePath(sourceFolder)) ) {
		QMessageBox::critical(this, tr("Chyba"), tr("Složka na zálohy nemůže být podsložkou zálohované složky (ani obráceně)"));
		return;
	}*/

	if( rowId_ == -1 ) {
		if( !QDir(targetFolder).isEmpty() ) {
			QMessageBox::critical(this, tr("Chyba"), tr("Složka na zálohy '%1' není prázdná!").arg(targetFolder));
			return;
		}

		QSqlQuery q("INSERT INTO backupDirectories DEFAULT VALUES");
		rowId_ = q.lastInsertId().toInt();
	}

	QSqlQuery q;
	q.prepare("UPDATE backupDirectories SET remoteDir = :remoteDir, sourceDir = :sourceDir, backupInterval = :backupInterval, keepHistoryDuration = :keepHistoryDuration, excludeFilter = :excludeFilter WHERE id = :id");
	q.bindValue(":sourceDir", ui->btnSourceFolder->text());
	q.bindValue(":remoteDir", ui->btnBackupFolder->text());
	q.bindValue(":backupInterval", backupIntervals[ui->cmbBackupInterval->currentIndex()]);
	q.bindValue(":keepHistoryDuration", backupIntervals[ui->cmbBackupKeepInterval->currentIndex()]);
	q.bindValue(":excludeFilter", ui->teExcludeFilter->toPlainText());

	q.bindValue(":id", rowId_);
	q.exec();

	accept();
	QMetaObject::invokeMethod(global->backupManager, "checkForBackups");
}

void BackupDirectoryEditDialog::on_btnCancel_clicked()
{
	close();
}

void BackupDirectoryEditDialog::on_btnSourceFolder_clicked()
{
	QString prevDir = ui->btnSourceFolder->text();
	QString dir = QFileDialog::getExistingDirectory( this, nullptr, prevDir );
	if( dir.isEmpty() || dir == prevDir )
		return;

	ui->btnSourceFolder->setText(dir);
}

void BackupDirectoryEditDialog::on_btnBackupFolder_clicked()
{
	QString prevDir = ui->btnBackupFolder->text();
	QString dir = QFileDialog::getExistingDirectory( this, nullptr, prevDir );
	if( dir.isEmpty() || dir == prevDir )
		return;

	ui->btnBackupFolder->setText(dir);
}
