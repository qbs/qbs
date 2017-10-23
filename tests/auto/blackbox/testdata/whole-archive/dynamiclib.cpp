#include "../dllexport.h"

void usedFunc();

DLL_EXPORT void f() { usedFunc(); }
