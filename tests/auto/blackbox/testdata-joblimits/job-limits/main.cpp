/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#if defined(_WIN32) || defined(WIN32)
#include <io.h>
#include <sys/locking.h>
#else
#include <unistd.h>
#endif

static bool tryLock(FILE *f)
{
    const int exitCode =
#if defined(_WIN32) || defined(WIN32)
        _locking(_fileno(f), _LK_NBLCK, 10);

#else
        lockf(fileno(f), F_TLOCK, 10);
#endif
    return exitCode == 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "tool needs exactly one argument" << std::endl;
        return 1;
    }

    const std::string lockFilePath = std::string(argv[0]) + ".lock";
    std::FILE * const lockFile = std::fopen(lockFilePath.c_str(), "w");
    if (!lockFile) {
        std::cerr << "cannot open lock file: " << strerror(errno) << std::endl;
        return 2;
    }
    if (!tryLock(lockFile)) {
        if (errno == EACCES || errno == EAGAIN) {
            std::cerr << "tool is exclusive" << std::endl;
            return 3;
        } else {
            std::cerr << "unexpected lock failure: " << strerror(errno) << std::endl;
            fclose(lockFile);
            return 4;
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    fclose(lockFile);
    std::FILE * const output = std::fopen(argv[1], "w");
    if (!output) {
        std::cerr << "cannot create output file: " << strerror(errno) << std::endl;
        return 5;
    }
    fclose(output);
}
