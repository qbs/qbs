
module;
#include <iostream>

export module a;

import :p2; // private partition
export import :p1;  // public partition

export void baz()
{
    foo();
    bar();
    std::cout << "baz from module\n";
}
