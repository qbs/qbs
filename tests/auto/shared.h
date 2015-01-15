/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company.  For licensing terms and
** conditions see http://www.qt.io/terms-conditions.  For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/
#ifndef QBS_TEST_SHARED_H
#define QBS_TEST_SHARED_H

#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/settings.h>

#include <QFile>
#include <QtTest>

#if QT_VERSION >= 0x050000
#define SKIP_TEST(message) QSKIP(message)
#else
#define SKIP_TEST(message) QSKIP(message, SkipAll)
#endif

inline void waitForNewTimestamp()
{
    // Waits for the time that corresponds to the host file system's time stamp granularity.
    if (qbs::Internal::HostOsInfo::isWindowsHost())
        QTest::qWait(1);        // NTFS has 100 ns precision. Let's ignore exFAT.
    else
        QTest::qWait(1000);
}

inline void touch(const QString &fn)
{
    QFile f(fn);
    int s = f.size();
    if (!f.open(QFile::ReadWrite))
        qFatal("cannot open file %s", qPrintable(fn));
    f.resize(s+1);
    f.resize(s);
}

inline void copyFileAndUpdateTimestamp(const QString &source, const QString &target)
{
    QFile::remove(target);
    if (!QFile::copy(source, target))
        qFatal("Failed to copy '%s' to '%s'", qPrintable(source), qPrintable(target));
    touch(target);
}

inline QString objectFileName(const QString &baseName, const QString &profileName)
{
    qbs::Settings settings((QString()));
    qbs::Profile profile(profileName, &settings);
    const QString suffix = profile.value("qbs.toolchain").toStringList().contains("msvc")
            ? "obj" : "o";
    return baseName + '.' + suffix;
}

#endif // Include guard.
