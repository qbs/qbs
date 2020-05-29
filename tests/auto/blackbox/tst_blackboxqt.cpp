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
#include <tools/preferences.h>
#include <tools/profile.h>

#include <QtCore/qjsondocument.h>

#define WAIT_FOR_NEW_TIMESTAMP() waitForNewTimestamp(testDataDir)

using qbs::Internal::HostOsInfo;
using qbs::Profile;

TestBlackboxQt::TestBlackboxQt() : TestBlackboxBase (SRCDIR "/testdata-qt", "blackbox-qt")
{
    setNeedsQt();
}

void TestBlackboxQt::addQObjectMacroToGeneratedCppFile()
{
    QDir::setCurrent(testDataDir + "/add-qobject-macro-to-generated-cpp-file");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("moc"), m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("object.cpp.in", "// ", "");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("moc"), m_qbsStdout.constData());
}

void TestBlackboxQt::autoQrc()
{
    QDir::setCurrent(testDataDir + "/auto-qrc");
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    if (m_qbsStdout.contains("targetPlatform differs from hostPlatform"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QCOMPARE(runQbs(QbsRunParameters("run", QStringList{"-p", "app"})), 0);
    QVERIFY2(m_qbsStdout.simplified().contains("resource data: resource1 resource2"),
             m_qbsStdout.constData());
}

void TestBlackboxQt::cachedQml()
{
    QDir::setCurrent(testDataDir + "/cached-qml");
    QCOMPARE(runQbs(), 0);
    QString dataDir = relativeBuildDir() + "/install-root/data";
    QVERIFY2(m_qbsStdout.contains("qmlcachegen must work: true")
             || m_qbsStdout.contains("qmlcachegen must work: false"),
             m_qbsStdout.constData());
    if (m_qbsStdout.contains("qmlcachegen must work: false")
            && QFile::exists(dataDir + "/main.cpp")) {
        // If C++ source files were installed then Qt.qmlcache is not available. See project file.
        QSKIP("No QML cache files generated.");
    }
    QVERIFY(QFile::exists(dataDir + "/main.qmlc"));
    QVERIFY(QFile::exists(dataDir + "/MainForm.ui.qmlc"));
    QVERIFY(QFile::exists(dataDir + "/stuff.jsc"));
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
    QVERIFY(QFile::copy(testSourceDir + "/../../../../examples/helloworld-qt/main.cpp",
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

void TestBlackboxQt::forcedMoc()
{
    QDir::setCurrent(testDataDir + "/forced-moc");
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    if (m_qbsStdout.contains("targetPlatform differs from hostPlatform"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QVERIFY2(m_qbsStderr.contains("Hello from slot"), m_qbsStderr.constData());
}

void TestBlackboxQt::includedMocCpp()
{
    QDir::setCurrent(testDataDir + "/included-moc-cpp");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling moc_myobject.cpp"), m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("myobject.cpp", "#include <moc_myobject.cpp", "// #include <moc_myobject.cpp");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling moc_myobject.cpp"), m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("myobject.cpp", "// #include <moc_myobject.cpp", "#include <moc_myobject.cpp");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling moc_myobject.cpp"), m_qbsStdout.constData());
}

void TestBlackboxQt::linkerVariant()
{
    QDir::setCurrent(testDataDir + "/linker-variant");
    QCOMPARE(runQbs(QStringList{"--command-echo-mode", "command-line"}), 0);
    const bool goldRequired = m_qbsStdout.contains("Qt requires gold: true");
    const bool goldNotRequired = m_qbsStdout.contains("Qt requires gold: false");
    QVERIFY2(goldRequired != goldNotRequired, m_qbsStdout.constData());
    QCOMPARE(m_qbsStdout.contains("-fuse-ld=gold"), goldRequired);
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

void TestBlackboxQt::metaTypes_data()
{
    QTest::addColumn<bool>("generate");
    QTest::addColumn<QString>("installDir");
    QTest::newRow("don't generate") << false << QString();
    QTest::newRow("don't generate with install info") << false << QString("blubb");
    QTest::newRow("generate only") << true << QString();
    QTest::newRow("generate and install") << true << QString("blubb");
}

void TestBlackboxQt::metaTypes()
{
    QDir::setCurrent(testDataDir + "/metatypes");
    QFETCH(bool, generate);
    QFETCH(QString, installDir);
    const QStringList args{"modules.Qt.core.generateMetaTypesFile:"
                               + QString(generate ? "true" : "false"),
                           "modules.Qt.core.metaTypesInstallDir:" + installDir,
                           "-v", "--force-probe-execution"};
    QCOMPARE(runQbs(QbsRunParameters("resolve", args)), 0);
    const bool canGenerate = m_qbsStdout.contains("can generate");
    const bool cannotGenerate = m_qbsStdout.contains("cannot generate");
    QVERIFY(canGenerate != cannotGenerate);
    const bool expectFiles = generate && canGenerate;
    const bool expectInstalledFiles = expectFiles && !installDir.isEmpty();
    QCOMPARE(runQbs(QStringList("--clean-install-root")), 0);
    const QString productDir = relativeProductBuildDir("mylib");
    const QString outputDir =  productDir + "/qt.headers";
    QVERIFY(!regularFileExists(outputDir + "/moc_unmocableclass.cpp.json"));
    QCOMPARE(regularFileExists(outputDir + "/moc_mocableclass1.cpp.json"), expectFiles);
    QCOMPARE(regularFileExists(outputDir + "/mocableclass2.moc.json"), expectFiles);
    QCOMPARE(regularFileExists(productDir + "/mylib_metatypes.json"), expectFiles);
    QCOMPARE(regularFileExists(relativeBuildDir() + "/install-root/some-prefix/" + installDir
                               + "/mylib_metatypes.json"), expectInstalledFiles);
}

void TestBlackboxQt::mixedBuildVariants()
{
    QDir::setCurrent(testDataDir + "/mixed-build-variants");
    const SettingsPtr s = settings();
    Profile profile(profileName(), s.get());
    if (profileToolchain(profile).contains("msvc")) {
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

void TestBlackboxQt::mocAndCppCombining()
{
    QDir::setCurrent(testDataDir + "/moc-and-cxx-combining");
    QCOMPARE(runQbs(), 0);
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

void TestBlackboxQt::mocSameFileName()
{
    QDir::setCurrent(testDataDir + "/moc-same-file-name");
    QCOMPARE(runQbs(), 0);
    QCOMPARE(m_qbsStdout.count("compiling moc_someclass.cpp"), 2);
}

void TestBlackboxQt::pkgconfig()
{
    QDir::setCurrent(testDataDir + "/pkgconfig");
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    if (m_qbsStdout.contains("targetPlatform differs from hostPlatform"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QbsRunParameters params;
    params.command = "run";
    QCOMPARE(runQbs(params), 0);
    if (m_qbsStdout.contains("targetPlatform differs from hostPlatform"))
        QSKIP("pkgconfig or Qt not found");
}

void TestBlackboxQt::pluginMetaData()
{
    QDir::setCurrent(testDataDir + "/plugin-meta-data");
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    if (m_qbsStdout.contains("targetPlatform differs from hostPlatform"))
        QSKIP("Cannot run binaries in cross-compiled build");

    QVERIFY2(runQbs(QbsRunParameters("run", QStringList{"-p", "app"})) == 0,
             m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.contains("all ok!"), m_qbsStderr.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    touch("metadata.json");
    waitForFileUnlock();
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("moc"), m_qbsStdout.constData());
}

void TestBlackboxQt::pluginSupport_data()
{
    QTest::addColumn<bool>("invalidPlugin");
    QTest::newRow("request valid plugins") << false;
    QTest::newRow("request invalid plugin") << true;
}

void TestBlackboxQt::pluginSupport()
{
    QDir::setCurrent(testDataDir + "/plugin-support");
    QFETCH(bool, invalidPlugin);
    QbsRunParameters resolveParams("resolve");
    if (invalidPlugin) {
        resolveParams.arguments << "modules.m1.useDummy:true";
        resolveParams.expectFailure = true;
    }
    QCOMPARE(runQbs(resolveParams) == 0, !invalidPlugin);
    if (invalidPlugin) {
        QVERIFY2(m_qbsStderr.contains("Plugin 'dummy' of type 'imageformats' was requested, "
                                      "but is not available"), m_qbsStderr.constData());
        return;
    }
    const bool isStaticQt = m_qbsStdout.contains("static Qt: true");
    const bool isDynamicQt = m_qbsStdout.contains("static Qt: false");
    QVERIFY(isStaticQt != isDynamicQt);
    if (isStaticQt)
        QVERIFY2(m_qbsStdout.contains("platform plugin count: 1"), m_qbsStdout.constData());
    else
        QVERIFY2(m_qbsStdout.contains("platform plugin count: 0"), m_qbsStdout.constData());
    const auto extractList = [this](const char sep) {
        const int listStartIndex = m_qbsStdout.indexOf(sep);
        const int listEndIndex = m_qbsStdout.indexOf(sep, listStartIndex + 1);
        const QByteArray listString = m_qbsStdout.mid(listStartIndex + 1,
                                                      listEndIndex - listStartIndex - 1);
        return listString.isEmpty() ? QByteArrayList() : listString.split(',');
    };
    const QByteArrayList enabledPlugins = extractList('%');
    if (isStaticQt)
        QCOMPARE(enabledPlugins.size(), 2);
    else
        QVERIFY(enabledPlugins.empty());
    const QByteArrayList allPlugins = extractList('#');
    QVERIFY(allPlugins.size() >= enabledPlugins.size());
    QCOMPARE(runQbs(), 0);
    for (const QByteArray &plugin : allPlugins) {
        QCOMPARE(m_qbsStdout.contains("qt_plugin_import_" + plugin + ".cpp"),
                 enabledPlugins.contains(plugin));
    }
}

void TestBlackboxQt::qmlDebugging()
{
    QDir::setCurrent(testDataDir + "/qml-debugging");
    QCOMPARE(runQbs(), 0);
    const SettingsPtr s = settings();
    Profile profile(profileName(), s.get());
    if (!profileToolchain(profile).contains("gcc"))
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

void TestBlackboxQt::qmlTypeRegistrar_data()
{
    QTest::addColumn<QString>("importName");
    QTest::addColumn<QString>("installDir");
    QTest::newRow("don't generate") << QString() << QString();
    QTest::newRow("don't generate with install info") << QString() << QString("blubb");
    QTest::newRow("generate only") << QString("People") << QString();
    QTest::newRow("generate and install") << QString("People") << QString("blubb");
}

void TestBlackboxQt::qmlTypeRegistrar()
{
    QDir::setCurrent(testDataDir + "/qmltyperegistrar");
    QFETCH(QString, importName);
    QFETCH(QString, installDir);
    rmDirR(relativeBuildDir());
    const QStringList args{"modules.Qt.qml.importName:" + importName,
                           "modules.Qt.qml.typesInstallDir:" + installDir};
    QCOMPARE(runQbs(QbsRunParameters("resolve", args)), 0);
    const bool hasRegistrar = m_qbsStdout.contains("has registrar");
    const bool doesNotHaveRegistrar = m_qbsStdout.contains("does not have registrar");
    QVERIFY(hasRegistrar != doesNotHaveRegistrar);
    if (doesNotHaveRegistrar)
        QSKIP("Qt version too old");
    QCOMPARE(runQbs(), 0);
    const bool enabled = !importName.isEmpty();
    QCOMPARE(m_qbsStdout.contains("running qmltyperegistrar"), enabled);
    QCOMPARE(m_qbsStdout.contains("compiling myapp_qmltyperegistrations.cpp"), enabled);
    const QString buildDir = relativeProductBuildDir("myapp");
    QCOMPARE(regularFileExists(buildDir + "/myapp.qmltypes"), enabled);
    QCOMPARE(regularFileExists(relativeBuildDir() + "/install-root/" + installDir
                               + "/myapp.qmltypes"), enabled && !installDir.isEmpty());
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

void TestBlackboxQt::quickCompiler()
{
    QDir::setCurrent(testDataDir + "/quick-compiler");
    QCOMPARE(runQbs(), 0);
    const bool hasCompiler = m_qbsStdout.contains("compiler available");
    const bool doesNotHaveCompiler = m_qbsStdout.contains("compiler not available");
    QVERIFY2(hasCompiler || doesNotHaveCompiler, m_qbsStdout.constData());
    QCOMPARE(m_qbsStdout.contains("compiling qml_subdir_test_qml.cpp"), hasCompiler);
    if (doesNotHaveCompiler)
        QSKIP("qtquickcompiler not available");
    QVERIFY2(m_qbsStdout.contains("generating loader source"), m_qbsStdout.constData());

    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("generating loader source"), m_qbsStdout.constData());

    WAIT_FOR_NEW_TIMESTAMP();
    touch("qml/subdir/test.qml");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("generating loader source"), m_qbsStdout.constData());

    QCOMPARE(runQbs(QbsRunParameters(QStringList{"config:off",
                                                 "modules.Qt.quick.useCompiler:false"})), 0);
    QVERIFY2(m_qbsStdout.contains("compiling"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("compiling qml_subdir_test_qml.cpp"), m_qbsStdout.constData());
}

void TestBlackboxQt::qtScxml()
{
    QDir::setCurrent(testDataDir + "/qtscxml");
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    if (m_qbsStdout.contains("targetPlatform differs from hostPlatform"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QCOMPARE(runQbs(), 0);
    if (m_qbsStdout.contains("QtScxml not present"))
        QSKIP("QtScxml module not present");
    QVERIFY2(m_qbsStdout.contains("state machine name: qbs_test_machine"),
             m_qbsStdout.constData());
    QCOMPARE(runQbs(QbsRunParameters("resolve",
        QStringList("modules.Qt.scxml.additionalCompilerFlags:--blubb"))), 0);
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY2(runQbs(params) != 0, m_qbsStdout.constData());
}

void TestBlackboxQt::removeMocHeaderFromFileList()
{
    QDir::setCurrent(testDataDir + "/remove-moc-header-from-file-list");
    QCOMPARE(runQbs(), 0);
    QString projectFile("remove-moc-header-from-file-list.qbs");
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFile, "\"file.h\"", "// \"file.h\"");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFile, "// \"file.h\"", "\"file.h\"");
    QCOMPARE(runQbs(), 0);
}


void TestBlackboxQt::staticQtPluginLinking()
{
    QDir::setCurrent(testDataDir + "/static-qt-plugin-linking");
    QCOMPARE(runQbs(QStringList("products.p.type:application")), 0);
    const bool isStaticQt = m_qbsStdout.contains("Qt is static");
    QVERIFY2(m_qbsStdout.contains("Creating static import") == isStaticQt, m_qbsStdout.constData());
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("products.p.type:staticlibrary"))), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("Creating static import"), m_qbsStdout.constData());
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
    QString moc_bla_objectFileName = relativeProductBuildDir("i") + '/'
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
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    if (m_qbsStdout.contains("targetPlatform differs from hostPlatform"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QVERIFY2(m_qbsStdout.contains("rcc"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("compiling test.cpp"), m_qbsStdout.constData());
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
    REPLACE_IN_FILE("i.qbs", "//\"test.cpp\"", "\"test.cpp\"");
    waitForFileUnlock();
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QVERIFY2(m_qbsStdout.contains("rcc bla.qrc"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("compiling test.cpp"), m_qbsStdout.constData());
    QVERIFY(regularFileExists(fileName));
    QVERIFY(dt < QFileInfo(fileName).lastModified());

    WAIT_FOR_NEW_TIMESTAMP();
    touch("i.qbs");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("rcc"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("compiling test.cpp"), m_qbsStdout.constData());

    // Turn on big resources.
    WAIT_FOR_NEW_TIMESTAMP();
    QCOMPARE(runQbs(QbsRunParameters("resolve", {"modules.Qt.core.enableBigResources:true"})), 0);
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QVERIFY2(m_qbsStdout.contains("rcc (pass 1) bla.qrc"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("rcc (pass 2) bla.qrc"), m_qbsStdout.constData());

    // Check null build.
    WAIT_FOR_NEW_TIMESTAMP();
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Building"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("rcc"), m_qbsStdout.constData());

    // Turn off big resources.
    WAIT_FOR_NEW_TIMESTAMP();
    QCOMPARE(runQbs(QbsRunParameters("resolve", {"modules.Qt.core.enableBigResources:false"})), 0);
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QVERIFY2(m_qbsStdout.contains("rcc bla.qrc"), m_qbsStdout.constData());

    // Check null build.
    WAIT_FOR_NEW_TIMESTAMP();
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Building"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("rcc"), m_qbsStdout.constData());
}

void TestBlackboxQt::unmocable()
{
    QDir::setCurrent(testDataDir + "/unmocable");
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStderr.contains("No relevant classes found. No output generated."));
}

QTEST_MAIN(TestBlackboxQt)
