#ifdef __DMC__
#include <windows.h>
#define EXPORT_FUN _export
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    return TRUE;
}
#else
#define EXPORT_FUN
#endif

#if defined(_WIN32) || defined(WIN32)
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif

DLL_EXPORT void EXPORT_FUN foo(void)
{
}
