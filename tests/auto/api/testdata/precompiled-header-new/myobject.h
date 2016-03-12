#ifndef MYOBJECT_H
#define MYOBJECT_H

#include <QtCore/QObject>

class MyObject : public QObject
{
    Q_OBJECT
public:
    MyObject();
    ~MyObject();
};

#endif

