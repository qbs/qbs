#include <iostream>

static void import() {}
static void foo()
{
    const char import[] = "hello";
    std::cout << import << std::endl;
}
static int bar()
{
    const int import = 0;
    return import;
}
static int bar2()
{
    int import;
    import = 0;
    return import;
}

int main()
{
    import();
    foo();
    return bar() + bar2();
}
