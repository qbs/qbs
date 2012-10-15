#include "object.h"
#include <cstdio>

Object::Object(QObject *parent)
    : QObject(parent)
{}

int main()
{
    Object obj;
    printf("Hello World\n");
}

