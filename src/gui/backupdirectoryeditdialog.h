#ifndef BACKUPDIRECTORYEDITDIALOG_H
#define BACKUPDIRECTORYEDITDIALOG_H

#include <QDialog>

namespace Ui {
	class BackupDirectoryEditDialog;
}

class BackupDirectoryEditDialog : public QDialog
{
	Q_OBJECT

public:
	explicit BackupDirectoryEditDialog(QWidget *parent = 0);
	~BackupDirectoryEditDialog();

public:
	void show(int rowId);

private slots:
	void on_btnOk_clicked();
	void on_btnCancel_clicked();
	void on_btnSourceFolder_clicked();
	void on_btnBackupFolder_clicked();

private:
	Ui::BackupDirectoryEditDialog *ui;
	int rowId_;

};

#endif // BACKUPDIRECTORYEDITDIALOG_H
