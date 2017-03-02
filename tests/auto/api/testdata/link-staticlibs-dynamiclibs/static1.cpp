#include <stdio.h>

#if defined(_WIN32) || defined(WIN32)
#   define IMPORT __declspec(dllimport)
#else
#   define IMPORT
#endif

IMPORT int dynamic1_hello();

void static1_hello()
{
    int n = dynamic1_hello();
    printf("static%d says hello!\n", n);
}
