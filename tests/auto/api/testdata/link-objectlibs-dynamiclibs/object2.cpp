#include "object2.h"
#include "../dllexport.h"
#include <cstdio>

DLL_IMPORT void dynamic2_hello();

void TestMe::hello() const
{
    dynamic2_hello();
    std::puts("object2 says hello!");
}
