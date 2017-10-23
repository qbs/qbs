#include "../dllexport.h"
#include <stdio.h>

DLL_IMPORT int dynamic1_hello();

int main()
{
    int result = dynamic1_hello();
    puts("application says hello!");
    return result;
}
