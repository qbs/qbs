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

#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>

#define WAIT_FOR_NEW_TIMESTAMP() waitForNewTimestamp(testDataDir)

using qbs::Internal::HostOsInfo;

TestBlackboxQt::TestBlackboxQt() : TestBlackboxBase (SRCDIR "/testdata-qt", "blackbox-qt")
{
    setNeedsQt();
}

std::optional<bool> TestBlackboxQt::findShaderTools(bool *hasViewCount)
{
    QDir::setCurrent(testDataDir);

    rmDirR(relativeBuildDir());
    QbsRunParameters checkParams;
    checkParams.command = "resolve";
    checkParams.arguments << QStringLiteral("-f") << "find-shadertools.qbs";
    if (runQbs(checkParams) != 0) {
        qWarning() << "fail to resolve find-shadertools.qbs" << m_qbsStderr;
        return std::nullopt;
    }
    if (!m_qbsStdout.contains("is qt6: ")) {
        qWarning() << "stdout does not contain 'is qt6:'" << m_qbsStdout;
        return std::nullopt;
    }
    if (!m_qbsStdout.contains("is static qt: ")) {
        qWarning() << "stdout does not contain 'is static qt:'" << m_qbsStdout;
        return std::nullopt;
    }
    if (!m_qbsStdout.contains("is shadertools present: ")) {
        qWarning() << "stdout does not contain 'is shadertools present:'" << m_qbsStdout;
        return std::nullopt;
    }
    if (!m_qbsStdout.contains("has viewCount: ")) {
        qWarning() << "stdout does not contain 'has viewCount:'" << m_qbsStdout;
        return std::nullopt;
    }

    const bool isQt6 = m_qbsStdout.contains("is qt6: true");
    const bool isStaticQt = m_qbsStdout.contains("is static qt: true");
    const bool isShadertoolsPresent = m_qbsStdout.contains("is shadertools present: true");
    if (hasViewCount) {
        *hasViewCount = m_qbsStdout.contains("has viewCount: true");
    }
    return isQt6 && !isStaticQt && isShadertoolsPresent;
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
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QCOMPARE(runQbs(QbsRunParameters("run", QStringList{"-p", "app"})), 0);
    QVERIFY2(m_qbsStdout.simplified().contains("resource data: resource1 resource2"),
             m_qbsStdout.constData());
}

void TestBlackboxQt::cachedQml()
{
    QDir::setCurrent(testDataDir + "/cached-qml");
    if ((runQbs() != 0) && m_qbsStderr.contains("Dependency 'Qt.qml' not found for product 'app'"))
        QSKIP("Qt version too old");

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

void TestBlackboxQt::emscriptenHtml()
{
    QDir::setCurrent(testDataDir + "/emscripten-html");
    QCOMPARE(runQbs(), 0);
    if (m_qbsStdout.contains("is emscripten: false"))
        QSKIP("Skipping emscripten test");
    QVERIFY(m_qbsStdout.contains("is emscripten: true"));

    const auto relativeInstallRoot = relativeBuildDir() + QStringLiteral("/install-root/");
    QVERIFY(!regularFileExists(relativeInstallRoot + QStringLiteral("qtloader.js")));
    QVERIFY(!regularFileExists(relativeInstallRoot + QStringLiteral("qtlogo.svg")));
    QVERIFY(!regularFileExists(relativeInstallRoot + QStringLiteral("app.html")));

    const QStringList params = {QStringLiteral("products.app.generateHtml:true")};
    QCOMPARE(runQbs(QbsRunParameters("resolve", params)), 0);
    QCOMPARE(runQbs(QbsRunParameters("build", params)), 0);

    QVERIFY(regularFileExists(relativeInstallRoot + QStringLiteral("qtloader.js")));
    QVERIFY(regularFileExists(relativeInstallRoot + QStringLiteral("qtlogo.svg")));
    QVERIFY(regularFileExists(relativeInstallRoot + QStringLiteral("app.html")));
}

void TestBlackboxQt::forcedMoc()
{
    QDir::setCurrent(testDataDir + "/forced-moc");
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    if (m_qbsStdout.contains("using qt4"))
        QSKIP("Qt version too old");
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QVERIFY2(m_qbsStderr.contains("Hello from slot"), m_qbsStderr.constData());
}

void TestBlackboxQt::includedMocCpp()
{
    QDir::setCurrent(testDataDir + "/included-moc-cpp");
    QCOMPARE(runQbs(), 0);
    if (m_qbsStdout.contains("using qt4"))
        QSKIP("Qt version too old");
    QVERIFY2(!m_qbsStdout.contains("compiling moc_myobject.cpp"), m_qbsStdout.constData());

    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("myobject.cpp", "#include <moc_myobject.cpp", "// #include <moc_myobject.cpp");
    QbsRunParameters failParams;
    failParams.expectFailure = true;
    QEXPECT_FAIL(nullptr, "not worth supporting", Continue);
    QCOMPARE(runQbs(failParams), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("myobject.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling moc_myobject.cpp"), m_qbsStdout.constData());

    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("myobject.cpp", "// #include <moc_myobject.cpp", "#include <moc_myobject.cpp");
    QEXPECT_FAIL(nullptr, "not worth supporting", Continue);
    QCOMPARE(runQbs(failParams), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("myobject.h");
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
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");

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

void TestBlackboxQt::lupdate()
{
    QDir::setCurrent(testDataDir + "/lupdate");

    // Initial run.
    QCOMPARE(runQbs(QbsRunParameters(QStringList{"-p", "lupdate-runner"})), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("creating lupdate project file"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("updating translation files"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("Found 4 source text"), m_qbsStdout.constData());
    QFile jsonFile(relativeProductBuildDir("lupdate-runner") + "/lupdate.json");
    QVERIFY2(jsonFile.open(QIODevice::ReadOnly), qPrintable(jsonFile.fileName()));
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonFile.readAll());
    QVERIFY(jsonDoc.isObject());
    QJsonObject mainObj = jsonDoc.object();
    QJsonArray tsFiles = mainObj.value("translations").toArray();
    QCOMPARE(tsFiles.size(), 1);
    QJsonArray mainSources = mainObj.value("sources").toArray();
    QCOMPARE(mainSources.size(), 1);
    QVERIFY(mainSources.at(0).toString().endsWith(".ui"));
    QJsonArray subProjects = mainObj.value("subProjects").toArray();
    QCOMPARE(subProjects.size(), 3);
    for (const QJsonValue &v : subProjects) {
        const QJsonObject subProject = v.toObject();
        const QString name = subProject.value("projectFile").toString();
        QVERIFY2(name == "cxx 1" || name == "cxx 2" || name == "cxx 3", qPrintable(name));
        const QJsonArray includePaths = subProject.value("includePaths").toArray();
        bool hasSubdir = false;
        bool hasP1Headers = false;
        bool hasP2Headers = false;
        for (const QJsonValue &v : includePaths) {
            const QString vString = v.toString();
            if (vString.contains("subdir"))
                hasSubdir = true;
            else if (vString.contains("qt1."))
                hasP1Headers = true;
            else if (vString.contains("qt2."))
                hasP2Headers = true;
        }
        QVERIFY(hasP1Headers != hasP2Headers);
        if (hasSubdir) {
            QVERIFY(hasP1Headers);
            QVERIFY(!hasP2Headers);
        }
        const QJsonArray sources = subProject.value("sources").toArray();
        const int expectedSourceCount = hasSubdir ? 1 : 3;
        if (sources.size() != expectedSourceCount)
            qDebug() << sources;
        QCOMPARE(sources.size(), expectedSourceCount);
    }
    jsonFile.close();

    // Run again. The JSON file should not get updated.
    QCOMPARE(runQbs(QbsRunParameters(QStringList{"-p", "lupdate-runner"})), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QVERIFY2(!m_qbsStdout.contains("creating lupdate project file"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("updating translation files"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("Found 4 source text"), m_qbsStdout.constData());

    // Add source file, verify that the JSON file gets updated.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("lupdate.qbs", "/*\"qt2-new.cpp\"*/", "\"qt2-new.cpp\"");
    QCOMPARE(runQbs(QbsRunParameters(QStringList{"-p", "lupdate-runner"})), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("creating lupdate project file"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("updating translation files"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("Found 5 source text"), m_qbsStdout.constData());

    // Change include paths, verify that the JSON file gets updated and the sources re-grouped.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(
        "lupdate.qbs", "/*cpp.includePaths: \"subdir\"*/", "cpp.includePaths: \"subdir\"");
    REPLACE_IN_FILE("lupdate.qbs", "cpp.includePaths: outer.concat(\"subdir\")", "");
    QCOMPARE(runQbs(QbsRunParameters(QStringList{"-p", "lupdate-runner"})), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("creating lupdate project file"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("updating translation files"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("Found 5 source text"), m_qbsStdout.constData());
    QVERIFY2(jsonFile.open(QIODevice::ReadOnly), qPrintable(jsonFile.fileName()));
    jsonDoc = QJsonDocument::fromJson(jsonFile.readAll());
    QVERIFY(jsonDoc.isObject());
    mainObj = jsonDoc.object();
    tsFiles = mainObj.value("translations").toArray();
    QCOMPARE(tsFiles.size(), 1);
    mainSources = mainObj.value("sources").toArray();
    QCOMPARE(mainSources.size(), 1);
    QVERIFY(mainSources.at(0).toString().endsWith(".ui"));
    subProjects = mainObj.value("subProjects").toArray();
    QCOMPARE(subProjects.size(), 2);
    for (const QJsonValue &v : subProjects) {
        const QJsonObject subProject = v.toObject();
        const QString name = subProject.value("projectFile").toString();
        QVERIFY2(name == "cxx 1" || name == "cxx 2", qPrintable(name));
        const QJsonArray includePaths = subProject.value("includePaths").toArray();
        const QJsonArray sources = subProject.value("sources").toArray();
        const int expectedSourceCount = 4;
        if (sources.size() != expectedSourceCount)
            qDebug() << sources;
        QCOMPARE(sources.size(), expectedSourceCount);
    }

    // Change an unrelated property. The JSON file should not get updated.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("lupdate.qbs", "// cpp.cxxLanguageVersion:", "cpp.cxxLanguageVersion:");
    QCOMPARE(runQbs(QbsRunParameters(QStringList{"-p", "lupdate-runner"})), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("creating lupdate project file"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("updating translation files"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("Found 5 source text"), m_qbsStdout.constData());

    // Change a source file. The JSON file should not get updated.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("qt2-main.cpp", "// auto s2", "auto s2");
    QCOMPARE(runQbs(QbsRunParameters(QStringList{"-p", "lupdate-runner"})), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QVERIFY2(!m_qbsStdout.contains("creating lupdate project file"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("updating translation files"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("Found 6 source text"), m_qbsStdout.constData());
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
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    const bool isMsvc = m_qbsStdout.contains("is msvc: true");
    const bool isNotMsvc = m_qbsStdout.contains("is msvc: false");
    if (isMsvc) {
        QVERIFY2(m_qbsStderr.contains("not allowed"), m_qbsStderr.constData());
    } else {
        QVERIFY(isNotMsvc);
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

void TestBlackboxQt::mocCompilerDefines()
{
    QDir::setCurrent(testDataDir + "/moc-compiler-defines");
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxQt::mocSameFileName()
{
    QDir::setCurrent(testDataDir + "/moc-same-file-name");
    QCOMPARE(runQbs(), 0);
    QCOMPARE(m_qbsStdout.count("compiling moc_someclass.cpp"), 2);
}

void TestBlackboxQt::noMocRunAfterTouchingOtherCppFile()
{
    QDir::setCurrent(testDataDir + "/no-moc-run-after-touching-other-cpp-file");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("moc header1.h"));
    QVERIFY(m_qbsStdout.contains("moc header2.h"));
    QVERIFY(m_qbsStdout.contains("compiling moc_header1.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling moc_header2.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(m_qbsStdout.contains("linking"));

    WAIT_FOR_NEW_TIMESTAMP();
    touch("main.cpp");
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("moc header1.h"));
    QVERIFY(!m_qbsStdout.contains("moc header2.h"));
    QVERIFY(!m_qbsStdout.contains("compiling moc_header1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling moc_header2.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(m_qbsStdout.contains("linking"));
}

void TestBlackboxQt::noRelinkOnQDebug()
{
    QFETCH(QString, checkMode);
    QFETCH(bool, expectRelink);

    QVERIFY(QDir::setCurrent(testDataDir + "/no-relink-on-qdebug"));
    rmDirR("default");

    // Target check.
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    if (m_qbsStdout.contains("is emscripten: true"))
        QSKIP("Irrelevant for emscripten");
    QVERIFY(m_qbsStdout.contains("is emscripten: false"));
    QVERIFY2(m_qbsStdout.contains("is GCC: "), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("is MinGW: "), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("is Darwin: "), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("is static qt: "), m_qbsStdout.constData());
    const bool isGCCLike = m_qbsStdout.contains("is GCC: true");
    const bool isMingw = m_qbsStdout.contains("is MinGW: true");
    const bool isDarwin = m_qbsStdout.contains("is Darwin: true");
    const bool isStaticQt = m_qbsStdout.contains("is static qt: true");
    if (!isGCCLike)
        expectRelink = false;
    else if (isMingw || isDarwin || isStaticQt)
        expectRelink = true;

    // Initial build.
    QbsRunParameters params("resolve");
    if (isGCCLike && !checkMode.isEmpty())
        params.arguments << ("modules.cpp.exportedSymbolsCheckMode:" + checkMode);
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QCOMPARE(m_qbsStdout.count("linking"), 2);

    // Inserting the qDebug() statement will pull in weak symbols.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("lib.cpp", "// qDebug", "qDebug");
    QCOMPARE(runQbs(), 0);
    QCOMPARE(m_qbsStdout.count("linking"), expectRelink ? 2 : 1);

    // Also check the opposite case.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("lib.cpp", "qDebug", "// qDebug");
    QCOMPARE(runQbs(), 0);
    QCOMPARE(m_qbsStdout.count("linking"), expectRelink ? 2 : 1);
}

void TestBlackboxQt::noRelinkOnQDebug_data()
{
    QTest::addColumn<QString>("checkMode");
    QTest::addColumn<bool>("expectRelink");
    QTest::newRow("default") << QString() << false;
    QTest::newRow("relaxed") << QString("ignore-undefined") << false;
    QTest::newRow("strict") << QString("strict") << true;
}

void TestBlackboxQt::pkgconfig()
{
    QDir::setCurrent(testDataDir + "/pkgconfig");
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QbsRunParameters params;
    params.command = "run";
    QCOMPARE(runQbs(params), 0);
}

void TestBlackboxQt::pkgconfigQt()
{
    QFETCH(QStringList, arguments);
    QFETCH(bool, success);

    QDir::setCurrent(testDataDir + "/pkgconfig-qt");
    rmDirR(relativeBuildDir());

    QbsRunParameters dumpParams("resolve", {"-f", "dump-libpath.qbs"});
    QCOMPARE(runQbs(dumpParams), 0);
    auto lines = QString::fromUtf8(m_qbsStdout).split('\n');
    const QString needle = "libPath=";
    qbs::Internal::removeIf(
            lines, [&needle](const auto &line) { return !line.startsWith(needle); });
    QCOMPARE(lines.size(), 1);
    const auto libPath = lines[0].mid(needle.size());
    auto prefix = QFileInfo(libPath).path();
    if (prefix.endsWith("/lib") && !prefix.startsWith("/lib"))
       prefix = QFileInfo(prefix).path();
    const auto pkgConfigPath = libPath + "/pkgconfig/";
    if (!QFileInfo(pkgConfigPath).exists())
        QSKIP("No *.pc files found");

    rmDirR(relativeBuildDir());
    QbsRunParameters params("build", {"-f", "pkgconfig-qt.qbs"});
    // need to override prefix for the downloaded Qt
    params.environment.insert("PKG_CONFIG_QT5CORE_PREFIX", prefix);
    params.environment.insert("PKG_CONFIG_QT6CORE_PREFIX", prefix);
    params.arguments << "moduleProviders.qbspkgconfig.extraPaths:" + pkgConfigPath;
    params.arguments << arguments;

    QCOMPARE(runQbs(params) == 0, success);

    if (!success)
        QVERIFY(m_qbsStderr.contains("Dependency 'Qt.core' not found for product 'p'"));
}

void TestBlackboxQt::pkgconfigQt_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<bool>("success");
    QTest::newRow("pkgconfig") << QStringList() << true;
    QTest::newRow("dummy")
            << QStringList({"products.p.qbsModuleProviders:dummyProvider"}) << false;
    QTest::newRow("cross-compiling")
            << QStringList({"moduleProviders.qbspkgconfig.sysroot:/some/fake/sysroot"}) << false;
}

void TestBlackboxQt::pkgconfigNoQt()
{
    QDir::setCurrent(testDataDir + "/pkgconfig-qt");
    rmDirR(relativeBuildDir());
    QbsRunParameters params("build", {"-f", "pkgconfig-qt.qbs"});
    params.arguments << "moduleProviders.qbspkgconfig.libDirs:nonexistent";
    params.expectFailure = true;

    QCOMPARE(runQbs(params) == 0, false);
    QVERIFY2(m_qbsStderr.contains("Dependency 'Qt.core' not found for product 'p'"), m_qbsStderr);
    // QBS-1777: basic check for JS exceptions in case of missing Qt
    QVERIFY2(!m_qbsStderr.contains("Error executing provider for module 'Qt.core'"), m_qbsStderr);
}

void TestBlackboxQt::pluginMetaData()
{
    QDir::setCurrent(testDataDir + "/plugin-meta-data");
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    if (m_qbsStdout.contains("using qt4"))
        QSKIP("Qt version too old");
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
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
    bool resolveResult = runQbs(resolveParams) == 0;
    if (m_qbsStdout.contains("using qt4"))
        QSKIP("Qt version too old");
    QCOMPARE(resolveResult, !invalidPlugin);

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

void TestBlackboxQt::qdoc()
{
    QDir::setCurrent(testDataDir + "/qdoc");
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    if (m_qbsStdout.contains("Qt is too old"))
        QSKIP("Skip test since qdoc3 does not work properly");
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFileInfo(relativeProductBuildDir("QDoc Test") + "/qdoctest.qch").exists());
}

void TestBlackboxQt::qmlDebugging()
{
    QDir::setCurrent(testDataDir + "/qml-debugging");
    QCOMPARE(runQbs(), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");

    const bool isGcc = m_qbsStdout.contains("is gcc: true");
    const bool isNotGcc = m_qbsStdout.contains("is gcc: false");
    if (isNotGcc)
        QSKIP("The remainder of this test only applies to gcc");
    QVERIFY(isGcc);

    QProcess nm;
    nm.start("nm", QStringList(relativeExecutableFilePath("debuggable-app", m_qbsStdout)));
    if (!nm.waitForStarted())
        QSKIP("The remainder of this test requires nm");

    QVERIFY2(nm.waitForFinished(), qPrintable(nm.errorString()));
    QVERIFY2(nm.exitCode() == 0, nm.readAllStandardError().constData());
    const QByteArray output = nm.readAllStandardOutput();
    QVERIFY2(output.toLower().contains("debugginghelper"), output.constData());
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
    if ((runQbs(QbsRunParameters("resolve", args)) != 0) &&
            m_qbsStderr.contains("Dependency 'Qt.qml' not found for product 'myapp'"))
        QSKIP("Qt version too old");
    const bool hasRegistrar = m_qbsStdout.contains("has registrar");
    const bool doesNotHaveRegistrar = m_qbsStdout.contains("does not have registrar");
    QVERIFY(hasRegistrar != doesNotHaveRegistrar);
    if (doesNotHaveRegistrar)
        QSKIP("Qt version too old");
    QbsRunParameters buildParams;
    buildParams.arguments << "--command-echo-mode"
                          << "command-line";
    QCOMPARE(runQbs(buildParams), 0);
    const bool enabled = !importName.isEmpty();
    QCOMPARE(m_qbsStdout.contains("--foreign-types"), enabled);
    QCOMPARE(m_qbsStdout.contains("myapp_qmltyperegistrations.cpp.o"), enabled);
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
    if ((runQbs() != 0) &&
            m_qbsStderr.contains("'Qt.quick' has version 4.8.7, but it needs to be at least 5.0.0."))
        QSKIP("Qt version too old");
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
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
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

void TestBlackboxQt::shadertools_data()
{
    QTest::addColumn<QString>("projectName");
    QTest::addColumn<QStringList>("linesToCheck");

    QTest::newRow("basic") << "basic"
                           << QStringList{
                                  "qsb.*basic_shader.frag.qsb.*",
                                  "qsb.*--glsl ['\"]100 es,120,150['\"] --hlsl 50 --msl 20 -O -g",
                              };
    QTest::newRow("defines")
        << "defines"
        << QStringList{"qsb.*defines_shader.frag.qsb.*-DTEST_DEFINE_1=1.*-DTEST_DEFINE_2=1"};
    QTest::newRow("tessellation") << "tessellation"
                                  << QStringList{
                                         "qsb.*tessellation_shader.vert.qsb.*--glsl.*410,320es",
                                         "qsb.*tessellation_shader.tesc.qsb.*--glsl.*410,320es.*"
                                         "--msltess --tess-mode triangles --tess-vertex-count 3"};
    QTest::newRow("viewcount")
        << "viewcount"
        << QStringList{
               "qsb.*basic_shader.frag.qsb.*",
               "qsb.*--glsl ['\"]100 es,120,150['\"] --hlsl 50 --msl 20 --view-count 4",
           };
}

void TestBlackboxQt::shadertools()
{
    QFETCH(QString, projectName);
    QFETCH(QStringList, linesToCheck);

    bool hasViewCount = false;
    const auto shaderToolsFound = findShaderTools(&hasViewCount);
    QVERIFY(shaderToolsFound.has_value());
    if (!shaderToolsFound.value())
        QSKIP("Test requires Qt 6, dynamic Qt build and Qt Shader Tools");

    if (projectName == "viewcount" && !hasViewCount) {
        QSKIP("Test requires Qt 6.7.0 or later");
    }

    QDir::setCurrent(testDataDir + "/shadertools");
    rmDirR(relativeBuildDir());

    rmDirR(relativeBuildDir());
    QbsRunParameters params;
    params.arguments << QStringLiteral("-f") << projectName + ".qbs"
                     << "--command-echo-mode"
                     << "command-line";
    QCOMPARE(runQbs(params), 0);
    for (const auto &line : linesToCheck) {
        const QRegularExpression pattern(line);
        const QRegularExpressionMatch match = pattern.match(m_qbsStdout);
        QVERIFY2(match.hasMatch(), qPrintable(m_qbsStdout));
    }
}

void TestBlackboxQt::shadertoolsLinking_data()
{
    QTest::addColumn<bool>("enableLinking");

    QTest::newRow("linking enabled") << true;
    QTest::newRow("linking disabled") << false;
}

void TestBlackboxQt::shadertoolsLinking()
{
    QFETCH(bool, enableLinking);

    const auto shaderToolsFound = findShaderTools();
    QVERIFY(shaderToolsFound.has_value());
    if (!shaderToolsFound.value())
        QSKIP("Test requires Qt 6, dynamic Qt build and Qt Shader Tools");

    QDir::setCurrent(testDataDir + "/shadertools-linking");

    QbsRunParameters params;
    params.arguments << QString("modules.Qt.shadertools.enableLinking:%1").arg(enableLinking);
    params.expectFailure = !enableLinking;

    if (enableLinking) {
        QCOMPARE(runQbs(params), 0);
        QVERIFY(!m_qbsStdout.contains("compiling shader.frag.qsb"));
    } else {
        QVERIFY(runQbs(params) != 0);
    }
}

void TestBlackboxQt::staticQtPluginLinking()
{
    QDir::setCurrent(testDataDir + "/static-qt-plugin-linking");
    QCOMPARE(runQbs(QStringList("products.p.type:application")), 0);
    const bool isStaticQt = m_qbsStdout.contains("Qt is static");
    QVERIFY2(m_qbsStdout.contains("creating static import") == isStaticQt, m_qbsStdout.constData());
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("products.p.type:staticlibrary"))), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("creating static import"), m_qbsStdout.constData());
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

    const QString objectSuffix = parsedObjectSuffix(m_qbsStdout);
    const QString productFilePath = relativeExecutableFilePath("i", m_qbsStdout);
    QVERIFY2(regularFileExists(productFilePath), qPrintable(productFilePath));
    QString moc_bla_objectFileName = relativeProductBuildDir("i") + '/' + inputDirHash("qt.headers")
                                     + "/moc_bla.cpp" + objectSuffix;
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
    if (m_qbsStdout.contains("using qt4"))
        QSKIP("Qt version too old");
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    const QString fileName = relativeExecutableFilePath("i", m_qbsStdout);
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QVERIFY2(m_qbsStdout.contains("rcc"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("compiling test.cpp"), m_qbsStdout.constData());
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
