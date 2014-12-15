#if defined(_WIN32) || defined(WIN32)
#   define EXPORT __declspec(dllexport)
#else
#   define EXPORT
#endif

EXPORT int getAnswer() { return 42; }
