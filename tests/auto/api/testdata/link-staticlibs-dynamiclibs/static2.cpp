#include "static2.h"
#include <stdio.h>

#if defined(_WIN32) || defined(WIN32)
#   define IMPORT __declspec(dllimport)
#else
#   define IMPORT
#endif

IMPORT void dynamic2_hello();

void TestMe::hello() const
{
    dynamic2_hello();
    puts("static2 says hello!");
}
