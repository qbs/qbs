#include "../dllexport.h"

#include <stdio.h>

DLL_IMPORT void foo(void);

int main(void)
{
    printf("Hello from app\n");
    foo();
    return 0;
}
