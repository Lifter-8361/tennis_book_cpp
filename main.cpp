#include "mainwidget.h"

#include <iostream>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setOrganizationName(QString::fromUtf8("Ssipta"));
    a.setApplicationName(QString::fromUtf8("book_tennis_client"));

    MainWidget w;
    w.show();
    return a.exec();
}
