#include <stdio.h>

extern void printHelloC()
{
    printf("Hello from C in " __FILE__ "\n");
}
