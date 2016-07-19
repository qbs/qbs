#include <string>
#include <objc/objc.h>

int main()
{
    std::string s = "Hello World";
    (void)s;
    sel_registerName("qbs");
    return 0;
}
