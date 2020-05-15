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
#include <tools/toolchains.h>

#include <QtCore/qbytearray.h>
#include <QtCore/qcryptographichash.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qstring.h>
#include <QtCore/qtemporaryfile.h>

#include <QtTest/qtest.h>

#include <memory>



#define REPLACE_IN_FILE(filePath, oldContent, newContent)                           \
    do {                                                                            \
        QFile f((filePath));                                                        \
        QVERIFY2(f.open(QIODevice::ReadWrite), qPrintable(f.errorString()));        \
        QByteArray content = f.readAll();                                           \
        const QByteArray savedContent = content;                                    \
        content.replace((oldContent), (newContent));                                \
        QVERIFY(content != savedContent);                                           \
        f.resize(0);                                                                \
        f.write(content);                                                           \
    } while (false)

inline int testTimeoutInMsecs()
{
    bool ok;
    int timeoutInSecs = qEnvironmentVariableIntValue("QBS_AUTOTEST_TIMEOUT", &ok);
    if (!ok)
        timeoutInSecs = 600;
    return timeoutInSecs * 1000;
}

// On Windows, it appears that a lock is sometimes held on files for a short while even after
// they are closed. The likelihood for that seems to increase with the slowness of the machine.
inline void waitForFileUnlock()
{
    bool ok;
    int timeoutInSecs = qEnvironmentVariableIntValue("QBS_AUTOTEST_IO_GRACE_PERIOD", &ok);
    if (!ok)
        timeoutInSecs = qbs::Internal::HostOsInfo::isWindowsHost() ? 1 : 0;
    if (timeoutInSecs > 0)
        QTest::qWait(timeoutInSecs * 1000);
}

using SettingsPtr = std::unique_ptr<qbs::Settings>;
inline SettingsPtr settings()
{
    const QString settingsDir = QLatin1String(qgetenv("QBS_AUTOTEST_SETTINGS_DIR"));
    return std::make_unique<qbs::Settings>(settingsDir);
}

inline QString profileName()
{
    const QString suiteProfile = QLatin1String(
                qgetenv("QBS_AUTOTEST_PROFILE_" QBS_TEST_SUITE_NAME));
    if (!suiteProfile.isEmpty())
        return suiteProfile;
    const QString profile = QLatin1String(qgetenv("QBS_AUTOTEST_PROFILE"));
    return !profile.isEmpty() ? profile : QLatin1String("none");
}

inline QString relativeBuildDir(const QString &configurationName = QString())
{
    return !configurationName.isEmpty() ? configurationName : QLatin1String("default");
}

inline QString relativeBuildGraphFilePath(const QString &configName = QString()) {
    return relativeBuildDir(configName) + QLatin1Char('/') + relativeBuildDir(configName)
            + QLatin1String(".bg");
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

struct ReadFileContentResult
{
    QByteArray content;
    QString errorString;
};

inline ReadFileContentResult readFileContent(const QString &filePath)
{
    ReadFileContentResult result;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.errorString = file.errorString();
        return result;
    }
    result.content = file.readAll();
    return result;
}

inline QByteArray diffText(const QByteArray &actual, const QByteArray &expected)
{
    QByteArray result;
    QList<QByteArray> actualLines = actual.split('\n');
    QList<QByteArray> expectedLines = expected.split('\n');
    int n = 1;
    while (!actualLines.isEmpty() && !expectedLines.isEmpty()) {
        QByteArray actualLine = actualLines.takeFirst();
        QByteArray expectedLine = expectedLines.takeFirst();
        if (actualLine != expectedLine) {
            result += QStringLiteral("%1:  actual: %2\n%1:expected: %3\n")
                    .arg(n, 2)
                    .arg(QString::fromUtf8(actualLine))
                    .arg(QString::fromUtf8(expectedLine))
                    .toUtf8();
        }
        n++;
    }
    auto addLines = [&result, &n] (const QList<QByteArray> &lines) {
        for (const QByteArray &line : qAsConst(lines)) {
            result += QStringLiteral("%1:          %2\n").arg(n).arg(QString::fromUtf8(line));
            n++;
        }
    };
    if (!actualLines.isEmpty()) {
        result += "Extra unexpected lines:\n";
        addLines(actualLines);
    }
    if (!expectedLines.isEmpty()) {
        result += "Missing expected lines:\n";
        addLines(expectedLines);
    }
    return result;
}

#define READ_TEXT_FILE(filePath, contentVariable)                                                  \
    QByteArray contentVariable;                                                                    \
    {                                                                                              \
        auto c = readFileContent(filePath);                                                        \
        QVERIFY2(c.errorString.isEmpty(),                                                          \
                 qUtf8Printable(QStringLiteral("Cannot open file %1. %2")                          \
                                .arg(filePath, c.errorString)));                                   \
        contentVariable = std::move(c.content);                                                    \
    }

#define TEXT_FILE_COMPARE(actualFilePath, expectedFilePath)                                        \
    {                                                                                              \
        READ_TEXT_FILE(actualFilePath, ba1);                                                       \
        READ_TEXT_FILE(expectedFilePath, ba2);                                                     \
        if (ba1 != ba2) {                                                                          \
            QByteArray msg = "File contents differ:\n" + diffText(ba1, ba2);                       \
            QFAIL(msg.constData());                                                                \
        }                                                                                          \
    }

template <typename T>
inline QString prefixedIfNonEmpty(const T &prefix, const QString &str)
{
    if (str.isEmpty())
        return QString();
    return prefix + str;
}

inline QString uniqueProductName(const QString &productName,
                                 const QString &multiplexConfigurationId)
{
    return productName + prefixedIfNonEmpty(QLatin1Char('.'), multiplexConfigurationId);
}

inline QString relativeProductBuildDir(const QString &productName,
                                       const QString &configurationName = QString(),
                                       const QString &multiplexConfigurationId = QString())
{
    const QString fullName = uniqueProductName(productName, multiplexConfigurationId);
    QString dirName = qbs::Internal::HostOsInfo::rfc1034Identifier(fullName);
    const QByteArray hash = QCryptographicHash::hash(fullName.toUtf8(), QCryptographicHash::Sha1);
    dirName.append('.').append(hash.toHex().left(8));
    return relativeBuildDir(configurationName) + '/' + dirName;
}

inline QString relativeExecutableFilePath(const QString &productName,
                                          const QString &configName = QString())
{
    return relativeProductBuildDir(productName, configName) + '/'
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
        int totalMsPassed = 0;
        while (totalMsPassed <= 2000) {
            static const int increment = 50;
            QTest::qWait(increment);
            totalMsPassed += increment;
            QTemporaryFile f2(nameTemplate);
            if (!f2.open())
                qFatal("Failed to open temp file");
            if (QFileInfo(f2).lastModified() > initialTime)
                return;
        }
        qWarning("Got no new timestamp after two seconds, going ahead anyway. Subsequent "
                 "test failure might not be genuine.");
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

inline QStringList profileToolchain(const qbs::Profile &profile)
{
    const auto toolchainType = profile.value(QStringLiteral("qbs.toolchainType")).toString();
    if (!toolchainType.isEmpty())
        return qbs::canonicalToolchain(toolchainType);
    return profile.value(QStringLiteral("qbs.toolchain")).toStringList();
}

inline QString objectFileName(const QString &baseName, const QString &profileName)
{
    const SettingsPtr s = settings();
    qbs::Profile profile(profileName, s.get());

    const auto tcList = profileToolchain(profile);
    const bool isMsvc = tcList.contains("msvc")
            || (tcList.isEmpty() && qbs::Internal::HostOsInfo::isWindowsHost());
    const QString suffix = isMsvc ? "obj" : "o";
    return baseName + '.' + suffix;
}

inline QString inputDirHash(const QString &dir)
{
    return QCryptographicHash::hash(dir.toLatin1(), QCryptographicHash::Sha1).toHex().left(16);
}

inline QString testDataSourceDir(const QString &dir)
{
    QDir result;
    QString testSourceRootDirFromEnv = QDir::fromNativeSeparators(qEnvironmentVariable("QBS_TEST_SOURCE_ROOT"));
    if (testSourceRootDirFromEnv.isEmpty()) {
        result.setPath(dir);
    } else {
        QDir testSourceRootDir(dir);
        while (testSourceRootDir.dirName() != "tests")
            testSourceRootDir = QFileInfo(testSourceRootDir, "").dir();

        QString relativeDataPath = testSourceRootDir.relativeFilePath(dir);
        QString absoluteDataPath = QDir(testSourceRootDirFromEnv).absoluteFilePath(relativeDataPath);
        result.setPath(absoluteDataPath);
    }

    if (!result.exists())
        qFatal("Expected data folder '%s' to be present, but it does not exist. You may set "
               "QBS_TEST_SOURCE_ROOT to the 'tests' folder in your qbs repository to configure "
               "a custom location.", qPrintable(result.absolutePath()));

    return result.absolutePath();
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

inline bool copyDllExportHeader(const QString &srcDataDir, const QString &targetDataDir)
{
    QFile sourceFile(srcDataDir + "/../../dllexport.h");
    if (!sourceFile.exists())
        return true;
    const QString targetPath = targetDataDir + "/dllexport.h";
    QFile::remove(targetPath);
    return sourceFile.copy(targetPath);
}

inline qbs::Internal::HostOsInfo::HostOs targetOs()
{
    const SettingsPtr s = settings();
    const qbs::Profile buildProfile(profileName(), s.get());
    const QString targetPlatform = buildProfile.value("qbs.targetPlatform").toString();
    if (!targetPlatform.isEmpty()) {
        const std::vector<std::string> targetOS = qbs::Internal::HostOsInfo::canonicalOSIdentifiers(
                    targetPlatform.toStdString());
        if (qbs::Internal::contains(targetOS, "windows"))
            return qbs::Internal::HostOsInfo::HostOsWindows;
        if (qbs::Internal::contains(targetOS, "linux"))
            return qbs::Internal::HostOsInfo::HostOsLinux;
        if (qbs::Internal::contains(targetOS, "macos"))
            return qbs::Internal::HostOsInfo::HostOsMacos;
        if (qbs::Internal::contains(targetOS, "unix"))
            return qbs::Internal::HostOsInfo::HostOsOtherUnix;
        return qbs::Internal::HostOsInfo::HostOsOther;
    }
    return qbs::Internal::HostOsInfo::hostOs();
}

#endif // Include guard.
