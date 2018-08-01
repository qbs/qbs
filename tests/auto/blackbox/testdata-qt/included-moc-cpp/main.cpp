#include "myobject.h"

int main()
{
    MyObject o;
    QObject::connect(&o, &QObject::destroyed, [] { });
}
