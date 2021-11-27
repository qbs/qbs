extern WEAK_IMPORT int somefunction();
extern void indirect();

#include <cstdio>

int main()
{
    std::printf("meow\n");
    if (&somefunction != nullptr)
        std::printf("somefunction existed and it returned %d\n", somefunction());
    else
        std::printf("somefunction did not exist\n");
#if SHOULD_INSTALL_LIB
    indirect();
#endif
    return 0;
}
