#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlQuery>
#include <QSqlQueryModel>

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

private slots:
	void updateBkpDirList();
	void onBkpListSelectionChanged();

	void logInfo(QString text);
	void logWarning(QString text);
	void logError(QString text);

private slots:
	void on_btnNewBackupFolder_clicked();
	void on_tvDirList_activated(const QModelIndex &index);
	void on_actionExit_triggered();
	void on_actionAbout_triggered();
	void on_cmbAction_currentIndexChanged(int index);

private:
	Ui::MainWindow *ui;

private:
	QSqlQuery bkpDirsQuery_;
	QSqlQueryModel bkpDirsModel_;

};

#endif // MAINWINDOW_H
