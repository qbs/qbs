#include <stdio.h>

#if defined(_WIN32) || defined(WIN32)
#   define EXPORT __declspec(dllexport)
#else
#   define EXPORT
#endif

#ifndef USING_HELPER2
#error define USING_HELPER2 missing
#endif

EXPORT void helper1_hello()
{
    puts("helper1 says hello!");
}
