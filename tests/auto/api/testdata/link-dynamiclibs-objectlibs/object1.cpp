#include "../dllexport.h"
#include <cstdio>

DLL_IMPORT void dynamic2_hello();

void object1_hello()
{
    dynamic2_hello();
    std::puts("object1 says hello!");
}
