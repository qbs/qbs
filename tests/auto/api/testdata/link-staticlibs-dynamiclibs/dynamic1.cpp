#include "../dllexport.h"
#include "static2.h"
#include <cstdio>

DLL_EXPORT int dynamic1_hello()
{
    TestMe tm;
    tm.hello();
    std::puts("dynamic1 says hello!");
    return 1;
}
