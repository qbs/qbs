#include "../dllexport.h"
#include <iostream>

DLL_EXPORT void theThirdLibFunc()
{
    std::cout << "Hello from thethirdlib!" << std::endl;
}
