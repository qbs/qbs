#ifndef DIRECT_GLOBAL_H
#define DIRECT_GLOBAL_H

#if defined(DIRECT_LIBRARY)
#define DIRECT_EXPORT __attribute__((visibility("default")))
#else
#define DIRECT_EXPORT __attribute__((visibility("default")))
#endif

#endif // DIRECT_GLOBAL_H
