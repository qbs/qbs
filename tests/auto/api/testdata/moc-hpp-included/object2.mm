#include "object2.h"

Object2::Object2(QObject *parent)
    : QObject(parent)
{}

#include "moc_object2.cpp"
#include <cstdio>

int main2()
{
    Object2 obj;
    printf("Hello World\n");
    return 0;
}

