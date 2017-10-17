#include "../dllexport.h"
#include <iostream>

DLL_EXPORT void theOtherLibFunc()
{
    std::cout << "Hello from theotherlib!" << std::endl;
}
