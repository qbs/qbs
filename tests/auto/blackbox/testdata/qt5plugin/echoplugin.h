
#ifndef ECHOPLUGIN_H
#define ECHOPLUGIN_H

#include <QObject>
#include <QtPlugin>
#include "echointerface.h"

class EchoPlugin : public QObject, EchoInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.Examples.EchoInterface" FILE "echoplugin.json")
    Q_INTERFACES(EchoInterface)

public:
    QString echo(const QString &message);
};

#endif
