//![0]
// lib/hello.cppm
module;

#include "lib_global.h"
#include <iostream>
#include <string_view>

export module hello;

export namespace Hello {

void MYLIB_EXPORT printHello(std::string_view name)
{
    std::cout << "Hello, " << name << '!' << std::endl;
}

} // namespace Hello
//![0]