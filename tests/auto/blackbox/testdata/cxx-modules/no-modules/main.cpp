#include <iostream>

const auto s = R"(
import NotAModule;
)";

static void import() {}
static void module() {}
static void foo()
{
    const char import[] = "hello";
    const char module[] = "world";
    std::cout << import << ' ' << module << std::endl;
}
static int bar()
{
    const int import = 0;
    const int module = 0;
    return import + module;
}
static int bar2()
{
    int import;
    int module;
    import = 0;
    module = 0;
    return import + module;
}

int main()
{
    import();
    module();
    foo();
    return bar() + bar2();
}
