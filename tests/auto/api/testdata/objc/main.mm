#import <Foundation/Foundation.h>
#include <QCoreApplication>

int main(int argc, char **argv)
{
    // We support both C++
    QCoreApplication app(argc, argv);
    Q_UNUSED(app);
    // And Objective-C
    NSDictionary *version = [NSDictionary dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"];
    NSString *productVersion = [version objectForKey:@"ProductVersion"];
    NSLog(@"Hello, macOS %@!", productVersion);
    // So it's Objective-C++
}
