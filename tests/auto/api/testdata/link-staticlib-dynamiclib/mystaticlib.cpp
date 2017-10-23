#include "../dllexport.h"

DLL_IMPORT int dynamic_foo();

int static_foo() { return dynamic_foo() * 13; }
