#include "../dllexport.h"
#include <cstdio>

DLL_EXPORT void plugin3_hello()
{
    std::puts("plugin3 says hello!");
}
