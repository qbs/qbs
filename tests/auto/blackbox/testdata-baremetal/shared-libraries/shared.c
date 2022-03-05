#include "../dllexport.h"

#include <stdio.h>

#ifdef __DMC__
#include <windows.h>
#define EXPORT_FUN _export
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    return TRUE;
}
#else
#define EXPORT_FUN
#endif // __DMC__

DLL_EXPORT void EXPORT_FUN foo(void)
{
    printf("Hello from lib\n");
}
