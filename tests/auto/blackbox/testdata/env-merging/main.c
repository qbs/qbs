#include <stdio.h>
#include <stdlib.h>

int main()
{
    printf("PATH=%s", getenv("PATH"));
    return 0;
}
