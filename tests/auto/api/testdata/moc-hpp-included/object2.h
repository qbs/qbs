#ifndef OBJECT2_H
#define OBJECT2_H
#include <QObject>

class Object2 : public QObject
{
    Q_OBJECT
public:
    Object2(QObject *parent = 0);
};

#endif

