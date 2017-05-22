extern WEAK_IMPORT int somefunction();
extern void indirect();

#include <stdio.h>

int main()
{
    printf("meow\n");
    if (&somefunction != nullptr)
        printf("somefunction existed and it returned %d\n", somefunction());
    else
        printf("somefunction did not exist\n");
#if SHOULD_INSTALL_LIB
    indirect();
#endif
    return 0;
}
