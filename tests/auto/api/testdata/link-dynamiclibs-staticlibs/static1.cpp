#include "../dllexport.h"
#include <stdio.h>

DLL_IMPORT void dynamic2_hello();

void static1_hello()
{
    dynamic2_hello();
    puts("static1 says hello!");
}
