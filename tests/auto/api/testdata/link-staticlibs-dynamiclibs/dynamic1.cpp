#include "../dllexport.h"
#include "static2.h"
#include <stdio.h>

DLL_EXPORT int dynamic1_hello()
{
    TestMe tm;
    tm.hello();
    puts("dynamic1 says hello!");
    return 1;
}
