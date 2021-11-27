#include "../dllexport.h"

#include <cstdio>

DLL_IMPORT int dynamic1_hello();

void static1_hello()
{
    int n = dynamic1_hello();
    std::printf("static%d says hello!\n", n);
}
