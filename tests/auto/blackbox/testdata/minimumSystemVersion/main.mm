/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <iostream>
#import <Foundation/Foundation.h>

int main()
{
    // This gets set by -mmacosx-version-min. If left undefined, defaults to the current OS version.
    std::cout << "__MAC_OS_X_VERSION_MIN_REQUIRED="
              << __MAC_OS_X_VERSION_MIN_REQUIRED << std::endl;

    bool print = false;
    NSTask *task = [[NSTask alloc] init];
    [task setLaunchPath:@"/usr/bin/otool"];
    [task setArguments:[NSArray arrayWithObjects:@"-l", [[[NSProcessInfo processInfo] arguments] firstObject], nil]];
    NSPipe *pipe = [NSPipe pipe];
    [task setStandardOutput:pipe];
    [task launch];
    NSData *data = [[pipe fileHandleForReading] readDataToEndOfFile];
    [task waitUntilExit];
    NSString *result = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    NSString *line;
    NSEnumerator *enumerator = [[result componentsSeparatedByString:@"\n"] objectEnumerator];
    while ((line = [enumerator nextObject]) != nil) {
        if ([line rangeOfString:@"LC_VERSION_MIN_MACOSX"].location != NSNotFound)
            print = true;

        if (print && [line rangeOfString:@"version"].location != NSNotFound) {
            std::cout << [[line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] UTF8String] << std::endl;
            print = false;
            continue;
        }

#ifdef __clang__
#if __clang_major__ >= 10 && __clang_minor__ >= 0
        if ([line rangeOfString:@"LC_BUILD_VERSION"].location != NSNotFound)
            print = true;

        if (print && [line rangeOfString:@"minos"].location != NSNotFound) {
            std::cout << [[line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] UTF8String] << std::endl;
            print = false;
        }
#endif
#endif // __clang__
    }

}
