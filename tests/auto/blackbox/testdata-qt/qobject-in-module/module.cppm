module;

#include "../dllexport.h"
#include <module.moc.h>

#include <QCoreApplication>
#include <QObject>
#include <QTimer>

#include <iostream>

export module MyQObjectModule;

export class DLL_EXPORT MyQObject : public QObject
{
    Q_OBJECT
public:
    MyQObject() = default;

    void start()
    {
        QTimer::singleShot(0, this, [] {
            std::cout << "queued hello";
            qApp->quit();
        });
    }
};

#include <module.moc.data>
