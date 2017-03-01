#include <stdio.h>

#if defined(_WIN32) || defined(WIN32)
#   define EXPORT __declspec(dllexport)
#   define IMPORT __declspec(dllimport)
#else
#   define EXPORT
#   define IMPORT
#endif

IMPORT void static1_hello();

EXPORT int dynamic1_hello()
{
    static1_hello();
    puts("dynamic1 says hello!");
    return 0;
}
