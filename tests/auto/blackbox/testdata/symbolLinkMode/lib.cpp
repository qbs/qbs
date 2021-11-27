int somefunction()
{
    return 42;
}

#include <cstdio>

static const auto func = []() {
    std::printf("Lib was loaded!\n");
    return 0;
}();
