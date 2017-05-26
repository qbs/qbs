#import <Foundation/Foundation.h>

int main()
{
    NSDictionary *infoPlist = [[NSBundle mainBundle] infoDictionary];
    BOOL success = [[infoPlist objectForKey:@"QBS"] isEqualToString:@"org.qt-project.qbs.testdata.embedInfoPlist"];
    fprintf(success ? stdout : stderr, "%s\n", [[infoPlist description] UTF8String]);
    return success ? 0 : 1;
}
