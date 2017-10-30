#ifndef SOMECLASS2_H
#define SOMECLASS2_H

#include <QObject>

namespace Src2 {

class SomeClass : public QObject
{
    Q_OBJECT
public:
    SomeClass(QObject *parent = nullptr);
};

}

#endif
