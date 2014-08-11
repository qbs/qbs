
#ifndef ECHOINTERFACE_H
#define ECHOINTERFACE_H

#include <QString>

//! [0]
class EchoInterface
{
public:
    virtual ~EchoInterface() {}
    virtual QString echo(const QString &message) = 0;
};


QT_BEGIN_NAMESPACE

#define EchoInterface_iid "org.qt-project.Qt.Examples.EchoInterface"

Q_DECLARE_INTERFACE(EchoInterface, EchoInterface_iid)
QT_END_NAMESPACE

//! [0]
#endif
