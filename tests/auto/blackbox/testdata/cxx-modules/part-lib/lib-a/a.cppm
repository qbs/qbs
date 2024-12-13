
module;

#include "../dllexport.h"
#include <iostream> // workaround for gcc bug

export module a;

import :p2; // private partition
export import :p1;  // public partition

export LIB_EXPORT void a()
{
    a_p1();
    a_p2();
    std::cout << "hello from module a\n";
}