module;

#include "../dllexport.h"
#include <iostream>

export module a:p1;

export LIB_EXPORT void a_p1()
{
    std::cout << "hello from a.part1\n";
}
