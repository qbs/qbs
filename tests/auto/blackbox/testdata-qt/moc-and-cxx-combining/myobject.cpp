#include "myobject.h"

#include <QObject>

class MyObject : public QObject
{
    Q_OBJECT
public:
    void use() {}
};

void useMyObject() { MyObject().use(); }

#include <myobject.moc>
