#include <testlib.h>

#include <header.h>
#include <iostream>

const char *nameA();

int main()
{
    HelloWorld h(42 + hello());
    std::cout << "Calling: " << nameA() << std::endl;
}
