#include "../dllexport.h"
#include <cstdio>

DLL_EXPORT void plugin1_hello()
{
    std::puts("plugin1 says hello!");
}
