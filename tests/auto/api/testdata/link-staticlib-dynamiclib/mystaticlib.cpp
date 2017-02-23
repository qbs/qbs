#if defined(_WIN32) || defined(WIN32)
#   define EXPORT __declspec(dllexport)
#   define IMPORT __declspec(dllimport)
#else
#   define EXPORT
#   define IMPORT
#endif

IMPORT int dynamic_foo();

int static_foo() { return dynamic_foo() * 13; }
