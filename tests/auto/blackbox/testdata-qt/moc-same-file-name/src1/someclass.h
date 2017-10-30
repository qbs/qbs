#ifndef SOMECLASS1_H
#define SOMECLASS1_H

#include <QObject>

namespace Src1 {

class SomeClass : public QObject
{
    Q_OBJECT
public:
    SomeClass(QObject *parent = nullptr);
};

}

#endif
