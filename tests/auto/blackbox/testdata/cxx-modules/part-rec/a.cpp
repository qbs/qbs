module;
#include <iostream>

module a;

import :p2; // private partition

void a()
{
    a_p1();
    a_p2();
    std::cout << "hello from module a\n";
}
