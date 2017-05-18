#include <stdio.h>

#if defined(_WIN32) || defined(WIN32)
#   define EXPORT __declspec(dllexport)
#else
#   define EXPORT
#endif

EXPORT void helper_hello()
{
    puts("helper says hello!");
}
