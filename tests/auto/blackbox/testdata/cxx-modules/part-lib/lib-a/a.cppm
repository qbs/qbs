module;

#include "../dllexport.h"

export module a;
export import :p1;  // public partition

export LIB_EXPORT void a();
