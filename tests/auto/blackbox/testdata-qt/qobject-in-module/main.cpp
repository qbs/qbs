#include <QCoreApplication>

import MyQObjectModule;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    MyQObject o;
    o.start();
    return app.exec();
}
