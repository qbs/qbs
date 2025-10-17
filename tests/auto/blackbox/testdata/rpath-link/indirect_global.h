#pragma once

#if defined(INDIRECT_LIBRARY)
#define INDIRECT_EXPORT __attribute__((visibility("default")))
#else
#define INDIRECT_EXPORT __attribute__((visibility("default")))
#endif
