#ifndef OBJECT_H
#define OBJECT_H
#include <QObject>

class Object : public QObject
{
    Q_OBJECT
public:
    Object(QObject *parent = 0);
};

#endif

