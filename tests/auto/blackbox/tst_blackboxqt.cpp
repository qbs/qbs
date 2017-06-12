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

#include "tst_blackboxqt.h"

#include "../shared.h"
#include <tools/hostosinfo.h>
#include <tools/profile.h>

#include <QtCore/qjsondocument.h>

#define WAIT_FOR_NEW_TIMESTAMP() waitForNewTimestamp(testDataDir)

using qbs::Internal::HostOsInfo;
using qbs::Profile;

TestBlackboxQt::TestBlackboxQt() : TestBlackboxBase (SRCDIR "/testdata-qt", "blackbox-qt")
{
}

void TestBlackboxQt::validateTestProfile()
{
    const SettingsPtr s = settings();
    if (!s->profiles().contains(profileName()))
        QFAIL(QByteArray("The build profile '" + profileName().toLocal8Bit() +
                         "' could not be found. Please set it up on your machine."));

    Profile buildProfile(profileName(), s.get());
    const QStringList searchPaths
            = buildProfile.value(QLatin1String("preferences.qbsSearchPaths")).toStringList();
    if (searchPaths.isEmpty())
        QFAIL(QByteArray("The build profile '" + profileName().toLocal8Bit() +
                         "' is not a valid Qt profile."));
    if (!QFileInfo(searchPaths.first()).isDir())
        QFAIL(QByteArray("The build profile '" + profileName().toLocal8Bit() +
                         "' points to an invalid qbs search path."));
}

void TestBlackboxQt::autoQrc()
{
    QDir::setCurrent(testDataDir + "/auto-qrc");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.simplified().contains("resource data: resource1 resource2"),
             m_qbsStdout.constData());
}

void TestBlackboxQt::combinedMoc()
{
    QDir::setCurrent(testDataDir + "/combined-moc");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling moc_theobject.cpp"));
    QVERIFY(!m_qbsStdout.contains("creating amalgamated_moc_theapp.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling amalgamated_moc_theapp.cpp"));
    QbsRunParameters params(QStringList("modules.Qt.core.combineMocOutput:true"));
    params.command = "resolve";
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling moc_theobject.cpp"));
    QVERIFY(m_qbsStdout.contains("creating amalgamated_moc_theapp.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling amalgamated_moc_theapp.cpp"));
}

void TestBlackboxQt::createProject()
{
    QDir::setCurrent(testDataDir + "/create-project");
    QVERIFY(QFile::copy(SRCDIR "/../../../examples/helloworld-qt/main.cpp",
                        QDir::currentPath() + "/main.cpp"));
    QbsRunParameters createParams("create-project");
    createParams.profile.clear();
    QCOMPARE(runQbs(createParams), 0);
    createParams.expectFailure = true;
    QVERIFY(runQbs(createParams) != 0);
    QVERIFY2(m_qbsStderr.contains("already contains qbs files"), m_qbsStderr.constData());
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling"), m_qbsStdout.constData());
}

void TestBlackboxQt::dbusAdaptors()
{
    QDir::setCurrent(testDataDir + "/dbus-adaptors");
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxQt::dbusInterfaces()
{
    QDir::setCurrent(testDataDir + "/dbus-interfaces");
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxQt::lrelease()
{
    QDir::setCurrent(testDataDir + QLatin1String("/lrelease"));
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(relativeProductBuildDir("lrelease-test") + "/de.qm"));
    QVERIFY(regularFileExists(relativeProductBuildDir("lrelease-test") + "/hu.qm"));

    QCOMPARE(runQbs(QString("clean")), 0);
    QbsRunParameters params(QStringList({"modules.Qt.core.lreleaseMultiplexMode:true"}));
    params.command = "resolve";
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(regularFileExists(relativeProductBuildDir("lrelease-test") + "/lrelease-test.qm"));
    QVERIFY(!regularFileExists(relativeProductBuildDir("lrelease-test") + "/de.qm"));
    QVERIFY(!regularFileExists(relativeProductBuildDir("lrelease-test") + "/hu.qm"));

    QCOMPARE(runQbs(QString("clean")), 0);
    params.command = "resolve";
    params.arguments << "modules.Qt.core.qmBaseName:somename";
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    params.arguments.clear();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(regularFileExists(relativeProductBuildDir("lrelease-test") + "/somename.qm"));
    QVERIFY(!regularFileExists(relativeProductBuildDir("lrelease-test") + "/lrelease-test.qm"));
    QVERIFY(!regularFileExists(relativeProductBuildDir("lrelease-test") + "/de.qm"));
    QVERIFY(!regularFileExists(relativeProductBuildDir("lrelease-test") + "/hu.qm"));
}

void TestBlackboxQt::mixedBuildVariants()
{
    QDir::setCurrent(testDataDir + "/mixed-build-variants");
    const SettingsPtr s = settings();
    Profile profile(profileName(), s.get());
    if (profile.value("qbs.toolchain").toStringList().contains("msvc")) {
        QbsRunParameters params;
        params.arguments << "qbs.buildVariant:debug";
        params.expectFailure = true;
        QVERIFY(runQbs(params) != 0);
        QVERIFY2(m_qbsStderr.contains("not allowed"), m_qbsStderr.constData());
    } else {
        QbsRunParameters params;
        params.expectFailure = true;
        QVERIFY(runQbs(params) != 0);
        QVERIFY2(m_qbsStderr.contains("not supported"), m_qbsStderr.constData());
    }
}

void TestBlackboxQt::mocFlags()
{
    QDir::setCurrent(testDataDir + "/moc-flags");
    QCOMPARE(runQbs(), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    QbsRunParameters params;
    params.expectFailure = true;
    params.arguments << "Qt.core.mocFlags:-E";
    QVERIFY(runQbs(params) != 0);
}

void TestBlackboxQt::pluginMetaData()
{
    QDir::setCurrent(testDataDir + "/plugin-meta-data");
    QCOMPARE(runQbs(), 0);
    const QString appFilePath = relativeBuildDir() + "/install-root/"
            + qbs::Internal::HostOsInfo::appendExecutableSuffix("plugin-meta-data");
    QVERIFY(regularFileExists(appFilePath));
    QProcess app;
    app.start(appFilePath);
    QVERIFY(app.waitForStarted());
    QVERIFY(app.waitForFinished());
    QVERIFY2(app.exitCode() == 0, app.readAllStandardError().constData());
    WAIT_FOR_NEW_TIMESTAMP();
    touch("metadata.json");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("moc"), m_qbsStdout.constData());
}

void TestBlackboxQt::qmlDebugging()
{
    QDir::setCurrent(testDataDir + "/qml-debugging");
    QCOMPARE(runQbs(), 0);
    const SettingsPtr s = settings();
    Profile profile(profileName(), s.get());
    if (!profile.value("qbs.toolchain").toStringList().contains("gcc"))
        return;
    QProcess nm;
    nm.start("nm", QStringList(relativeExecutableFilePath("debuggable-app")));
    if (nm.waitForStarted()) { // Let's ignore hosts without nm.
        QVERIFY2(nm.waitForFinished(), qPrintable(nm.errorString()));
        QVERIFY2(nm.exitCode() == 0, nm.readAllStandardError().constData());
        const QByteArray output = nm.readAllStandardOutput();
        QVERIFY2(output.toLower().contains("debugginghelper"), output.constData());
    }
}

void TestBlackboxQt::qobjectInObjectiveCpp()
{
    if (!HostOsInfo::isMacosHost())
        QSKIP("only applies on macOS");
    const QString testDir = testDataDir + "/qobject-in-mm";
    QDir::setCurrent(testDir);
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxQt::qtKeywords()
{
    QDir::setCurrent(testDataDir + "/qt-keywords");
    QbsRunParameters params(QStringList("modules.Qt.core.enableKeywords:false"));
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    params.arguments.clear();
    QVERIFY(runQbs(params) != 0);
    params.command = "resolve";
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
}

void TestBlackboxQt::qtScxml()
{
    QDir::setCurrent(testDataDir + "/qtscxml");
    QCOMPARE(runQbs(), 0);
    if (m_qbsStdout.contains("QtScxml not present"))
        QSKIP("QtScxml module not present");
    QVERIFY2(m_qbsStdout.contains("state machine name: qbs_test_machine"),
             m_qbsStdout.constData());
}


void TestBlackboxQt::staticQtPluginLinking()
{
    QDir::setCurrent(testDataDir + "/static-qt-plugin-linking");
    QCOMPARE(runQbs(), 0);
    const bool isStaticQt = m_qbsStdout.contains("Qt is static");
    QVERIFY2(m_qbsStdout.contains("Creating static import") == isStaticQt, m_qbsStdout.constData());
}

void TestBlackboxQt::trackAddMocInclude()
{
    QDir::setCurrent(testDataDir + "/trackAddMocInclude");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDataDir + "/trackAddMocInclude/work");
    // The build must fail because the main.moc include is missing.
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);

    WAIT_FOR_NEW_TIMESTAMP();
    ccp("../after", ".");
    touch("main.cpp");
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxQt::track_qobject_change()
{
    QDir::setCurrent(testDataDir + "/trackQObjChange");
    copyFileAndUpdateTimestamp("bla_qobject.h", "bla.h");
    QCOMPARE(runQbs(), 0);
    const QString productFilePath = relativeExecutableFilePath("i");
    QVERIFY2(regularFileExists(productFilePath), qPrintable(productFilePath));
    QString moc_bla_objectFileName = relativeProductBuildDir("i") + "/.obj/"
            + inputDirHash("qt.headers") + objectFileName("/moc_bla.cpp", profileName());
    QVERIFY2(regularFileExists(moc_bla_objectFileName), qPrintable(moc_bla_objectFileName));

    WAIT_FOR_NEW_TIMESTAMP();
    copyFileAndUpdateTimestamp("bla_noqobject.h", "bla.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(productFilePath));
    QVERIFY(!QFile(moc_bla_objectFileName).exists());
}

void TestBlackboxQt::track_qrc()
{
    QDir::setCurrent(testDataDir + "/qrc");
    QCOMPARE(runQbs(), 0);
    const QString fileName = relativeExecutableFilePath("i");
    QVERIFY2(regularFileExists(fileName), qPrintable(fileName));
    QDateTime dt = QFileInfo(fileName).lastModified();
    WAIT_FOR_NEW_TIMESTAMP();
    {
        QFile f("stuff.txt");
        f.remove();
        QVERIFY(f.open(QFile::WriteOnly));
        f.write("bla");
        f.close();
    }
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(fileName));
    QVERIFY(dt < QFileInfo(fileName).lastModified());
}

QTEST_MAIN(TestBlackboxQt)
