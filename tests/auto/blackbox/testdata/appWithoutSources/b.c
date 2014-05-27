#include <stdio.h>

int foo();  // defined in a.cpp

int main()
{
    printf("The answer is %d.\n", foo());
    return 0;
}

