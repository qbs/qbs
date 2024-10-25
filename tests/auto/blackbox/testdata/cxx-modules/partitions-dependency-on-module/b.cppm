
module;
#include <iostream>

export module b;

export import :p;

export void baz()
{
    bar();
    std::cout << "hello from module b\n";
}
