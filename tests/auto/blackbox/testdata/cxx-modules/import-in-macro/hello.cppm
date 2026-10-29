module;

#include <iostream>

export module helloworld;

#ifdef OPTIONAL_MODULE
import optional_module;
#endif

export void hello()
{
    std::cout << "Hello world!\n";
}
