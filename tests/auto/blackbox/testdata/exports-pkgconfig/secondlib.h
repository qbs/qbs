#include "../dllexport.h"

#ifdef SECONDLIB
#   define SECONDLIB_EXPORT DLL_EXPORT
#else
#   define SECONDLIB_EXPORT DLL_IMPORT
#endif

SECONDLIB_EXPORT void secondLib();
