#ifdef WITH_PTHREAD
#include <pthread.h>
#elif defined(WITH_SETUPAPI)
#include <windows.h>
#include <Setupapi.h>
#endif

void b();
void c();

int d()
{
    b();
    c();

#ifdef WITH_PTHREAD
    pthread_t self = pthread_self();
    return static_cast<int>(self);
#elif defined(WITH_SETUPAPI)
    CABINET_INFO ci;
    ci.SetId = 0;
    return ci.SetId;
#else
    return 0;
#endif
}
