#include "../dllexport.h"
#include <cstdio>

DLL_IMPORT int dynamic1_hello();

int main()
{
    int result = dynamic1_hello();
    std::puts("application says hello!");
    return result;
}
