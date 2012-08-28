#include "foo.h"
#include <stdio.h>

#ifndef HAVE_MAIN_CPP
#   error missing define HAVE_MAIN_CPP
#endif

#ifndef SOMETHING
#   error missing define SOMETHING
#endif

int main()
{
    someUsefulFunction();
#ifdef _DEBUG
    puts("Hello World! (debug version)");
#else
    puts("Hello World! (release version)");
#endif
}

