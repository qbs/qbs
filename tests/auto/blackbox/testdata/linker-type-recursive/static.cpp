#include <iostream>

extern "C" {
void funcCxx()
{
    std::cout << "Hi!" << std::endl;
}
}
