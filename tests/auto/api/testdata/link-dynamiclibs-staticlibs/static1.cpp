#include <stdio.h>

#if defined(_WIN32) || defined(WIN32)
#   define IMPORT __declspec(dllimport)
#else
#   define IMPORT
#endif

IMPORT void dynamic2_hello();

void static1_hello()
{
    dynamic2_hello();
    puts("static1 says hello!");
}
