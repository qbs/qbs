#include "myqtclass.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    MyQtClass c;
    QObject::connect(&c, &MyQtClass::mySignal, [] { qDebug() << "Hello from slot"; qApp->quit(); });
    QTimer::singleShot(0, &c, &MyQtClass::mySignal);
    return app.exec();
}
