#ifndef FOO_GLOBAL_H
#define FOO_GLOBAL_H

#if defined(_WIN32) || defined(WIN32)
#define FOO_DECL_EXPORT __declspec(dllexport)
#define FOO_DECL_IMPORT __declspec(dllimport)
#else
#define FOO_DECL_EXPORT __attribute__((visibility("default")))
#define FOO_DECL_IMPORT __attribute__((visibility("default")))
#endif

#if defined(FOO_STATIC_LIBRARY)
#define FOO_EXPORT
#else
#if defined(FOO_LIBRARY)
#define FOO_EXPORT FOO_DECL_EXPORT
#else
#define FOO_EXPORT FOO_DECL_IMPORT
#endif
#endif

#endif // FOO_GLOBAL_H
