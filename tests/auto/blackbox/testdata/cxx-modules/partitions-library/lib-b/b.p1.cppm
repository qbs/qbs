
module;
#include "../dllexport.h"
#include <iostream>

export module b:p1;

export LIB_EXPORT void b_p1()
{
    std::cout << "hello from b.part1\n";
}
