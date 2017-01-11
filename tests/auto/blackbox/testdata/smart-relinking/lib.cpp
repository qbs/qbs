#include <cstdio>

__attribute__ ((visibility ("default"))) void publicFunc()
{
#ifdef PRINTF
    std::printf("Tach\n");
#endif
}

__attribute__ ((visibility ("hidden"))) void privateFunc()
{
}

#ifdef PRIV2
__attribute__ ((visibility ("hidden"))) void privateFunc2()
{
}
#endif

#ifdef PUB2
__attribute__ ((visibility ("default"))) void publicFunc2()
{
}
#endif
