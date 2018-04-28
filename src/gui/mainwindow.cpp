#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCloseEvent>
#include <QMessageBox>
#include <QDateTime>
#include <QScrollBar>

#include "gui/backupdirectoryeditdialog.h"
#include "job/backupmanager.h"
#include "global.h"

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::init()
{
	bkpDirsQuery_.prepare(QString("SELECT id, sourceDir AS '%1', strftime('%2', datetime(lastFinishedBackup, 'unixepoch')) AS '%3' FROM backupDirectories ORDER BY sourceDir ASC")
			.arg(tr("Zdrojová složka"), tr("%d.%m.%Y %H:%S"), tr("Poslední záloha")));
	ui->tvDirList->setModel(&bkpDirsModel_);

	connect(global->backupDirectoryEditDialog, SIGNAL(accepted()), this, SLOT(updateBkpDirList()));
	connect(ui->tvDirList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(onBkpListSelectionChanged()));
	connect(global->backupManager, SIGNAL(logInfo(QString)), this, SLOT(logInfo(QString)));
	connect(global->backupManager, SIGNAL(logWarning(QString)), this, SLOT(logWarning(QString)));
	connect(global->backupManager, SIGNAL(logError(QString)), this, SLOT(logError(QString)));
	connect(global->backupManager, SIGNAL(backupFinished()), this, SLOT(updateBkpDirList()));

	updateBkpDirList();
	onBkpListSelectionChanged();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
#ifdef Q_OS_OSX
		if (!event->spontaneous() || !isVisible()) {
				return;
		}
#endif

	hide();
	e->ignore();
}

void MainWindow::updateBkpDirList()
{
	bkpDirsQuery_.exec();
	bkpDirsModel_.setQuery(bkpDirsQuery_);
	ui->tvDirList->hideColumn(0);
	ui->tvDirList->show();
}

void MainWindow::onBkpListSelectionChanged()
{
	bool selected = !ui->tvDirList->selectionModel()->selectedRows().isEmpty();
	ui->btnDeleteBackupFolder->setEnabled(selected);
	ui->btnBackupNow->setEnabled(selected);
}

void MainWindow::logInfo(QString text)
{
	ui->tbLog->append(QString("[%1] %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), text));
	ui->tbLog->verticalScrollBar()->setValue( ui->tbLog->verticalScrollBar()->maximum() );
}

void MainWindow::logWarning(QString text)
{
	logInfo("<span style='color: orange;'>" + text + "</span>");
}

void MainWindow::logError(QString text)
{
	logInfo("<span style='color: red;'>" + text + "</span>");
}

void MainWindow::on_btnExit_clicked()
{
	QApplication::quit();
}

void MainWindow::on_btnNewBackupFolder_clicked()
{
	global->backupDirectoryEditDialog->show(-1);
}

void MainWindow::on_btnDeleteBackupFolder_clicked()
{
	int row = ui->tvDirList->selectionModel()->selectedRows().first().row();
	if( row < 0 )
		return;

	int id = bkpDirsModel_.data(bkpDirsModel_.index(row, 0)).toInt();

	if( QMessageBox::question(this, tr("Potvrdit smazání"), tr("Opravdu smazat vybraný záznam?")) != QMessageBox::Yes )
		return;

	QSqlQuery q;
	q.prepare("DELETE FROM files WHERE backupDirectory = ?");
	q.bindValue(0, id);
	q.exec();

	q.prepare("DELETE FROM history WHERE backupDirectory = ?");
	q.bindValue(0, id);
	q.exec();

	q.prepare("DELETE FROM backupDirectories WHERE id = ?");
	q.bindValue(0, id);
	q.exec();

	updateBkpDirList();
}

void MainWindow::on_tvDirList_activated(const QModelIndex &index)
{
	int row = index.row();
	if( row < 0 )
		return;

	int id = bkpDirsModel_.data(bkpDirsModel_.index(row, 0)).toInt();
	global->backupDirectoryEditDialog->show( id );
}

void MainWindow::on_btnBackupNow_clicked()
{
	int row = ui->tvDirList->selectionModel()->selectedRows().first().row();
	if( row < 0 )
		return;

	int id = bkpDirsModel_.data(bkpDirsModel_.index(row, 0)).toInt();

	QSqlQuery q;
	q.prepare("UPDATE backupDirectories SET lastFinishedBackup = NULL WHERE id = ?");
	q.bindValue(0, id);
	q.exec();

	QMetaObject::invokeMethod(global->backupManager, "checkForBackups");
}
