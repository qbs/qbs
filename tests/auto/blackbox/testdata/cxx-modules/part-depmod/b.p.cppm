
module;
#include <iostream>

export module b:p;

import a;

export void bar()
{
    foo();
    std::cout << "hello from b.p\n";
}
