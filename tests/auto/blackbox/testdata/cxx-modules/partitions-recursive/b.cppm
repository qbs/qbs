
module;
#include <iostream>

export module b;

import :p2; // private partition
export import :p1;  // public partition

import a;

export void b()
{
    b_p1();
    b_p2();
    std::cout << "hello from module b\n";
    // these are also visible as they are public in module a
    a_p1();
    a();
}
