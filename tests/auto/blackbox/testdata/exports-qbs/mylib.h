#if defined(_WIN32) || defined(WIN32)
#    define DLL_EXPORT __declspec(dllexport)
#    define DLL_IMPORT __declspec(dllimport)
#else
#    define DLL_EXPORT __attribute__((visibility("default")))
#    define DLL_IMPORT __attribute__((visibility("default")))
#  endif

#ifdef MYLIB_BUILD
#define MYLIB_EXPORT DLL_EXPORT
#else
#define MYLIB_EXPORT DLL_IMPORT
#endif

namespace MyLib {
MYLIB_EXPORT void f();
}
