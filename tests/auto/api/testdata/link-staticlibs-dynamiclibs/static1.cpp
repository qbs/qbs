#include "../dllexport.h"
#include <stdio.h>

DLL_IMPORT int dynamic1_hello();

void static1_hello()
{
    int n = dynamic1_hello();
    printf("static%d says hello!\n", n);
}
