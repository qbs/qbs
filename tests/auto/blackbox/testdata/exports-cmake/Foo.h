#ifndef FOO_H
#define FOO_H
#include <dllexport.h>

#ifdef FOO_LIB_STATIC
#define FOO_LIB_EXPORT
#else
#ifdef FOO_LIB
#define FOO_LIB_EXPORT DLL_EXPORT
#else
#define FOO_LIB_EXPORT DLL_IMPORT
#endif
#endif

FOO_LIB_EXPORT int someFooWork();
#endif // FOO_H
