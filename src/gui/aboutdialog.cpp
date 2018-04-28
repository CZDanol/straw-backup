#include "aboutdialog.h"
#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::AboutDialog)
{
	ui->setupUi(this);
	ui->lblVersion->setText( tr("Verze: %1").arg( PROGRAM_VERSION ) );
}

AboutDialog::~AboutDialog()
{
	delete ui;
}
