#include <QApplication>
#include "mainwidget.h"
#include <iostream>


int main(int argc, char* argv[])
{
	setlocale(LC_ALL, "Russian");
	QApplication app(argc, argv);

	app.setOrganizationName(QString::fromUtf8("Ssipta"));
	app.setApplicationName(QString::fromUtf8("book_tennis_server"));

	MainWidget w;
	w.setGeometry(300,300,500,500);
	w.show();
	return app.exec();
}