#import <Foundation/Foundation.h>
#include <iostream>

int main(int argc, char **argv)
{
    // We support both C++
    std::cout << "Hello from C++" << std::endl;
    // And Objective-C
    NSDictionary *version = [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
    NSString *productVersion = [version objectForKey:@"ProductVersion"];
    NSLog(@"Hello, macOS %@!", productVersion);
    // So it's Objective-C++
}
