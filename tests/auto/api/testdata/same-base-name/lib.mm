#include <iostream>
#import <Foundation/Foundation.h>

extern "C" void printHelloObjcpp()
{
    NSLog(@"Hello from Objective-C++...");
    std::cout << "...in " __FILE__ << std::endl;
}
