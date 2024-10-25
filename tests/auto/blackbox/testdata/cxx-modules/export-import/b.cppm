module;
#include <iostream>

export module b;
export import a;

export void bar()
{
    std::cout << "hello b" << std::endl;
}