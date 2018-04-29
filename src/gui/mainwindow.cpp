#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCloseEvent>
#include <QMessageBox>
#include <QDateTime>
#include <QScrollBar>
#include <QDesktopServices>
#include <QDir>

#include "gui/backupdirectoryeditdialog.h"
#include "gui/aboutdialog.h"
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
	bkpDirsQuery_.prepare(QString("SELECT id, sourceDir AS '%1', remoteDir AS '%4', strftime('%2', datetime(lastFinishedBackup, 'unixepoch', 'localtime')) AS '%3' FROM backupDirectories ORDER BY sourceDir ASC")
			.arg(tr("Zdrojová složka"), tr("%d.%m.%Y %H:%M"), tr("Poslední záloha"), tr("Cílová složka")));
	ui->tvDirList->setModel(&bkpDirsModel_);

	connect(global->backupDirectoryEditDialog, SIGNAL(accepted()), this, SLOT(updateBkpDirList()));
	connect(ui->tvDirList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(onBkpListSelectionChanged()));
	connect(global->backupManager, SIGNAL(logInfo(QString)), this, SLOT(logInfo(QString)));
	connect(global->backupManager, SIGNAL(logWarning(QString)), this, SLOT(logWarning(QString)));
	connect(global->backupManager, SIGNAL(logError(QString)), this, SLOT(logError(QString)));
	connect(global->backupManager, SIGNAL(logSuccess(QString)), this, SLOT(logSuccess(QString)));
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

	global->trayIcon->showMessage(tr("Program stále běží na pozadí"), tr("Program Straw Backup stále běží. Pokud jej chcete vypnout, otevřete okno programu a v menu zvolte položku 'Ukončit'."));
}

void MainWindow::updateBkpDirList()
{
	bkpDirsQuery_.exec();
	bkpDirsModel_.setQuery(bkpDirsQuery_);
	ui->tvDirList->hideColumn(0);
	ui->tvDirList->show();

	emit onBkpListSelectionChanged();
}

void MainWindow::onBkpListSelectionChanged()
{
	bool selected = !ui->tvDirList->selectionModel()->selectedRows().isEmpty();
	ui->cmbAction->setEnabled(selected);
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
}

void MainWindow::on_btnNewBackupFolder_clicked()
{
	global->backupDirectoryEditDialog->show(-1);
}

void MainWindow::on_tvDirList_activated(const QModelIndex &index)
{
	int row = index.row();
	if( row < 0 )
		return;

	int id = bkpDirsModel_.data(bkpDirsModel_.index(row, 0)).toInt();
	global->backupDirectoryEditDialog->show( id );
}

void MainWindow::on_actionExit_triggered()
{
	QApplication::quit();
}

void MainWindow::on_actionAbout_triggered()
{
	global->aboutDialog->show();
}

void MainWindow::on_cmbAction_currentIndexChanged(int index)
{
	int row = ui->tvDirList->selectionModel()->selectedRows().first().row();
	if( row < 0 )
		return;

	int id = bkpDirsModel_.data(bkpDirsModel_.index(row, 0)).toInt();
	ui->cmbAction->setCurrentIndex(0);

	switch(index) {

	case 1: {
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
		break;
	}

	case 2: {
		QSqlQuery q;
		q.prepare("UPDATE backupDirectories SET lastFinishedBackup = NULL WHERE id = ?");
		q.bindValue(0, id);
		q.exec();

		QMetaObject::invokeMethod(global->backupManager, "checkForBackups");
		break;
	}

	case 3: {
		QDesktopServices::openUrl( QUrl( "file:///" + QDir::toNativeSeparators( bkpDirsModel_.data(bkpDirsModel_.index(row, 3)).toString())));
		break;
	}

	case 4: {
		QDesktopServices::openUrl( QUrl( "file:///" + QDir::toNativeSeparators( bkpDirsModel_.data(bkpDirsModel_.index(row, 1)).toString())));
		break;
	}


	default:
		return;

	}
}
