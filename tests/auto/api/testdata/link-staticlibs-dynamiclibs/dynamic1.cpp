#include "static2.h"
#include <stdio.h>

#if defined(_WIN32) || defined(WIN32)
#   define EXPORT __declspec(dllexport)
#else
#   define EXPORT
#endif

EXPORT int dynamic1_hello()
{
    TestMe tm;
    tm.hello();
    puts("dynamic1 says hello!");
    return 1;
}
