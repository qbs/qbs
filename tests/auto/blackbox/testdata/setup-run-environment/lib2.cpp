#include "../dllexport.h"
DLL_IMPORT void lib5Func();

DLL_EXPORT void lib2Func()
{
    lib5Func();
}
