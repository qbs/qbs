
module;
#include "../dllexport.h"
#include <iostream>

export module b;

import :p2; // private partition
export import :p1;  // public partition

export LIB_EXPORT void b();
