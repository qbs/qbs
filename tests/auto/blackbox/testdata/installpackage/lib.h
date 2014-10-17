#include <QtGlobal>

#ifdef MYLIB
#define MYLIB_EXPORT Q_DECL_EXPORT
#else
#define MYLIB_EXPORT Q_DECL_IMPORT
#endif

MYLIB_EXPORT void f();
