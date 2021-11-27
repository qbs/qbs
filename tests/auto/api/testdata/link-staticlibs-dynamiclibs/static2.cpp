#include "../dllexport.h"
#include "static2.h"
#include <cstdio>

DLL_IMPORT void dynamic2_hello();

void TestMe::hello() const
{
    dynamic2_hello();
    std::puts("static2 says hello!");
}
