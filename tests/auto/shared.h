/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://www.qt.io/licensing.  For further information
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
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/
#ifndef QBS_TEST_SHARED_H
#define QBS_TEST_SHARED_H

#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/settings.h>

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QtTest>

#include <ctime>

inline QString profileName() { return QLatin1String("qbs_autotests"); }
inline QString relativeBuildDir() { return profileName() + QLatin1String("-debug"); }

inline QString relativeBuildGraphFilePath() {
    return relativeBuildDir() + QLatin1Char('/') + relativeBuildDir() + QLatin1String(".bg");
}

inline bool regularFileExists(const QString &filePath)
{
    const QFileInfo fi(filePath);
    return fi.exists() && fi.isFile();
}

inline QString uniqueProductName(const QString &productName, const QString &_profileName)
{
    const QString p = _profileName.isEmpty() ? profileName() : _profileName;
    return productName + '.' + p;
}

inline QString relativeProductBuildDir(const QString &productName,
                                       const QString &profileName = QString())
{
    const QString fullName = uniqueProductName(productName, profileName);
    QString dirName = fullName;
    for (int i = 0; i < dirName.count(); ++i) {
        QCharRef c = dirName[i];
        const bool okChar = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')
                || (c >= 'a' && c <= 'z') || c == '_' || c == '.';
        if (!okChar)
            c = QChar::fromLatin1('_');
    }
    const QByteArray hash = QCryptographicHash::hash(fullName.toUtf8(), QCryptographicHash::Sha1);
    dirName.append('.').append(hash.toHex().left(8));
    return relativeBuildDir() + '/' + dirName;
}

inline QString relativeExecutableFilePath(const QString &productName)
{
    return relativeProductBuildDir(productName) + '/'
            + qbs::Internal::HostOsInfo::appendExecutableSuffix(productName);
}

inline void waitForNewTimestamp()
{
    // Waits for the time that corresponds to the host file system's time stamp granularity.
    if (qbs::Internal::HostOsInfo::isWindowsHost()) {
        QTest::qWait(1);        // NTFS has 100 ns precision. Let's ignore exFAT.
    } else {
        time_t oldTime;
        time_t newTime = std::time(0);
        do {
            oldTime = newTime;
            QTest::qWait(50);
            newTime = std::time(0);
        } while (oldTime == newTime);
    }
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
