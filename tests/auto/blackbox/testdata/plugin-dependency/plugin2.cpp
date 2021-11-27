#include "../dllexport.h"
#include <cstdio>

DLL_EXPORT void plugin2_hello()
{
    std::puts("plugin2 says hello!");
}
