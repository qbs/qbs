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

#ifdef _WIN32
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#endif

int main(int argc, char *argv[])
{
    if (argc != 1)
        return 1;

#ifdef _WIN32
#if defined(__clang__)
    std::cout << "Unsupported compiler" << std::endl;
    return 0;
#endif
#if defined(WINVER) && defined(QBS_WINVER)
    std::cout << "WINVER=" << WINVER << std::endl;
    std::string command = TOOLCHAIN_INSTALL_PATH;
    std::replace(command.begin(), command.end(), '/', '\\');
    command = "\"\"" + command;
#ifdef __GNUC__
    command += "\\objdump.exe\" -p \"";
#else
    command += "\\dumpbin.exe\" /HEADERS \"";
#endif
    command += argv[0];
    command += "\" > qbs-test-dumpbin.txt\"";
    int status = ::system(command.c_str());
    if (status != 0)
        return status;

    std::ifstream in("qbs-test-dumpbin.txt");
    std::string s;
    while (std::getline(in, s)) {
#ifdef __GNUC__
        static const char *majorOSystemVersion = "MajorOSystemVersion\t";
        if (s.find(majorOSystemVersion) != std::string::npos)
            std::cout << s.substr(std::strlen(majorOSystemVersion));

        static const char *minorOSystemVersion = "MinorOSystemVersion\t";
        if (s.find(minorOSystemVersion) != std::string::npos)
            std::cout << ".0" << s.substr(std::strlen(minorOSystemVersion))
                      << " operating system version" << std::endl;

        static const char *majorSubsystemVersion = "MajorSubsystemVersion\t";
        if (s.find(majorSubsystemVersion) != std::string::npos)
            std::cout << s.substr(std::strlen(majorSubsystemVersion));

        static const char *minorSubsystemVersion = "MinorSubsystemVersion\t";
        if (s.find(minorSubsystemVersion) != std::string::npos)
            std::cout << ".0" << s.substr(std::strlen(minorSubsystemVersion))
                      << " subsystem version" << std::endl;
#else
        if (s.find("operating system version") != std::string::npos ||
            s.find("subsystem version") != std::string::npos) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                    std::not1(std::ptr_fun<int, int>(std::isspace))));
            std::cout << s << std::endl;
        }
#endif
    }

    unlink("qbs-test-dumpbin.txt");
#else
    std::cout << "WINVER is not defined" << std::endl;
#endif
#endif

    return 0;
}
