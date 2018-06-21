#include <QApplication>

#include <QStyleFactory>
#include <QFile>

#include "global.h"

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);

	// Setup stylesheet
	{
		qApp->setStyle(QStyleFactory::create("Fusion"));

		QPalette p = qApp->palette();
		p.setColor(QPalette::Light, QColor("#ddd") );
		qApp->setPalette(p);

		QFile f( ":/stylesheet.css" );
		f.open( QFile::ReadOnly );
		qApp->setStyleSheet( QString( f.readAll() ) );
	}

	global = new Global;
	global->init();

	int result = app.exec();

	global->uninit();

	delete global;

	return result;
}
