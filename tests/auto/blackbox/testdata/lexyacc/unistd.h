#ifndef MY_UNISTD_H
#define MY_UNISTD_H

#ifdef _MSC_BUILD
#include <io.h>
#define isatty _isatty
#else
#include_next <unistd.h>
#endif

#endif
