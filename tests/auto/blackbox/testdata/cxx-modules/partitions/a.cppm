
module;
#include <iostream>

export module a;

export import :p1;  // public partition

export void baz()
{
    foo();
    std::cout << "baz from module\n";
}
