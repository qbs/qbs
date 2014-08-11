#include <iostream>

extern "C" void printHelloCpp()
{
    std::cout << "Hello from C++ in " << __FILE__ << std::endl;
}
