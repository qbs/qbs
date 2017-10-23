#include "../dllexport.h"
#include "static2.h"
#include <stdio.h>

DLL_IMPORT void dynamic2_hello();

void TestMe::hello() const
{
    dynamic2_hello();
    puts("static2 says hello!");
}
