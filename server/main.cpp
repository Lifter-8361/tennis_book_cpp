#include <QApplication>
#include "mainwidget.h"

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	MainWidget w;
	w.setGeometry(300,300,500,500);
	w.show();
	return app.exec();
}