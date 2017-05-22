int somefunction()
{
    return 42;
}

#include <stdio.h>

static const auto func = []() {
    printf("Lib was loaded!\n");
    return 0;
}();
