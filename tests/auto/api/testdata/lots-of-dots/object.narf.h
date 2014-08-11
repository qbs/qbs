#ifndef OBJECT_H
#define OBJECT_H
#include <QObject>

class ObjectNarf : public QObject
{
    Q_OBJECT
public:
    ObjectNarf(QObject *parent = 0);
};

#endif

