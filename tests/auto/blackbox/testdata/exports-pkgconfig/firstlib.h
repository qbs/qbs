#include "../dllexport.h"

#ifdef FIRSTLIB
#   define FIRSTLIB_EXPORT DLL_EXPORT
#else
#   define FIRSTLIB_EXPORT DLL_IMPORT
#endif

FIRSTLIB_EXPORT void firstLib();
