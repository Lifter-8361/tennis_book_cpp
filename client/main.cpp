#include "mainwidget.h"

#include <iostream>
#include <QApplication>

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "Russian");
    QApplication app(argc, argv);

    app.setOrganizationName(QString::fromUtf8("Ssipta"));
    app.setApplicationName(QString::fromUtf8("book_tennis_client"));

    MainWidget w;
    w.show();
    return app.exec();
}
