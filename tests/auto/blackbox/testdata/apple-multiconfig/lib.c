#include <stdio.h>

int foo()
{
#ifdef __i386__
    printf("Hello from " VARIANT " i386\n");
#endif
#ifdef __x86_64__
    printf("Hello from " VARIANT " x86_64\n");
#endif
    return 0;
}
