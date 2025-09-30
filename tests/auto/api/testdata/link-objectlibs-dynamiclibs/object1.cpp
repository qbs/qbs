#include "../dllexport.h"

#include <cstdio>

DLL_IMPORT int dynamic1_hello();

void object1_hello()
{
    int n = dynamic1_hello();
    std::printf("object%d says hello!\n", n);
}
