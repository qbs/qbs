#include <stdio.h>

#if defined(_WIN32) || defined(WIN32)
#   define IMPORT __declspec(dllimport)
#else
#   define IMPORT
#endif

IMPORT int dynamic1_hello();

int main()
{
    int result = dynamic1_hello();
    puts("application says hello!");
    return result;
}
