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
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
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

#include <QFileInfo>
#include <QTemporaryFile>
#include <QTest>

namespace qbs {
namespace Internal {

TestTools::TestTools(Settings *settings) : m_settings(settings)
{
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
    QTemporaryFile tempFile(QLatin1String("CamelCase"));
    QVERIFY(tempFile.open());
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
    params.setBuildVariant(QLatin1String("debug"));
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
    QCOMPARE(finalQbsMap.value(QLatin1String("buildVariant")).toString(),
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
