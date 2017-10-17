#include "../dllexport.h"
#include <iostream>

DLL_EXPORT void theLibFunc()
{
    std::cout << "Hello from thelib!" << std::endl;
}
