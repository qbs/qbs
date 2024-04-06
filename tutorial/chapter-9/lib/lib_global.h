#ifndef LIB_GLOBAL_H
#define LIB_GLOBAL_H

#if defined(_WIN32) || defined(WIN32)
#define MY_LIB_DECL_EXPORT __declspec(dllexport)
#define MY_LIB_DECL_IMPORT __declspec(dllimport)
#else
#define MY_LIB_DECL_EXPORT __attribute__((visibility("default")))
#define MY_LIB_DECL_IMPORT __attribute__((visibility("default")))
#endif

// ![0]
// lib/lib_global.h
#if defined(MYLIB_STATIC_LIBRARY)
#define MYLIB_EXPORT
#else
#if defined(MYLIB_LIBRARY)
#define MYLIB_EXPORT MY_LIB_DECL_EXPORT
#else
#define MYLIB_EXPORT MY_LIB_DECL_IMPORT
#endif
#endif
// ![0]

#endif // LIB_GLOBAL_H
