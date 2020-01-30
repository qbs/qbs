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

#include "tst_blackboxjava.h"

#include "../shared.h"
#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/qttools.h>

#include <QtCore/qjsondocument.h>
#include <QtCore/qtemporarydir.h>

using qbs::Internal::HostOsInfo;
using qbs::Profile;

TestBlackboxJava::TestBlackboxJava() : TestBlackboxBase (SRCDIR "/testdata-java", "blackbox-java")
{
}

static QProcessEnvironment processEnvironmentWithCurrentDirectoryInLibraryPath()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(HostOsInfo::libraryPathEnvironmentVariable(),
               (QStringList() << env.value(HostOsInfo::libraryPathEnvironmentVariable()) << ".")
               .join(HostOsInfo::pathListSeparator()));
    return env;
}

void TestBlackboxJava::java()
{
#if defined(Q_OS_WIN32) && !defined(Q_OS_WIN64)
    QSKIP("QTBUG-3845");
#endif

    const SettingsPtr s = settings();
    Profile p(profileName(), s.get());

    int status;
    const auto jdkTools = findJdkTools(&status);
    QCOMPARE(status, 0);

    QDir::setCurrent(testDataDir + "/java");

    status = runQbs();
    if (p.value("java.jdkPath").toString().isEmpty()
            && status != 0 && m_qbsStderr.contains("jdkPath")) {
        QSKIP("java.jdkPath not set and automatic detection failed");
    }

    QCOMPARE(status, 0);

    const QStringList classFiles =
            QStringList() << "Jet" << "Ship" << "Vehicles";
    QStringList classFiles1 = QStringList(classFiles) << "io/qt/qbs/HelloWorld" << "NoPackage";
    for (QString &classFile : classFiles1) {
        classFile = relativeProductBuildDir("cc") + "/classes/" + classFile + ".class";
        QVERIFY2(regularFileExists(classFile), qPrintable(classFile));
    }

    for (const QString &classFile : classFiles) {
        const QString filePath = relativeProductBuildDir("jar_file") + "/classes/" + classFile
                + ".class";
        QVERIFY2(regularFileExists(filePath), qPrintable(filePath));
    }
    const QString jarFilePath = relativeProductBuildDir("jar_file") + '/' + "jar_file.jar";
    QVERIFY2(regularFileExists(jarFilePath), qPrintable(jarFilePath));

    // Now check whether we correctly predicted the class file output paths.
    QCOMPARE(runQbs(QbsRunParameters("clean")), 0);
    for (const QString &classFile : qAsConst(classFiles1)) {
        QVERIFY2(!regularFileExists(classFile), qPrintable(classFile));
    }

    // This tests various things: java.manifestClassPath, JNI, etc.
    QDir::setCurrent(relativeBuildDir() + "/install-root");
    QProcess process;
    process.setProcessEnvironment(processEnvironmentWithCurrentDirectoryInLibraryPath());
    process.start(HostOsInfo::appendExecutableSuffix(jdkTools["java"]),
            QStringList() << "-jar" << "jar_file.jar");
    if (process.waitForStarted()) {
        QVERIFY2(process.waitForFinished(), qPrintable(process.errorString()));
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
        const QByteArray stdOut = process.readAllStandardOutput();
        QVERIFY2(stdOut.contains("Driving!"), stdOut.constData());
        QVERIFY2(stdOut.contains("Flying!"), stdOut.constData());
        QVERIFY2(stdOut.contains("Flying (this is a space ship)!"), stdOut.constData());
        QVERIFY2(stdOut.contains("Sailing!"), stdOut.constData());
        QVERIFY2(stdOut.contains("Native code performing complex internal combustion process ("),
                 stdOut.constData());
    }

    process.start("unzip", QStringList() << "-p" << "jar_file.jar");
    if (process.waitForStarted()) {
        QVERIFY2(process.waitForFinished(), qPrintable(process.errorString()));
        const QByteArray stdOut = process.readAllStandardOutput();
        QVERIFY2(stdOut.contains("Class-Path: random_stuff.jar car_jar.jar"), stdOut.constData());
        QVERIFY2(stdOut.contains("Main-Class: Vehicles"), stdOut.constData());
        QVERIFY2(stdOut.contains("Some-Property: Some-Value"), stdOut.constData());
        QVERIFY2(stdOut.contains("Additional-Property: Additional-Value"), stdOut.constData());
        QVERIFY2(stdOut.contains("Extra-Property: Crazy-Value"), stdOut.constData());
    }
}

static QString dpkgArch(const QString &prefix = QString())
{
    QProcess dpkg;
    dpkg.start("/usr/bin/dpkg", QStringList() << "--print-architecture");
    dpkg.waitForFinished();
    if (dpkg.exitStatus() == QProcess::NormalExit && dpkg.exitCode() == 0)
        return prefix + QString::fromLocal8Bit(dpkg.readAllStandardOutput().trimmed());
    return {};
}

void TestBlackboxJava::javaDependencyTracking()
{
    QFETCH(QString, jdkPath);
    QFETCH(QString, javaVersion);

    QDir::setCurrent(testDataDir + "/java");
    QbsRunParameters rp;
    rp.arguments.push_back("--check-outputs");
    if (!jdkPath.isEmpty())
        rp.arguments << ("modules.java.jdkPath:" + jdkPath);
    if (!javaVersion.isEmpty())
        rp.arguments << ("modules.java.languageVersion:'" + javaVersion + "'");
    rmDirR(relativeBuildDir());
    const bool defaultJdkPossiblyTooOld = jdkPath.isEmpty() && !javaVersion.isEmpty();
    rp.expectFailure = defaultJdkPossiblyTooOld;
    QVERIFY(runQbs(rp) == 0
            || (defaultJdkPossiblyTooOld && m_qbsStderr.contains("invalid source release")));
}

void TestBlackboxJava::javaDependencyTracking_data()
{
    QTest::addColumn<QString>("jdkPath");
    QTest::addColumn<QString>("javaVersion");

    const SettingsPtr s = settings();
    Profile p(profileName(), s.get());

    auto getSpecificJdkVersion = [](const QString &jdkVersion) -> QString {
        if (HostOsInfo::isMacosHost()) {
            QProcess java_home;
            java_home.start("/usr/libexec/java_home", QStringList() << "--version" << jdkVersion);
            java_home.waitForFinished();
            if (java_home.exitStatus() == QProcess::NormalExit && java_home.exitCode() == 0)
                return QString::fromLocal8Bit(java_home.readAllStandardOutput().trimmed());
        } else if (HostOsInfo::isWindowsHost()) {
            QSettings settings("HKEY_LOCAL_MACHINE\\SOFTWARE\\JavaSoft\\Java Development Kit\\"
                               + jdkVersion, QSettings::NativeFormat);
            return settings.value("JavaHome").toString();
        } else {
            QString minorVersion = jdkVersion;
            if (minorVersion.startsWith("1."))
                minorVersion.remove(0, 2);

            const QStringList searchPaths = {
                "/usr/lib/jvm/java-" + minorVersion + "-openjdk" + dpkgArch("-"), // Debian
                "/usr/lib/jvm/java-" + minorVersion + "-openjdk", // Arch
                "/usr/lib/jvm/jre-1." + minorVersion + ".0-openjdk", // Fedora
                "/usr/lib64/jvm/java-1." + minorVersion + ".0-openjdk", // OpenSuSE
            };
            for (const QString &searchPath : searchPaths) {
                if (QFile::exists(searchPath + "/bin/javac"))
                    return searchPath;
            }
        }

        return {};
    };

    static const auto knownJdkVersions = QStringList() << "1.6" << "1.7" << "1.8" << "1.9"
                                                       << QString(); // default JDK;
    QStringList seenJdkVersions;
    for (const auto &jdkVersion : knownJdkVersions) {
        QString specificJdkPath = getSpecificJdkVersion(jdkVersion);
        if (jdkVersion.isEmpty() || !specificJdkPath.isEmpty()) {
            const auto jdkPath = jdkVersion.isEmpty() ? jdkVersion : specificJdkPath;

            if (!jdkVersion.isEmpty())
                seenJdkVersions << jdkVersion;

            if (!seenJdkVersions.empty()) {
                const auto javaVersions = QStringList()
                    << knownJdkVersions.mid(0, knownJdkVersions.indexOf(seenJdkVersions.last()) + 1)
                    << QString(); // also test with no explicitly specified source version

                for (const auto &currentJavaVersion : javaVersions) {
                    const QString rowName = (!jdkPath.isEmpty() ? jdkPath : "default JDK")
                            + QStringLiteral(", ")
                            + (!currentJavaVersion.isEmpty()
                               ? ("Java " + currentJavaVersion)
                               : "default Java version");
                    QTest::newRow(rowName.toLatin1().constData())
                            << jdkPath << currentJavaVersion;
                }
            }
        }
    }

    if (seenJdkVersions.empty())
        QSKIP("No JDKs installed");
}

void TestBlackboxJava::javaDependencyTrackingInnerClass()
{
    const SettingsPtr s = settings();
    Profile p(profileName(), s.get());

    QDir::setCurrent(testDataDir + "/java/inner-class");
    QbsRunParameters params;
    int status = runQbs(params);
    if (p.value("java.jdkPath").toString().isEmpty()
            && status != 0 && m_qbsStderr.contains("jdkPath")) {
        QSKIP("java.jdkPath not set and automatic detection failed");
    }
    QCOMPARE(status, 0);
}

QTEST_MAIN(TestBlackboxJava)
