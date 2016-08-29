/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#undef QT_NO_CAST_FROM_ASCII // I am qmake, and I approve this hack.

#include "tst_tools.h"

#include "buildoptions.h"
#include "error.h"
#include "fileinfo.h"
#include "hostosinfo.h"
#include "processutils.h"
#include "profile.h"
#include "settings.h"
#include "setupprojectparameters.h"
#include "version.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>

namespace qbs {
namespace Internal {

TestTools::TestTools(Settings *settings) : m_settings(settings)
{
}

TestTools::~TestTools()
{
    qDeleteAll(m_tmpDirs);
}

void TestTools::testFileInfo()
{
    QCOMPARE(FileInfo::fileName("C:/waffl/copter.exe"), QString("copter.exe"));
    QCOMPARE(FileInfo::baseName("C:/waffl/copter.exe.lib"), QString("copter"));
    QCOMPARE(FileInfo::completeBaseName("C:/waffl/copter.exe.lib"), QString("copter.exe"));
    QCOMPARE(FileInfo::path("abc"), QString("."));
    QCOMPARE(FileInfo::path("/abc/lol"), QString("/abc"));
    QCOMPARE(FileInfo::path("/fileInRoot"), QString(QLatin1Char('/')));
    if (HostOsInfo::isWindowsHost())
        QCOMPARE(FileInfo::path("C:/fileInDriveRoot"), QString("C:/"));
    QVERIFY(!FileInfo::isAbsolute("bla/lol"));
    QVERIFY(FileInfo::isAbsolute("/bla/lol"));
    if (HostOsInfo::isWindowsHost())
        QVERIFY(FileInfo::isAbsolute("C:\\bla\\lol"));
    QCOMPARE(FileInfo::resolvePath("/abc/lol", "waffl"), QString("/abc/lol/waffl"));
    QCOMPARE(FileInfo::resolvePath("/abc/def/ghi/jkl/", "../foo/bar"), QString("/abc/def/ghi/foo/bar"));
    QCOMPARE(FileInfo::resolvePath("/abc/def/ghi/jkl/", "../../foo/bar"), QString("/abc/def/foo/bar"));
    QCOMPARE(FileInfo::resolvePath("/abc", "../../../foo/bar"), QString("/foo/bar"));
    QCOMPARE(FileInfo("/does/not/exist").lastModified(), FileTime());
}

void TestTools::fileCaseCheck()
{
    QTemporaryFile tempFile(QDir::tempPath() + QLatin1String("/CamelCase"));
    QVERIFY2(tempFile.open(), qPrintable(tempFile.errorString()));
    QFileInfo tempFileInfo(tempFile.fileName());
    const QString lowerFilePath = tempFileInfo.absolutePath() + QLatin1Char('/')
            + tempFileInfo.fileName().toLower();
    const QString upperFilePath = tempFileInfo.absolutePath() + QLatin1Char('/')
            + tempFileInfo.fileName().toUpper();
    QVERIFY(FileInfo::isFileCaseCorrect(tempFileInfo.absoluteFilePath()));
    if (QFile::exists(lowerFilePath))
        QVERIFY(!FileInfo::isFileCaseCorrect(lowerFilePath));
    if (QFile::exists(upperFilePath))
        QVERIFY(!FileInfo::isFileCaseCorrect(upperFilePath));
}

void TestTools::testProfiles()
{
    TemporaryProfile tpp("parent", m_settings);
    Profile parentProfile = tpp.p;
    TemporaryProfile tpc("child", m_settings);
    Profile childProfile = tpc.p;
    parentProfile.removeBaseProfile();
    parentProfile.remove("testKey");
    QCOMPARE(parentProfile.value("testKey", "none").toString(), QLatin1String("none"));
    parentProfile.setValue("testKey", "testValue");
    QCOMPARE(parentProfile.value("testKey").toString(), QLatin1String("testValue"));

    childProfile.remove("testKey");
    childProfile.removeBaseProfile();
    QCOMPARE(childProfile.value("testKey", "none").toString(), QLatin1String("none"));
    childProfile.setBaseProfile("parent");
    QCOMPARE(childProfile.value("testKey").toString(), QLatin1String("testValue"));

    // Change base profile and check if the inherited value also changes.
    TemporaryProfile tpf("foo", m_settings);
    Profile fooProfile = tpf.p;
    fooProfile.setValue("testKey", "gnampf");
    childProfile.setBaseProfile("foo");
    QCOMPARE(childProfile.value("testKey", "none").toString(), QLatin1String("gnampf"));

    ErrorInfo errorInfo;
    childProfile.setBaseProfile("SmurfAlongWithMe");
    childProfile.value("blubb", QString(), &errorInfo);
    QVERIFY(errorInfo.hasError());

    errorInfo.clear();
    childProfile.setBaseProfile("parent");
    parentProfile.setBaseProfile("child");
    QVERIFY(!childProfile.value("blubb", QString(), &errorInfo).isValid());
    QVERIFY(errorInfo.hasError());

    QVERIFY(!childProfile.allKeys(Profile::KeySelectionNonRecursive).isEmpty());

    errorInfo.clear();
    QVERIFY(childProfile.allKeys(Profile::KeySelectionRecursive, &errorInfo).isEmpty());
    QVERIFY(errorInfo.hasError());
}

void TestTools::testSettingsMigration()
{
    QFETCH(QString, baseDir);
    QFETCH(bool, hasOldSettings);
    Settings settings(baseDir);
    if (hasOldSettings) {
        QVERIFY(QFileInfo(settings.baseDirectory() + "/qbs/" QBS_VERSION "/profiles/right.txt")
                .exists());
        QCOMPARE(settings.value("key").toString(),
                 settings.baseDirectory() + "/qbs/" QBS_VERSION "/profilesright");
    } else {
        QVERIFY(!QFileInfo(settings.baseDirectory() + "/qbs/" QBS_VERSION "/profiles").exists());
        QVERIFY(settings.allKeys().isEmpty());
    }
}

void TestTools::testSettingsMigration_data()
{
    QTest::addColumn<QString>("baseDir");
    QTest::addColumn<bool>("hasOldSettings");
    QTest::newRow("settings dir with lots of versions") << setupSettingsDir1() << true;
    QTest::newRow("settings dir with only a fallback") << setupSettingsDir2() << true;
    QTest::newRow("no previous settings") << setupSettingsDir3() << false;
}

QString TestTools::setupSettingsDir1()
{
    QTemporaryDir * const baseDir = new QTemporaryDir;
    m_tmpDirs << baseDir;

    const Version thisVersion = Version::fromString(QBS_VERSION);
    Version predecessor;
    if (thisVersion.patchLevel() > 0) {
        predecessor.setMajorVersion(thisVersion.majorVersion());
        predecessor.setMinorVersion(thisVersion.minorVersion());
        predecessor.setPatchLevel(thisVersion.patchLevel() - 1);
    } else if (thisVersion.minorVersion() > 0) {
        predecessor.setMajorVersion(thisVersion.majorVersion());
        predecessor.setMinorVersion(thisVersion.minorVersion() - 1);
        predecessor.setPatchLevel(99);
    } else {
        predecessor.setMajorVersion(thisVersion.majorVersion() - 1);
        predecessor.setMajorVersion(99);
        predecessor.setPatchLevel(99);
    }
    const auto versions = QList<Version>() << Version(0, 1, 0) << Version(1, 0, 5) << predecessor
            << Version(thisVersion.majorVersion() + 1, thisVersion.minorVersion(),
                       thisVersion.patchLevel())
            << Version(thisVersion.majorVersion(), thisVersion.minorVersion() + 1,
                       thisVersion.patchLevel())
            << Version(thisVersion.majorVersion(), thisVersion.minorVersion(),
                       thisVersion.patchLevel() + 1)
            << Version(99, 99, 99);
    foreach (const Version &v, versions) {
        const QString settingsDir = baseDir->path() + "/qbs/" + v.toString();
        QSettings s(settingsDir + "/qbs.conf",
                    HostOsInfo::isWindowsHost() ? QSettings::IniFormat : QSettings::NativeFormat);
        const QString profilesDir = settingsDir + "/profiles";
        QDir::root().mkpath(profilesDir);
        const QString magicString = v == predecessor ? "right" : "wrong";
        QFile f(profilesDir + '/' + magicString + ".txt");
        f.open(QIODevice::WriteOnly);
        s.setValue("org/qt-project/qbs/key", profilesDir + magicString);
    }

    return baseDir->path();
}

QString TestTools::setupSettingsDir2()
{
    QTemporaryDir * const baseDir = new QTemporaryDir;
    m_tmpDirs << baseDir;
    const QString settingsDir = baseDir->path();
    QSettings s(settingsDir + QLatin1String("/qbs.conf"),
                HostOsInfo::isWindowsHost() ? QSettings::IniFormat : QSettings::NativeFormat);
    const QString profilesDir = settingsDir + QLatin1String("/qbs/profiles");
    QDir::root().mkpath(profilesDir);
    QFile f(profilesDir + "/right.txt");
    f.open(QIODevice::WriteOnly);
    s.setValue("org/qt-project/qbs/key", profilesDir + "right");

    return baseDir->path();
}

QString TestTools::setupSettingsDir3()
{
    auto * const baseDir = new QTemporaryDir;
    m_tmpDirs << baseDir;
    return baseDir->path();
}

void TestTools::testBuildConfigMerging()
{
    Settings settings((QString()));
    TemporaryProfile tp(QLatin1String("tst_tools_profile"), &settings);
    Profile profile = tp.p;
    profile.setValue(QLatin1String("topLevelKey"), QLatin1String("topLevelValue"));
    profile.setValue(QLatin1String("qbs.toolchain"), QLatin1String("gcc"));
    profile.setValue(QLatin1String("qbs.architecture"),
                     QLatin1String("Jean-Claude Pillemann"));
    profile.setValue(QLatin1String("cpp.treatWarningsAsErrors"), true);
    QVariantMap overrideMap;
    overrideMap.insert(QLatin1String("qbs.toolchain"), QLatin1String("clang"));
    overrideMap.insert(QLatin1String("qbs.installRoot"), QLatin1String("/blubb"));
    SetupProjectParameters params;
    params.setTopLevelProfile(profile.name());
    params.setConfigurationName(QLatin1String("debug"));
    params.setOverriddenValues(overrideMap);
    const ErrorInfo error = params.expandBuildConfiguration();
    QVERIFY2(!error.hasError(), qPrintable(error.toString()));
    const QVariantMap finalMap = params.finalBuildConfigurationTree();
    QCOMPARE(finalMap.count(), 3);
    QCOMPARE(finalMap.value(QLatin1String("topLevelKey")).toString(),
             QString::fromLatin1("topLevelValue"));
    const QVariantMap finalQbsMap = finalMap.value(QLatin1String("qbs")).toMap();
    QCOMPARE(finalQbsMap.count(), 4);
    QCOMPARE(finalQbsMap.value(QLatin1String("toolchain")).toString(),
             QString::fromLatin1("clang"));
    QCOMPARE(finalQbsMap.value(QLatin1String("configurationName")).toString(),
             QString::fromLatin1("debug"));
    QCOMPARE(finalQbsMap.value(QLatin1String("architecture")).toString(),
             QString::fromLatin1("Jean-Claude Pillemann"));
    QCOMPARE(finalQbsMap.value(QLatin1String("installRoot")).toString(), QLatin1String("/blubb"));
    const QVariantMap finalCppMap = finalMap.value(QLatin1String("cpp")).toMap();
    QCOMPARE(finalCppMap.count(), 1);
    QCOMPARE(finalCppMap.value(QLatin1String("treatWarningsAsErrors")).toBool(), true);
}

void TestTools::testProcessNameByPid()
{
    QCOMPARE(qAppName(), processNameByPid(QCoreApplication::applicationPid()));
}

} // namespace Internal
} // namespace qbs
