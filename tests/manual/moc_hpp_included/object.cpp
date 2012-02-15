#include "object.h"

Object::Object(QObject *parent)
    : QObject(parent)
{}

#include "moc_object.cpp"
#include <cstdio>

int main()
{
    Object obj;
    printf("Hello World\n");
}

