#include "../dllexport.h"
#include <cstdio>

DLL_IMPORT void dynamic2_hello();

void static1_hello()
{
    dynamic2_hello();
    std::puts("static1 says hello!");
}
