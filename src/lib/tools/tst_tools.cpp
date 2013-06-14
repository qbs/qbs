/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/
#include "tst_tools.h"

#include <tools/buildoptions.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/settings.h>
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
    bool exceptionCaught;
    Profile parentProfile("parent", m_settings);
    Profile childProfile("child", m_settings);
    try {
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
        Profile fooProfile("foo", m_settings);
        fooProfile.setValue("testKey", "gnampf");
        childProfile.setBaseProfile("foo");
        QCOMPARE(childProfile.value("testKey", "none").toString(), QLatin1String("gnampf"));
        exceptionCaught = false;
    } catch (ErrorInfo &) {
        exceptionCaught = true;
    }
    QVERIFY(!exceptionCaught);

    try {
        childProfile.setBaseProfile("SmurfAlongWithMe");
        childProfile.value("blubb");
        exceptionCaught = false;
    } catch (ErrorInfo &) {
        exceptionCaught = true;
    }
    QVERIFY(exceptionCaught);

    try {
        childProfile.setBaseProfile("parent");
        parentProfile.setBaseProfile("child");
        QVERIFY(!childProfile.value("blubb").isValid());
        exceptionCaught = false;
    } catch (ErrorInfo &) {
        exceptionCaught = true;
    }
    QVERIFY(exceptionCaught);

    try {
        QVERIFY(!childProfile.allKeys(Profile::KeySelectionNonRecursive).isEmpty());
        exceptionCaught = false;
    } catch (ErrorInfo &) {
        exceptionCaught = true;
    }
    QVERIFY(!exceptionCaught);

    try {
        QVERIFY(!childProfile.allKeys(Profile::KeySelectionRecursive).isEmpty());
        exceptionCaught = false;
    } catch (ErrorInfo &) {
        exceptionCaught = true;
    }
    QVERIFY(exceptionCaught);
}

} // namespace Internal
} // namespace qbs
