#include "mainwindow.h"

#include <QCloseEvent>
#include <QMessageBox>
#include <QDateTime>
#include <QScrollBar>
#include <QDesktopServices>
#include <QDir>
#include <QTextCursor>

#include "gui/backupdirectoryeditdialog.h"
#include "gui/aboutdialog.h"
#include "job/backupmanager.h"
#include "threaddb/dbmanager.h"
#include "global.h"

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	ui->twBackups->setCornerWidget(ui->twBackupsCorner);
	ui->twLog->setCornerWidget(ui->twLogCorner);

	tvDirListMenu_ = new QMenu(this);
	tvDirListMenu_->addActions({ui->actionFolderBackupNow, ui->actionFolderEdit, ui->actionFolderDelete});
	tvDirListMenu_->addSeparator();
	tvDirListMenu_->addActions({ui->actionFolderOpenSource, ui->actionFolderOpenTarget});
	connect(ui->tvDirList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onTvDirListMenuRequested(QPoint)));
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::init()
{
	//ui->tvDirList->setModel(&bkpDirsModel_);
	ui->tvDirList->setModel(&model_);

	connect(global->backupDirectoryEditDialog, SIGNAL(accepted()), this, SLOT(updateBkpDirList()));
	connect(global->backupManager, SIGNAL(logInfo(QString)), this, SLOT(logInfo(QString)));
	connect(global->backupManager, SIGNAL(logWarning(QString)), this, SLOT(logWarning(QString)));
	connect(global->backupManager, SIGNAL(logError(QString)), this, SLOT(logError(QString)));
	connect(global->backupManager, SIGNAL(logSuccess(QString)), this, SLOT(logSuccess(QString)));
	connect(global->backupManager, SIGNAL(backupFinished()), this, SLOT(updateBkpDirList()));

	updateBkpDirList();
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

	global->trayIcon->showMessage(tr("Program stále běží na pozadí"), tr("Program Straw Backup stále běží na pozadí. Pokud jej chcete vypnout, klikněte pravým tlačítkem na ikonu programu a vyberte 'Ukončit'."));
}

int MainWindow::selectedFolderId()
{
	if( ui->tvDirList->selectionModel()->selectedRows().isEmpty() )
		return -1;

	int row = ui->tvDirList->selectionModel()->selectedRows().first().row();
	if( row < 0 )
		return -1;

	return model_.data(model_.index(row, 0)).toInt();
}

void MainWindow::updateBkpDirList()
{
	//bkpDirsModel_.setQuery(0);
	model_.setQuery(global->db->selectQueryAssoc(
		QString("SELECT id, sourceDir AS '%1', remoteDir AS '%4', strftime('%2', datetime(lastFinishedBackup, 'unixepoch', 'localtime')) AS '%3' FROM backupDirectories ORDER BY sourceDir ASC")
		.arg(tr("Zdrojová složka"), tr("%d.%m.%Y %H:%M"), tr("Poslední záloha"), tr("Cílová složka"))));

	ui->tvDirList->hideColumn(0);
	ui->tvDirList->show();
}

void MainWindow::onTvDirListMenuRequested(const QPoint &pos)
{
	int id = selectedFolderId();
	if( id == -1 )
		return;

	tvDirListMenu_->popup(ui->tvDirList->viewport()->mapToGlobal(pos));
}

void MainWindow::logInfo(QString text)
{
	rawLog("<span style='color: gray;'>" + text + "</span>");
}

void MainWindow::logWarning(QString text)
{
	rawLog("<span style='color: orange;'>" + text + "</span>");
}

void MainWindow::logError(QString text)
{
	rawLog("<span style='color: red;'>" + text + "</span>");
}

void MainWindow::logSuccess(QString text)
{
	rawLog("<span style='color: green;'>" + text + "</span>");
}

void MainWindow::rawLog(QString text)
{
	QScrollBar *scrollBar = ui->tbLog->verticalScrollBar();
	const bool wasDown = scrollBar->value() == scrollBar->maximum();

	ui->tbLog->append(QString("<span style='color: gray;'>[%1]</span> %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), text));
	if(wasDown)
		scrollBar->setValue( scrollBar->maximum() );


	QTextCursor cursor = ui->tbLog->textCursor();
	cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
	cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, ui->tbLog->document()->lineCount() - 512);
	cursor.removeSelectedText();
}

void MainWindow::on_btnNewBackupFolder_clicked()
{
	global->backupDirectoryEditDialog->show(-1);
}

void MainWindow::on_actionExit_triggered()
{
	QApplication::quit();
}

void MainWindow::on_actionAbout_triggered()
{
	global->aboutDialog->show();
}

void MainWindow::on_actionBackupAll_triggered()
{
	global->db->blockingExecAssoc("UPDATE backupDirectories SET lastFinishedBackup = NULL");
	QMetaObject::invokeMethod(global->backupManager, "checkForBackups");
}

void MainWindow::on_actionShowMainWindow_triggered()
{
	show();
	activateWindow();
	raise();
}

void MainWindow::on_btnLogScrollDown_clicked()
{
	ui->tbLog->verticalScrollBar()->setValue(ui->tbLog->verticalScrollBar()->maximum());
}

void MainWindow::on_actionFolderDelete_triggered()
{
	int id = selectedFolderId();
	if( id == -1 )
		return;

	if( QMessageBox::question(this, tr("Potvrdit smazání"), tr("Opravdu smazat vybraný záznam?")) != QMessageBox::Yes )
		return;

	global->db->blockingExec("DELETE FROM files WHERE backupDirectory = ?", {id});
	global->db->blockingExec("DELETE FROM history WHERE backupDirectory = ?", {id});
	global->db->blockingExec("DELETE FROM backupDirectories WHERE id = ?", {id});

	updateBkpDirList();
}

void MainWindow::on_actionFolderBackupNow_triggered()
{
	int id = selectedFolderId();
	if( id == -1 )
		return;

	global->db->blockingExec("UPDATE backupDirectories SET lastFinishedBackup = NULL WHERE id = ?", {id});
	QMetaObject::invokeMethod(global->backupManager, "checkForBackups");
}

void MainWindow::on_actionFolderOpenSource_triggered()
{
	int id = selectedFolderId();
	if( id == -1 )
		return;

	QDesktopServices::openUrl( QUrl( "file:///" + QDir::toNativeSeparators( global->db->selectValue("SELECT sourceDir FROM backupDirectories WHERE id = ?", {id}).toString() )));
}

void MainWindow::on_actionFolderOpenTarget_triggered()
{
	int id = selectedFolderId();
	if( id == -1 )
		return;

	QDesktopServices::openUrl( QUrl( "file:///" + QDir::toNativeSeparators( global->db->selectValue("SELECT remoteDir FROM backupDirectories WHERE id = ?", {id}).toString() )));
}

void MainWindow::on_actionFolderEdit_triggered()
{
	int id = selectedFolderId();
	if( id == -1 )
		return;

	global->backupDirectoryEditDialog->show( id );
}
