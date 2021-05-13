#pragma once

#if defined(_WIN32) || defined(WIN32)
#    define DECL_EXPORT __declspec(dllexport)
#    define DECL_IMPORT __declspec(dllimport)
#else
#    define DECL_EXPORT __attribute__((visibility("default")))
#    define DECL_IMPORT __attribute__((visibility("default")))
#  endif

#if defined(LIBA_STATIC_LIBRARY)
#  define LIBA_EXPORT
#else
#  if defined(MYLIB_LIBRARY)
#    define LIBA_EXPORT DECL_EXPORT
#  else
#    define LIBA_EXPORT DECL_IMPORT
#  endif
#endif

LIBA_EXPORT void foo();
