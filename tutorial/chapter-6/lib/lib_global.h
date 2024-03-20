#ifndef LIB_GLOBAL_H
#define LIB_GLOBAL_H

#if defined(_WIN32) || defined(WIN32)
#define MYLIB_DECL_EXPORT __declspec(dllexport)
#define MYLIB_DECL_IMPORT __declspec(dllimport)
#else
#define MYLIB_DECL_EXPORT __attribute__((visibility("default")))
#define MYLIB_DECL_IMPORT __attribute__((visibility("default")))
#endif

#if defined(MYLIB_LIBRARY)
#define MYLIB_EXPORT MYLIB_DECL_EXPORT
#else
#define MYLIB_EXPORT MYLIB_DECL_IMPORT
#endif

#endif // LIB_GLOBAL_H
