#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QMenu>

#include "threaddb/dbmodel.h"

#include "ui_mainwindow.h"

namespace Ui {
	class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

	void init();

public:
	virtual void closeEvent(QCloseEvent *e) override;

public:
	Ui::MainWindow *getUI() {
		return ui;
	}

private:
	int selectedFolderId();

private slots:
	void updateBkpDirList();
	void onTvDirListMenuRequested(const QPoint &pos);

	void logInfo(QString text);
	void logWarning(QString text);
	void logError(QString text);
	void logSuccess(QString text);
	void rawLog(QTextBrowser *tb, QString text);

private slots:
	void on_btnNewBackupFolder_clicked();
	void on_actionExit_triggered();
	void on_actionAbout_triggered();
	void on_actionBackupAll_triggered();
	void on_actionShowMainWindow_triggered();
	void on_btnLogScrollDown_clicked();
	void on_actionFolderDelete_triggered();
	void on_actionFolderBackupNow_triggered();
	void on_actionFolderOpenSource_triggered();
	void on_actionFolderOpenTarget_triggered();
	void on_actionFolderEdit_triggered();

private:
	Ui::MainWindow *ui;

private:
	QSqlQuery bkpDirsQuery_;
	QSqlQueryModel bkpDirsModel_;
	DBModel model_;
	QMenu *tvDirListMenu_;

};

#endif // MAINWINDOW_H
