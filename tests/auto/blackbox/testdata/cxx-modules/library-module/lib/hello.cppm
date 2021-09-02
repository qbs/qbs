module;

#include "dllexport.h"
#include <iostream>

export module helloworld;

export LIB_EXPORT void hello()
{
    std::cout << "Hello world!\n";
}
