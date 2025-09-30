#if defined(WITH_ZLIB)
#include <zlib.h>
#endif

#if defined(WITH_WEBSOCK)
#include <emscripten/websocket.h>
#endif

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
    int result = 0;
#if defined(WITH_ZLIB)
    const char source[] = "Hello, world";
    uLongf buffer_size = compressBound(sizeof(source));
    result += static_cast<int>(buffer_size);
#endif

#if defined(WITH_WEBSOCK)
    const bool supported = emscripten_websocket_is_supported();
    (void) supported;
#endif

#ifdef WITH_PTHREAD
    pthread_t self = pthread_self();
    result += static_cast<int>(self);
#elif defined(WITH_SETUPAPI)
    CABINET_INFO ci;
    ci.SetId = 0;
    SetupIterateCabinet(L"invalid-file-path", 0, NULL, NULL);
    result += ci.SetId;
#endif
    return result;
}
