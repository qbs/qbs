/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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
#ifndef QBS_TEST_SHARED_H
#define QBS_TEST_SHARED_H

#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/settings.h>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QtTest>

inline QString profileName() { return QLatin1String("qbs_autotests"); }
inline QString relativeBuildDir(const QString &configurationName = QString())
{
    return !configurationName.isEmpty() ? configurationName : QLatin1String("default");
}

inline QString relativeBuildGraphFilePath() {
    return relativeBuildDir() + QLatin1Char('/') + relativeBuildDir() + QLatin1String(".bg");
}

inline bool regularFileExists(const QString &filePath)
{
    const QFileInfo fi(filePath);
    return fi.exists() && fi.isFile();
}

inline bool directoryExists(const QString &dirPath)
{
    const QFileInfo fi(dirPath);
    return fi.exists() && fi.isDir();
}

inline QString uniqueProductName(const QString &productName, const QString &_profileName)
{
    const QString p = _profileName.isEmpty() ? profileName() : _profileName;
    return productName + '.' + p;
}

inline QString relativeProductBuildDir(const QString &productName,
                                       const QString &productProfileName = QString())
{
    const QString fullName = uniqueProductName(productName, productProfileName);
    QString dirName = qbs::Internal::HostOsInfo::rfc1034Identifier(fullName);
    const QByteArray hash = QCryptographicHash::hash(fullName.toUtf8(), QCryptographicHash::Sha1);
    dirName.append('.').append(hash.toHex().left(8));
    return relativeBuildDir() + '/' + dirName;
}

inline QString relativeExecutableFilePath(const QString &productName)
{
    return relativeProductBuildDir(productName) + '/'
            + qbs::Internal::HostOsInfo::appendExecutableSuffix(productName);
}

inline void waitForNewTimestamp(const QString &testDir)
{
    // Waits for the time that corresponds to the host file system's time stamp granularity.
    if (qbs::Internal::HostOsInfo::isWindowsHost()) {
        QTest::qWait(1);        // NTFS has 100 ns precision. Let's ignore exFAT.
    } else {
        const QString nameTemplate = testDir + "/XXXXXX";
        QTemporaryFile f1(nameTemplate);
        if (!f1.open())
            qFatal("Failed to open temp file");
        const QDateTime initialTime = QFileInfo(f1).lastModified();
        while (true) {
            QTest::qWait(50);
            QTemporaryFile f2(nameTemplate);
            if (!f2.open())
                qFatal("Failed to open temp file");
            if (QFileInfo(f2).lastModified() > initialTime)
                break;
        }
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

inline QString inputDirHash(const QString &dir)
{
    return QCryptographicHash::hash(dir.toLatin1(), QCryptographicHash::Sha1).toHex().left(16);
}

inline QString testWorkDir(const QString &testName)
{
    QString dir = QDir::fromNativeSeparators(QString::fromLocal8Bit(qgetenv("QBS_TEST_WORK_ROOT")));
    if (dir.isEmpty()) {
        dir = QCoreApplication::applicationDirPath() + QStringLiteral("/../tests/auto/");
    } else {
        if (!dir.endsWith(QLatin1Char('/')))
            dir += QLatin1Char('/');
    }
    return dir + testName + "/testWorkDir";
}

inline qbs::Internal::HostOsInfo::HostOs targetOs()
{
    qbs::Settings settings((QString()));
    const qbs::Profile buildProfile(profileName(), &settings);
    const QStringList targetOS = buildProfile.value("qbs.targetOS").toStringList();
    if (targetOS.contains("windows"))
        return qbs::Internal::HostOsInfo::HostOsWindows;
    if (targetOS.contains("linux"))
        return qbs::Internal::HostOsInfo::HostOsLinux;
    if (targetOS.contains("macos"))
        return qbs::Internal::HostOsInfo::HostOsMacos;
    if (targetOS.contains("unix"))
        return qbs::Internal::HostOsInfo::HostOsOtherUnix;
    if (!targetOS.isEmpty())
        return qbs::Internal::HostOsInfo::HostOsOther;
    return qbs::Internal::HostOsInfo::hostOs();
}

#endif // Include guard.
