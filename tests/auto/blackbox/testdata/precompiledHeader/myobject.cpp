#include <iostream>
#include "myobject.h"

MyObject::MyObject()
{
    std::cout << "MyObject::MyObject()\n";
}

MyObject::~MyObject()
{
    std::cout << "MyObject::~MyObject()" << std::endl;
}

