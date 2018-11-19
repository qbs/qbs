#ifndef CREATEQTCLASS_H
#define CREATEQTCLASS_H

#include <QObject>

#define CREATE_QT_CLASS(className) \
class className : public QObject \
{ \
    Q_OBJECT \
public: \
    Q_SIGNAL void mySignal(); \
}

#endif
