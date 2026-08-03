
module;
#include "../dllexport.h"
#include <iostream>

export module b;

export import :p1;  // public partition

export LIB_EXPORT void b();
