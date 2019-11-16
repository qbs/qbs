#include "../dllexport.h"

DLL_IMPORT void lib1Func();
DLL_IMPORT void lib2Func();
DLL_IMPORT void lib3Func();

int main()
{
    lib1Func();
    lib2Func();
    lib3Func();
    return 0;
}
