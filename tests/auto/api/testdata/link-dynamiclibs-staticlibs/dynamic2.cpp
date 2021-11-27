#include "../dllexport.h"
#include "static2.h"
#include <cstdio>

DLL_EXPORT void dynamic2_hello()
{
    TestMe tm;
    tm.hello();
    std::puts("dynamic2 says hello!");
}
