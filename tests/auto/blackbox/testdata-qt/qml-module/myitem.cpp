#include "myitem.h"

#include <cstdio>

MyItem::MyItem(QObject *parent)
    : QObject(parent)
{
    puts("MyItem created");
}
