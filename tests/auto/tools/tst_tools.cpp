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

#include "../shared.h"

#include <tools/buildoptions.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/filesaver.h>
#include <tools/hostosinfo.h>
#include <tools/processutils.h>
#include <tools/profile.h>
#include <tools/set.h>
#include <tools/settings.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringutils.h>
#include <tools/version.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qsettings.h>
#include <QtCore/qtemporarydir.h>
#include <QtCore/qtemporaryfile.h>

#include <QtTest/qtest.h>

using namespace qbs;
using namespace qbs::Internal;

TestTools::TestTools(Settings *settings)
    : m_settings(settings), testDataDir(testWorkDir("tools"))
{
}

TestTools::~TestTools()
{
    qDeleteAll(m_tmpDirs);
}

void TestTools::initTestCase()
{
    QDir().mkpath(testDataDir);
}

void TestTools::fileSaver()
{
    QVERIFY(QDir::setCurrent(testDataDir));

    static const char *fn = "foo.txt";
    const auto run = [](const std::function<void()> &func) {
        if (QFile::exists(fn))
            QVERIFY(QFile::remove(fn));
        func();
        if (QFile::exists(fn))
            QVERIFY(QFile::remove(fn));
    };

    // failing to open the file means nothing works
    run([] {
        Internal::FileSaver fs(fn);
        QVERIFY(!fs.device());
        QVERIFY(!fs.write("hello"));
        QVERIFY(!fs.commit());
        QVERIFY(!QFile::exists(fn));
    });

    // verify that correct usage creates a file with the right contents
    run([] {
        Internal::FileSaver fs(fn);
        QVERIFY(fs.open());
        QVERIFY(fs.device());
        QVERIFY(fs.write("hello"));
        QVERIFY(fs.commit());
        QVERIFY(QFile::exists(fn));
        QFile f(fn);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArrayLiteral("hello"));
    });

    // failing to commit writes nothing
    run([] {
        Internal::FileSaver fs(fn);
        QVERIFY(fs.open());
        QVERIFY(fs.device());
        QVERIFY(fs.write("hello"));
        QVERIFY(!QFile::exists(fn));
    });

    // verify that correct usage creates a file with the right contents and does not overwrite
    run([] {
        {
            Internal::FileSaver fs(fn);
            QVERIFY(fs.open());
            QVERIFY(fs.device());
            QVERIFY(fs.write("hello"));
            QVERIFY(fs.commit());
            QVERIFY(QFile::exists(fn));
            QFile f(fn);
            QVERIFY(f.open(QIODevice::ReadOnly));
            QCOMPARE(f.readAll(), QByteArrayLiteral("hello"));
        }

        const auto lm = QFileInfo(fn).lastModified();
        QVERIFY(lm.isValid());

        waitForNewTimestamp(".");

        {
            Internal::FileSaver fs(fn);
            QVERIFY(fs.open());
            QVERIFY(fs.device());
            QVERIFY(fs.write("hello"));
            QVERIFY(fs.commit());
            QVERIFY(QFile::exists(fn));
        }

        const auto lm2 = QFileInfo(fn).lastModified();
        QVERIFY(lm2.isValid());

        QCOMPARE(lm, lm2); // timestamps should be the same since content did not change

        waitForNewTimestamp(".");

        {
            Internal::FileSaver fs(fn);
            QVERIFY(fs.open());
            QVERIFY(fs.device());
            QVERIFY(fs.write("hello2"));
            QVERIFY(fs.commit());
            QVERIFY(QFile::exists(fn));
            QFile f(fn);
            QVERIFY(f.open(QIODevice::ReadOnly));
            QCOMPARE(f.readAll(), QByteArrayLiteral("hello2"));
        }

        const auto lm3 = QFileInfo(fn).lastModified();
        QVERIFY(lm3.isValid());

        QVERIFY(lm != lm3); // timestamps should differ since the content changed

        waitForNewTimestamp(".");

        {
            // Test overwriteIfUnchanged
            Internal::FileSaver fs(fn, true);
            QVERIFY(fs.open());
            QVERIFY(fs.device());
            QVERIFY(fs.write("hello2"));
            QVERIFY(fs.commit());
            QVERIFY(QFile::exists(fn));
            QFile f(fn);
            QVERIFY(f.open(QIODevice::ReadOnly));
            QCOMPARE(f.readAll(), QByteArrayLiteral("hello2"));
        }

        const auto lm4 = QFileInfo(fn).lastModified();
        QVERIFY(lm4.isValid());

        QVERIFY(lm3 != lm4); // timestamps should differ since we always overwrite
    });
}

void TestTools::testFileInfo()
{
    QCOMPARE(FileInfo::fileName("C:/waffl/copter.exe"), QString("copter.exe"));
    QCOMPARE(FileInfo::baseName("C:/waffl/copter.exe.lib"), QString("copter"));
    QCOMPARE(FileInfo::completeBaseName("C:/waffl/copter.exe.lib"), QString("copter.exe"));
    QCOMPARE(FileInfo::suffix("C:/waffl/copter.exe.lib"), QString("lib"));
    QCOMPARE(FileInfo::completeSuffix("C:/waffl/copter.exe.lib"), QString("exe.lib"));
    QCOMPARE(FileInfo::path("abc"), QString("."));
    QCOMPARE(FileInfo::path("/abc/lol"), QString("/abc"));
    QCOMPARE(FileInfo::path("/fileInRoot"), QString(QLatin1Char('/')));
    if (HostOsInfo::isWindowsHost())
        QCOMPARE(FileInfo::path("C:/fileInDriveRoot"), QString("C:/"));
    QVERIFY(!FileInfo::isAbsolute("bla/lol"));
    QVERIFY(FileInfo::isAbsolute("/bla/lol"));
    if (HostOsInfo::isWindowsHost()) {
        QVERIFY(FileInfo::isAbsolute("C:\\bla\\lol"));
        QVERIFY(FileInfo::isAbsolute("C:\\"));
        QVERIFY(FileInfo::isAbsolute("C:/"));
        QVERIFY(!FileInfo::isAbsolute("C:"));
    }
    QCOMPARE(FileInfo::resolvePath("/abc/lol", "waffl"), QString("/abc/lol/waffl"));
    QCOMPARE(FileInfo::resolvePath("/abc/def/ghi/jkl/", "../foo/bar"), QString("/abc/def/ghi/foo/bar"));
    QCOMPARE(FileInfo::resolvePath("/abc/def/ghi/jkl/", "../../foo/bar"), QString("/abc/def/foo/bar"));
    QCOMPARE(FileInfo::resolvePath("/abc", "../../../foo/bar"), QString("/foo/bar"));
    if (HostOsInfo::isWindowsHost()) {
        QCOMPARE(FileInfo::resolvePath("C:/share", ".."), QString("C:/"));
        QCOMPARE(FileInfo::resolvePath("C:/share", "D:/"), QString("D:/"));
        QCOMPARE(FileInfo::resolvePath("C:/share", "D:"), QString()); // should soft-assert
    }
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

    QVERIFY(!childProfile.allKeys(Profile::KeySelectionNonRecursive).empty());

    errorInfo.clear();
    QVERIFY(childProfile.allKeys(Profile::KeySelectionRecursive, &errorInfo).empty());
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
        QCOMPARE(settings.value("key", Settings::UserScope).toString(),
                 settings.baseDirectory() + "/qbs/" QBS_VERSION "/profilesright");
    } else {
        QVERIFY(!QFileInfo(settings.baseDirectory() + "/qbs/" QBS_VERSION "/profiles").exists());
        QVERIFY(settings.allKeys(Settings::UserScope).empty());
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
    const auto baseDir = new QTemporaryDir;
    m_tmpDirs.push_back(baseDir);

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
    for (const Version &v : versions) {
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
    const auto baseDir = new QTemporaryDir;
    m_tmpDirs.push_back(baseDir);
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
    const auto baseDir = new QTemporaryDir;
    m_tmpDirs.push_back(baseDir);
    return baseDir->path();
}

void TestTools::testBuildConfigMerging()
{
    TemporaryProfile tp(QLatin1String("tst_tools_profile"), m_settings);
    Profile profile = tp.p;
    profile.setValue(QStringLiteral("topLevelKey"), QStringLiteral("topLevelValue"));
    profile.setValue(QStringLiteral("qbs.toolchain"), QStringLiteral("gcc"));
    profile.setValue(QStringLiteral("qbs.architecture"),
                     QStringLiteral("Jean-Claude Pillemann"));
    profile.setValue(QStringLiteral("cpp.treatWarningsAsErrors"), true);
    QVariantMap overrideMap;
    overrideMap.insert(QStringLiteral("qbs.toolchain"), QStringLiteral("clang"));
    overrideMap.insert(QStringLiteral("qbs.installRoot"), QStringLiteral("/blubb"));
    SetupProjectParameters params;
    params.setSettingsDirectory(m_settings->baseDirectory());
    params.setTopLevelProfile(profile.name());
    params.setConfigurationName(QStringLiteral("debug"));
    params.setOverriddenValues(overrideMap);
    const ErrorInfo error = params.expandBuildConfiguration();
    QVERIFY2(!error.hasError(), qPrintable(error.toString()));
    const QVariantMap finalMap = params.finalBuildConfigurationTree();
    QCOMPARE(finalMap.size(), 3);
    QCOMPARE(finalMap.value(QStringLiteral("topLevelKey")).toString(),
             QStringLiteral("topLevelValue"));
    const QVariantMap finalQbsMap = finalMap.value(QStringLiteral("qbs")).toMap();
    QCOMPARE(finalQbsMap.size(), 4);
    QCOMPARE(finalQbsMap.value(QStringLiteral("toolchain")).toString(),
             QStringLiteral("clang"));
    QCOMPARE(finalQbsMap.value(QStringLiteral("configurationName")).toString(),
             QStringLiteral("debug"));
    QCOMPARE(finalQbsMap.value(QStringLiteral("architecture")).toString(),
             QStringLiteral("Jean-Claude Pillemann"));
    QCOMPARE(finalQbsMap.value(QStringLiteral("installRoot")).toString(), QLatin1String("/blubb"));
    const QVariantMap finalCppMap = finalMap.value(QStringLiteral("cpp")).toMap();
    QCOMPARE(finalCppMap.size(), 1);
    QCOMPARE(finalCppMap.value(QStringLiteral("treatWarningsAsErrors")).toBool(), true);
}

void TestTools::testProcessNameByPid()
{
    QCOMPARE(qAppName(), processNameByPid(QCoreApplication::applicationPid()));
}


int toNumber(const QString &str)
{
    int res = 0;
    for (const QChar &c : str)
        res = (res * 10) + c.digitValue();
    return res;
}

void TestTools::set_operator_eq()
{
    {
        Set<int> set1, set2;
        QVERIFY(set1 == set2);
        QVERIFY(!(set1 != set2));

        set1.insert(1);
        QVERIFY(set1 != set2);
        QVERIFY(!(set1 == set2));

        set2.insert(1);
        QVERIFY(set1 == set2);
        QVERIFY(!(set1 != set2));

        set2.insert(1);
        QVERIFY(set1 == set2);
        QVERIFY(!(set1 != set2));

        set1.insert(2);
        QVERIFY(set1 != set2);
        QVERIFY(!(set1 == set2));
    }

    {
        Set<QString> set1, set2;
        QVERIFY(set1 == set2);
        QVERIFY(!(set1 != set2));

        set1.insert("one");
        QVERIFY(set1 != set2);
        QVERIFY(!(set1 == set2));

        set2.insert("one");
        QVERIFY(set1 == set2);
        QVERIFY(!(set1 != set2));

        set2.insert("one");
        QVERIFY(set1 == set2);
        QVERIFY(!(set1 != set2));

        set1.insert("two");
        QVERIFY(set1 != set2);
        QVERIFY(!(set1 == set2));
    }

    {
        Set<QString> a;
        Set<QString> b;

        a += "otto";
        b += "willy";

        QVERIFY(a != b);
        QVERIFY(!(a == b));
    }

    {
        Set<int> s1, s2;
        s1.reserve(100);
        s2.reserve(4);
        QVERIFY(s1 == s2);
        s1 << 100 << 200 << 300 << 400;
        s2 << 400 << 300 << 200 << 100;
        QVERIFY(s1 == s2);
    }
}

void TestTools::set_swap()
{
    Set<int> s1, s2;
    s1.insert(1);
    s2.insert(2);
    s1.swap(s2);
    QCOMPARE(*s1.begin(),2);
    QCOMPARE(*s2.begin(),1);
}

void TestTools::set_size()
{
    Set<int> set;
    QVERIFY(set.size() == 0);
    QVERIFY(set.empty());
    QVERIFY(set.size() == set.size());

    set.insert(1);
    QVERIFY(set.size() == 1);
    QVERIFY(!set.empty());
    QVERIFY(set.size() == set.size());

    set.insert(1);
    QVERIFY(set.size() == 1);
    QVERIFY(!set.empty());
    QVERIFY(set.size() == set.size());

    set.insert(2);
    QVERIFY(set.size() == 2);
    QVERIFY(!set.empty());
    QVERIFY(set.size() == set.size());

    set.remove(1);
    QVERIFY(set.size() == 1);
    QVERIFY(!set.empty());
    QVERIFY(set.size() == set.size());

    set.remove(1);
    QVERIFY(set.size() == 1);
    QVERIFY(!set.empty());
    QVERIFY(set.size() == set.size());

    set.remove(2);
    QVERIFY(set.size() == 0);
    QVERIFY(set.empty());
    QVERIFY(set.size() == set.size());
}

void TestTools::set_capacity()
{
    Set<int> set;
    size_t n = set.capacity();
    QVERIFY(n == 0);

    for (int i = 0; i < 1000; ++i) {
        set.insert(i);
        QVERIFY(set.capacity() >= set.size());
    }
}

void TestTools::set_reserve()
{
    Set<int> set;

    set.reserve(1000);
    QVERIFY(set.capacity() >= 1000);

    for (int i = 0; i < 500; ++i)
        set.insert(i);

    QVERIFY(set.capacity() >= 1000);

    for (int j = 0; j < 500; ++j)
        set.remove(j);

    QVERIFY(set.capacity() >= 1000);
}

void TestTools::set_clear()
{
    Set<QString> set1, set2;
    QVERIFY(set1.size() == 0);

    set1.clear();
    QVERIFY(set1.size() == 0);

    set1.insert("foo");
    QVERIFY(set1.size() != 0);

    set2 = set1;

    set1.clear();
    QVERIFY(set1.size() == 0);
    QVERIFY(set2.size() != 0);

    set2.clear();
    QVERIFY(set1.size() == 0);
    QVERIFY(set2.size() == 0);
}

void TestTools::set_remove()
{
    Set<QString> set1;

    const size_t max = 500;

    for (size_t i = 0; i < max; ++i)
        set1.insert(QString::number(i));

    QCOMPARE(set1.size(), max);

    for (size_t j = 0; j < max; ++j) {
        set1.remove(QString::number((j * 17) % max));
        QCOMPARE(set1.size(), max - j - 1);
    }
}

void TestTools::set_contains()
{
    Set<QString> set1;

    for (int i = 0; i < 500; ++i) {
        QVERIFY(!set1.contains(QString::number(i)));
        set1.insert(QString::number(i));
        QVERIFY(set1.contains(QString::number(i)));
    }

    QCOMPARE(set1.size(), size_t { 500 });

    for (int j = 0; j < 500; ++j) {
        int i = (j * 17) % 500;
        QVERIFY(set1.contains(QString::number(i)));
        set1.remove(QString::number(i));
        QVERIFY(!set1.contains(QString::number(i)));
    }
}

void TestTools::set_containsSet()
{
    Set<QString> set1;
    Set<QString> set2;

    // empty set contains the empty set
    QVERIFY(set1.contains(set2));

    for (int i = 0; i < 500; ++i) {
        set1.insert(QString::number(i));
        set2.insert(QString::number(i));
    }
    QVERIFY(set1.contains(set2));

    set2.remove(QString::number(19));
    set2.remove(QString::number(82));
    set2.remove(QString::number(7));
    QVERIFY(set1.contains(set2));

    set1.remove(QString::number(23));
    QVERIFY(!set1.contains(set2));

    // filled set contains the empty set as well
    Set<QString> set3;
    QVERIFY(set1.contains(set3));

    // the empty set doesn't contain a filled set
    QVERIFY(!set3.contains(set1));

    // verify const signature
    const Set<QString> set4;
    QVERIFY(set3.contains(set4));
}

void TestTools::set_begin()
{
    Set<int> set1;
    Set<int> set2 = set1;

    {
        const auto i = set1.constBegin();
        const auto j = set1.cbegin();
        const auto k = set2.constBegin();
        const auto ell = set2.cbegin();

        QVERIFY(i == j);
        QVERIFY(k == ell);
    }

    set1.insert(44);

    {
        const auto i = set1.constBegin();
        const auto j = set1.cbegin();
        const auto k = set2.constBegin();
        const auto ell = set2.cbegin();

        QVERIFY(i == j);
        QVERIFY(k == ell);
    }

    set2 = set1;

    {
        const auto i = set1.constBegin();
        const auto j = set1.cbegin();
        const auto k = set2.constBegin();
        const auto ell = set2.cbegin();

        QVERIFY(i == j);
        QVERIFY(k == ell);
    }
}

void TestTools::set_end()
{
    Set<int> set1;
    Set<int> set2 = set1;

    {
        const auto i = set1.constEnd();
        const auto j = set1.cend();
        const auto k = set2.constEnd();
        const auto ell = set2.cend();

        QVERIFY(i == j);
        QVERIFY(k == ell);

        QVERIFY(set1.constBegin() == set1.constEnd());
        QVERIFY(set2.constBegin() == set2.constEnd());
    }

    set1.insert(44);

    {
        const auto i = set1.constEnd();
        const auto j = set1.cend();
        const auto k = set2.constEnd();
        const auto ell = set2.cend();

        QVERIFY(i == j);
        QVERIFY(k == ell);

        QVERIFY(set1.constBegin() != set1.constEnd());
        QVERIFY(set2.constBegin() == set2.constEnd());
    }

    set2 = set1;

    {
        const auto i = set1.constEnd();
        const auto j = set1.cend();
        const auto k = set2.constEnd();
        const auto ell = set2.cend();

        QVERIFY(i == j);
        QVERIFY(k == ell);

        QVERIFY(set1.constBegin() != set1.constEnd());
        QVERIFY(set2.constBegin() != set2.constEnd());
    }

    set1.clear();
    set2.clear();
    QVERIFY(set1.constBegin() == set1.constEnd());
    QVERIFY(set2.constBegin() == set2.constEnd());
}

struct IdentityTracker {
    int value, id;
};
inline bool operator==(IdentityTracker lhs, IdentityTracker rhs) { return lhs.value == rhs.value; }
inline bool operator<(IdentityTracker lhs, IdentityTracker rhs) { return lhs.value < rhs.value; }

void TestTools::set_insert()
{
    {
        Set<int> set1;
        QVERIFY(set1.size() == 0);
        set1.insert(1);
        QVERIFY(set1.size() == 1);
        set1.insert(2);
        QVERIFY(set1.size() == 2);
        set1.insert(2);
        QVERIFY(set1.size() == 2);
        QVERIFY(set1.contains(2));
        set1.remove(2);
        QVERIFY(set1.size() == 1);
        QVERIFY(!set1.contains(2));
        set1.insert(2);
        QVERIFY(set1.size() == 2);
        QVERIFY(set1.contains(2));
    }

    {
        Set<int> set1;
        QVERIFY(set1.size() == 0);
        set1 << 1;
        QVERIFY(set1.size() == 1);
        set1 << 2;
        QVERIFY(set1.size() == 2);
        set1 << 2;
        QVERIFY(set1.size() == 2);
        QVERIFY(set1.contains(2));
        set1.remove(2);
        QVERIFY(set1.size() == 1);
        QVERIFY(!set1.contains(2));
        set1 << 2;
        QVERIFY(set1.size() == 2);
        QVERIFY(set1.contains(2));
    }

    {
        Set<IdentityTracker> set;
        QCOMPARE(set.size(), size_t { 0 });
        const int dummy = -1;
        IdentityTracker id00 = {0, 0}, id01 = {0, 1}, searchKey = {0, dummy};
        QCOMPARE(set.insert(id00).first->id, id00.id);
        QCOMPARE(set.size(), size_t { 1 });
        QCOMPARE(set.insert(id01).first->id, id00.id); // first inserted is kept
        QCOMPARE(set.size(), size_t { 1 });
        QCOMPARE(set.find(searchKey)->id, id00.id);
    }
}

void TestTools::set_reverseIterators()
{
    Set<int> s;
    s << 1 << 17 << 61 << 127 << 911;
    std::vector<int> v(s.begin(), s.end());
    std::reverse(v.begin(), v.end());
    const Set<int> &cs = s;
    QVERIFY(std::equal(v.begin(), v.end(), s.rbegin()));
    QVERIFY(std::equal(v.begin(), v.end(), s.crbegin()));
    QVERIFY(std::equal(v.begin(), v.end(), cs.rbegin()));
    QVERIFY(std::equal(s.rbegin(), s.rend(), v.begin()));
    QVERIFY(std::equal(s.crbegin(), s.crend(), v.begin()));
    QVERIFY(std::equal(cs.rbegin(), cs.rend(), v.begin()));
}

void TestTools::set_stlIterator()
{
    Set<QString> set1;
    for (int i = 0; i < 25000; ++i)
        set1.insert(QString::number(i));

    {
        int sum = 0;
        auto i = set1.cbegin();
        while (i != set1.end()) {
            sum += toNumber(*i);
            ++i;
        }
        QVERIFY(sum == 24999 * 25000 / 2);
    }

    {
        int sum = 0;
        auto i = set1.cend();
        while (i != set1.begin()) {
            --i;
            sum += toNumber(*i);
        }
        QVERIFY(sum == 24999 * 25000 / 2);
    }
}

void TestTools::set_stlMutableIterator()
{
    Set<QString> set1;
    for (int i = 0; i < 25000; ++i)
        set1.insert(QString::number(i));

    {
        int sum = 0;
        auto i = set1.begin();
        while (i != set1.end()) {
            sum += toNumber(*i);
            ++i;
        }
        QVERIFY(sum == 24999 * 25000 / 2);
    }

    {
        int sum = 0;
        auto i = set1.end();
        while (i != set1.begin()) {
            --i;
            sum += toNumber(*i);
        }
        QVERIFY(sum == 24999 * 25000 / 2);
    }

    {
        Set<QString> set2 = set1;
        Set<QString> set3 = set2;

        auto i = set2.begin();
        auto j = set3.begin();

        while (i != set2.end()) {
            i = set2.erase(i);
        }
        QVERIFY(set2.empty());
        QVERIFY(!set3.empty());

        j = set3.end();
        while (j != set3.begin()) {
            j--;
            if (j + 1 != set3.end())
                set3.erase(j + 1);
        }
        if (set3.begin() != set3.end())
            set3.erase(set3.begin());

        QVERIFY(set2.empty());
        QVERIFY(set3.empty());

        i = set2.insert("foo").first;
        QCOMPARE(*i, QLatin1String("foo"));
    }
}

void TestTools::set_setOperations()
{
    Set<QString> set1, set2;
    set1 << "alpha" << "beta" << "gamma" << "delta"              << "zeta"           << "omega";
    set2            << "beta" << "gamma" << "delta" << "epsilon"           << "iota" << "omega";

    Set<QString> set3 = set1;
    set3.unite(set2);
    QVERIFY(set3.size() == 8);
    QVERIFY(set3.contains("alpha"));
    QVERIFY(set3.contains("beta"));
    QVERIFY(set3.contains("gamma"));
    QVERIFY(set3.contains("delta"));
    QVERIFY(set3.contains("epsilon"));
    QVERIFY(set3.contains("zeta"));
    QVERIFY(set3.contains("iota"));
    QVERIFY(set3.contains("omega"));

    Set<QString> set4 = set2;
    set4.unite(set1);
    QVERIFY(set4.size() == 8);
    QVERIFY(set4.contains("alpha"));
    QVERIFY(set4.contains("beta"));
    QVERIFY(set4.contains("gamma"));
    QVERIFY(set4.contains("delta"));
    QVERIFY(set4.contains("epsilon"));
    QVERIFY(set4.contains("zeta"));
    QVERIFY(set4.contains("iota"));
    QVERIFY(set4.contains("omega"));

    QVERIFY(set3 == set4);

    Set<QString> set5 = set1;
    set5.intersect(set2);
    QVERIFY(set5.size() == 4);
    QVERIFY(set5.contains("beta"));
    QVERIFY(set5.contains("gamma"));
    QVERIFY(set5.contains("delta"));
    QVERIFY(set5.contains("omega"));

    Set<QString> set6 = set2;
    set6.intersect(set1);
    QVERIFY(set6.size() == 4);
    QVERIFY(set6.contains("beta"));
    QVERIFY(set6.contains("gamma"));
    QVERIFY(set6.contains("delta"));
    QVERIFY(set6.contains("omega"));

    QVERIFY(set5 == set6);

    Set<QString> set7 = set1;
    set7.subtract(set2);
    QVERIFY(set7.size() == 2);
    QVERIFY(set7.contains("alpha"));
    QVERIFY(set7.contains("zeta"));

    Set<QString> set8 = set2;
    set8.subtract(set1);
    QVERIFY(set8.size() == 2);
    QVERIFY(set8.contains("epsilon"));
    QVERIFY(set8.contains("iota"));

    Set<QString> set9 = set1 | set2;
    QVERIFY(set9 == set3);

    Set<QString> set10 = set1 & set2;
    QVERIFY(set10 == set5);

    Set<QString> set11 = set1 + set2;
    QVERIFY(set11 == set3);

    Set<QString> set12 = set1 - set2;
    QVERIFY(set12 == set7);

    Set<QString> set13 = set2 - set1;
    QVERIFY(set13 == set8);

    Set<QString> set14 = set1;
    set14 |= set2;
    QVERIFY(set14 == set3);

    Set<QString> set15 = set1;
    set15 &= set2;
    QVERIFY(set15 == set5);

    Set<QString> set16 = set1;
    set16 += set2;
    QVERIFY(set16 == set3);

    Set<QString> set17 = set1;
    set17 -= set2;
    QVERIFY(set17 == set7);

    Set<QString> set18 = set2;
    set18 -= set1;
    QVERIFY(set18 == set8);
}

void TestTools::set_makeSureTheComfortFunctionsCompile()
{
    Set<int> set1, set2, set3;
    set1 << 5;
    set1 |= set2;
    set1 |= 5;
    set1 &= set2;
    set1 &= 5;
    set1 += set2;
    set1 += 5;
    set1 -= set2;
    set1 -= 5;
    set1 = set2 | set3;
    set1 = set2 & set3;
    set1 = set2 + set3;
    set1 = set2 - set3;
}

void TestTools::set_initializerList()
{
    Set<int> set = {1, 1, 2, 3, 4, 5};
    QCOMPARE(set.size(), size_t { 5 });
    QVERIFY(set.contains(1));
    QVERIFY(set.contains(2));
    QVERIFY(set.contains(3));
    QVERIFY(set.contains(4));
    QVERIFY(set.contains(5));

    // check _which_ of the equal elements gets inserted (in the QHash/QMap case, it's the last):
    const Set<IdentityTracker> set2 = {{1, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}};
    QCOMPARE(set2.size(), size_t { 5 });
    const int dummy = -1;
    const IdentityTracker searchKey = {1, dummy};
    QCOMPARE(set2.find(searchKey)->id, 0);

    Set<int> emptySet{};
    QVERIFY(emptySet.empty());

    Set<int> set3{{}, {}, {}};
    QVERIFY(!set3.empty());
}

void TestTools::set_intersects()
{
    Set<int> s1;
    Set<int> s2;

    QVERIFY(!s1.intersects(s1));
    QVERIFY(!s1.intersects(s2));

    s1 << 100;
    QVERIFY(s1.intersects(s1));
    QVERIFY(!s1.intersects(s2));

    s2 << 200;
    QVERIFY(!s1.intersects(s2));

    s1 << 200;
    QVERIFY(s1.intersects(s2));

    Set<int> s3;
    s3 << 500;
    QVERIFY(!s1.intersects(s3));
    s3 << 200;
    QVERIFY(s1.intersects(s3));
}

void TestTools::stringutils_join()
{
    QFETCH(std::vector<std::string>, input);
    QFETCH(std::string, separator);
    QFETCH(std::string, expectedResult);

    QCOMPARE(join(input, separator), expectedResult);
}

void TestTools::stringutils_join_data()
{
    QTest::addColumn<std::vector<std::string>>("input");
    QTest::addColumn<std::string>("separator");
    QTest::addColumn<std::string>("expectedResult");

    QTest::newRow("data1")
                << std::vector<std::string>()
                << std::string()
                << std::string();

    QTest::newRow("data2")
                << std::vector<std::string>()
                << std::string("separator")
                << std::string();

    QTest::newRow("data3")
                << std::vector<std::string>({"one"})
                << std::string("separator")
                << std::string("one");

    QTest::newRow("data4")
                << std::vector<std::string>({"one"})
                << std::string("separator")
                << std::string("one");


    QTest::newRow("data5")
                << std::vector<std::string>({"a", "b"})
                << std::string(" ")
                << std::string("a b");

    QTest::newRow("data6")
                << std::vector<std::string>({"a", "b", "c"})
                << std::string(" ")
                << std::string("a b c");
}

void TestTools::stringutils_join_empty()
{
    std::vector<std::string> list;
    std::string string = join(list, std::string());

    QVERIFY(string.empty());
}

void TestTools::stringutils_join_char()
{
    QFETCH(std::vector<std::string>, input);
    QFETCH(char, separator);
    QFETCH(std::string, expectedResult);

    QCOMPARE(join(input, separator), expectedResult);
}

void TestTools::stringutils_join_char_data()
{
    QTest::addColumn<std::vector<std::string>>("input");
    QTest::addColumn<char>("separator");
    QTest::addColumn<std::string>("expectedResult");

    QTest::newRow("data1")
                << std::vector<std::string>()
                << ' '
                << std::string();

    QTest::newRow("data5")
                << std::vector<std::string>({"a", "b"})
                << ' '
                << std::string("a b");

    QTest::newRow("data6")
                << std::vector<std::string>({"a", "b", "c"})
                << ' '
                << std::string("a b c");
}

void TestTools::stringutils_startsWith()
{
    std::string a;
    a = "AB";
    QVERIFY( startsWith(a, "A") );
    QVERIFY( startsWith(a, "AB") );
    QVERIFY( !startsWith(a, "C") );
    QVERIFY( !startsWith(a, "ABCDEF") );
    QVERIFY( startsWith(a, "") );
    QVERIFY( startsWith(a, 'A') );
    QVERIFY( !startsWith(a, 'C') );
    QVERIFY( !startsWith(a, char()) );

    QVERIFY( startsWith(a, "A") );
    QVERIFY( startsWith(a, "AB") );
    QVERIFY( !startsWith(a, "C") );
    QVERIFY( !startsWith(a, "ABCDEF") );
    QVERIFY( startsWith(a, "") );

    a = "";
    QVERIFY( startsWith(a, "") );
    QVERIFY( !startsWith(a, "ABC") );

    QVERIFY( startsWith(a, "") );
    QVERIFY( !startsWith(a, "ABC") );

    QVERIFY( !startsWith(a, 'x') );
    QVERIFY( !startsWith(a, char()) );

    a = std::string();
    QVERIFY( startsWith(a, "") ); // different from QString::startsWith
    QVERIFY( !startsWith(a, "ABC") );

    QVERIFY( !startsWith(a, 'x') );
    QVERIFY( !startsWith(a, char()) );

    a = u8"\xc3\xa9";
    QVERIFY( startsWith(a, u8"\xc3\xa9") );
    QVERIFY( !startsWith(a, u8"\xc3\xa1") );
}

void TestTools::stringutils_endsWith()
{
    std::string a;
    a = "AB";
    QVERIFY( endsWith(a, "B") );
    QVERIFY( endsWith(a, "AB") );
    QVERIFY( !endsWith(a, "C") );
    QVERIFY( !endsWith(a, "ABCDEF") );
    QVERIFY( endsWith(a, "") );
    QVERIFY( endsWith(a, 'B') );
    QVERIFY( !endsWith(a, 'C') );
    QVERIFY( !endsWith(a, char()) );

    QVERIFY( endsWith(a, "B") );
    QVERIFY( endsWith(a, "AB") );
    QVERIFY( !endsWith(a, "C") );
    QVERIFY( !endsWith(a, "ABCDEF") );
    QVERIFY( endsWith(a, "") );

    a = "";
    QVERIFY( endsWith(a, "") );
    QVERIFY( !endsWith(a, "ABC") );
    QVERIFY( !endsWith(a, 'x') );
    QVERIFY( !endsWith(a, char()) );

    QVERIFY( endsWith(a, "") );
    QVERIFY( !endsWith(a, "ABC") );

    a = std::string();
    QVERIFY( endsWith(a, "") ); // different from QString::endsWith
    QVERIFY( !endsWith(a, "ABC") );

    QVERIFY( !endsWith(a, 'x') );
    QVERIFY( !endsWith(a, char()) );

    a = u8"\xc3\xa9";
    QVERIFY( endsWith(a, u8"\xc3\xa9") );
    QVERIFY( !endsWith(a, u8"\xc3\xa1") );
}

void TestTools::stringutils_trimmed()
{
    std::string a;
    a = "Text";
    QCOMPARE(a, std::string("Text"));
    QCOMPARE(trimmed(a), std::string("Text"));
    QCOMPARE(a, std::string("Text"));
    a = " ";
    QCOMPARE(trimmed(a), std::string(""));
    QCOMPARE(a, std::string(" "));
    a = " a   ";
    QCOMPARE(trimmed(a), std::string("a"));

    a = "Text";
    QCOMPARE(trimmed(std::move(a)), std::string("Text"));
    a = " ";
    QCOMPARE(trimmed(std::move(a)), std::string(""));
    a = " a   ";
    QCOMPARE(trimmed(std::move(a)), std::string("a"));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const SettingsPtr s = settings();
    TestTools tt(s.get());
    return QTest::qExec(&tt, argc, argv);
}
