module;
#include <iostream>

export module c;
export import b;

export void baz()
{
    std::cout << "hello c" << std::endl;
}