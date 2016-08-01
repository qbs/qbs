#include <iostream>

extern "C" int cpp();

int cpp()
{
    std::cout << "Hello world" << std::endl;
    return 0;
}
