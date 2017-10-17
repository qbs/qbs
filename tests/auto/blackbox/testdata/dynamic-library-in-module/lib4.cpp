#include "../dllexport.h"

DLL_IMPORT void theLibFunc();

DLL_EXPORT void theFourthLibFunc()
{
    theLibFunc();
}
