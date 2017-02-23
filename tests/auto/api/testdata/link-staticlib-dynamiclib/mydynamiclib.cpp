#if defined(_WIN32) || defined(WIN32)
#    define LIB_EXPORT __declspec(dllexport)
#else
#    define LIB_EXPORT __attribute__((visibility("default")))
#endif

LIB_EXPORT int dynamic_foo() { return 12; }
