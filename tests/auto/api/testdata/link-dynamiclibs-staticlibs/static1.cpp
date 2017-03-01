#include <stdio.h>

#if defined(_WIN32) || defined(WIN32)
#   define EXPORT __declspec(dllexport)
#   define IMPORT __declspec(dllimport)
#else
#   define EXPORT __attribute__((visibility("default")))
#   define IMPORT
#endif

IMPORT void dynamic2_hello();

EXPORT void static1_hello()
{
    dynamic2_hello();
    puts("static1 says hello!");
}
