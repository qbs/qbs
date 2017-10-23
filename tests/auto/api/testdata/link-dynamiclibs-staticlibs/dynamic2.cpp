#include "../dllexport.h"
#include "static2.h"
#include <stdio.h>

DLL_EXPORT void dynamic2_hello()
{
    TestMe tm;
    tm.hello();
    puts("dynamic2 says hello!");
}
