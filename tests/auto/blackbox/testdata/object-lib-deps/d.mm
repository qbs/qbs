#import <Foundation/Foundation.h>

void printGreeting()
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSLog (@"Hello darkness, my old friend!");
    [pool drain];
}
