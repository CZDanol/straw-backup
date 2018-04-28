#include <QApplication>

#include "global.h"

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);

	global = new Global;
	global->init();

	int result = app.exec();

	global->uninit();

	delete global;

	return result;
}
