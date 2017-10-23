#include <stdio.h>

#include "../dllexport.h"

void static1_hello();

DLL_EXPORT int dynamic1_hello()
{
    static1_hello();
    puts("dynamic1 says hello!");
    return 0;
}
