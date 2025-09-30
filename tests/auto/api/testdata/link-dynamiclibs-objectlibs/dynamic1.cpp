#include <cstdio>

#include "../dllexport.h"

void object1_hello();

DLL_EXPORT int dynamic1_hello()
{
    object1_hello();
    std::puts("dynamic1 says hello!");
    return 0;
}
