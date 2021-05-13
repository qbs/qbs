#include "libA.h"

#include <iostream>

void foo()
{
    std::cout << "hello from foo: ";
#ifdef MYLIB_FRAMEWORK
    std::cout << "bundled: yes";
#else
    std::cout << "bundled: no";
#endif
    std::cout << std::endl;
}
