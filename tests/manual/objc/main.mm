#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>
#include <QtCore/QCoreApplication>

int main(int argc, char **argv)
{
    // We support both C++
    QCoreApplication app(argc, argv);
    // And Objective-C
    SInt32 majorVersion, minorVersion, bugFix;
    Gestalt(gestaltSystemVersionMajor, &majorVersion);
    Gestalt(gestaltSystemVersionMinor, &minorVersion);
    Gestalt(gestaltSystemVersionBugFix, &bugFix);
    NSLog(@"Hello, MacOS %d.%d.%d!", majorVersion, minorVersion, bugFix);
    // So it's Objective-C++
}
