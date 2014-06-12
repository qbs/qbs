/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "tst_blackbox.h"

#include "../skip.h"

#include <app/shared/qbssettings.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/installoptions.h>
#include <tools/profile.h>

#include <QLocale>
#include <QRegExp>
#include <QTemporaryFile>
#include <QScriptEngine>
#include <QScriptValue>

using qbs::InstallOptions;
using qbs::Internal::HostOsInfo;
using qbs::Internal::removeDirectoryWithContents;
using qbs::Profile;

static QString initQbsExecutableFilePath()
{
    QString filePath = QCoreApplication::applicationDirPath() + QLatin1String("/qbs");
    filePath = HostOsInfo::appendExecutableSuffix(QDir::cleanPath(filePath));
    return filePath;
}

TestBlackbox::TestBlackbox()
    : testDataDir(QCoreApplication::applicationDirPath() + "/../tests/auto/blackbox/testWorkDir"),
      testSourceDir(QDir::cleanPath(SRCDIR "/testdata")),
      qbsExecutableFilePath(initQbsExecutableFilePath()),
      buildProfileName(QLatin1String("qbs_autotests")),
      buildDir(buildProfileName + QLatin1String("-debug")),
      defaultInstallRoot(buildDir + QLatin1Char('/') + InstallOptions::defaultInstallRoot()),
      buildGraphPath(buildDir + QLatin1Char('/') + buildDir + QLatin1String(".bg"))
{
    QLocale::setDefault(QLocale::c());
}

int TestBlackbox::runQbs(const QbsRunParameters &params)
{
    QStringList args;
    if (!params.command.isEmpty())
        args << params.command;
    if ((QStringList() << QLatin1String("") << QLatin1String("build") << QLatin1String("clean")
         << QLatin1String("install") << QLatin1String("resolve") << QLatin1String("run")
         << QLatin1String("shell") << QLatin1String("status") << QLatin1String("update-timestamps"))
            .contains(params.command)) {
        args.append(QStringList(QLatin1String("-d")) << QLatin1String("."));
    }
    args << params.arguments;
    if (params.useProfile)
        args.append(QLatin1String("profile:") + buildProfileName);
    QString cmdLine = qbsExecutableFilePath;
    foreach (const QString &str, args)
        cmdLine += QLatin1String(" \"") + str + QLatin1Char('"');
    QProcess process;
    process.setProcessEnvironment(params.environment);
    process.start(cmdLine);
    const int waitTime = 5 * 60000;
    if (!process.waitForStarted() || !process.waitForFinished(waitTime)) {
        m_qbsStderr = process.readAllStandardError();
        if (!params.expectFailure)
            qDebug("%s", qPrintable(process.errorString()));
        return -1;
    }

    m_qbsStderr = process.readAllStandardError();
    m_qbsStdout = process.readAllStandardOutput();
    sanitizeOutput(&m_qbsStderr);
    sanitizeOutput(&m_qbsStdout);
    if ((process.exitStatus() != QProcess::NormalExit
             || process.exitCode() != 0) && !params.expectFailure) {
        if (!m_qbsStderr.isEmpty())
            qDebug("%s", m_qbsStderr.constData());
        if (!m_qbsStdout.isEmpty())
            qDebug("%s", m_qbsStdout.constData());
    }
    return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
}

/*!
  Recursive copy from directory to another.
  Note that this updates the file stamps on Linux but not on Windows.
  */
static void ccp(const QString &sourceDirPath, const QString &targetDirPath)
{
    QDir currentDir;
    QDirIterator dit(sourceDirPath, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    while (dit.hasNext()) {
        dit.next();
        const QString targetPath = targetDirPath + QLatin1Char('/') + dit.fileName();
        currentDir.mkpath(targetPath);
        ccp(dit.filePath(), targetPath);
    }

    QDirIterator fit(sourceDirPath, QDir::Files | QDir::Hidden);
    while (fit.hasNext()) {
        fit.next();
        const QString targetPath = targetDirPath + QLatin1Char('/') + fit.fileName();
        QFile::remove(targetPath);  // allowed to fail
        QVERIFY(QFile::copy(fit.filePath(), targetPath));
    }
}

void TestBlackbox::rmDirR(const QString &dir)
{
    QString errorMessage;
    removeDirectoryWithContents(dir, &errorMessage);
}

void TestBlackbox::touch(const QString &fn)
{
    QFile f(fn);
    int s = f.size();
    if (!f.open(QFile::ReadWrite))
        qFatal("cannot open file %s", qPrintable(fn));
    f.resize(s+1);
    f.resize(s);
}

// Waits for the time that corresponds to the host file system's time stamp granularity.
void TestBlackbox::waitForNewTimestamp()
{
    if (qbs::Internal::HostOsInfo::isWindowsHost())
        QTest::qWait(1);        // NTFS has 100 ns precision. Let's ignore exFAT.
    else
        QTest::qWait(1000);
}

QByteArray TestBlackbox::unifiedLineEndings(const QByteArray &ba)
{
    if (HostOsInfo::isWindowsHost()) {
        QByteArray result;
        result.reserve(ba.size());
        for (int i = 0; i < ba.size(); ++i) {
            char c = ba.at(i);
            if (c != '\r')
                result.append(c);
        }
        return result;
    } else {
        return ba;
    }
}

void TestBlackbox::sanitizeOutput(QByteArray *ba)
{
    if (HostOsInfo::isWindowsHost())
        ba->replace('\r', "");
}

void TestBlackbox::initTestCase()
{
    QVERIFY(QFile::exists(qbsExecutableFilePath));

    SettingsPtr settings = qbsSettings(QString());
    if (!settings->profiles().contains(buildProfileName))
        QFAIL(QByteArray("The build profile '" + buildProfileName.toLocal8Bit() +
                         "' could not be found. Please set it up on your machine."));

    Profile buildProfile(buildProfileName, settings.data());
    QVariant qtBinPath = buildProfile.value(QLatin1String("Qt.core.binPath"));
    if (!qtBinPath.isValid())
        QFAIL(QByteArray("The build profile '" + buildProfileName.toLocal8Bit() +
                         "' is not a valid Qt profile."));
    if (!QFile::exists(qtBinPath.toString()))
        QFAIL(QByteArray("The build profile '" + buildProfileName.toLocal8Bit() +
                         "' points to an invalid Qt path."));

    // Initialize the test data directory.
    QVERIFY(testDataDir != testSourceDir);
    rmDirR(testDataDir);
    QDir().mkpath(testDataDir);
    ccp(testSourceDir, testDataDir);
}

void TestBlackbox::addedFilePersistent()
{
    QDir::setCurrent(testDataDir + QLatin1String("/added-file-persistent"));

    // On the initial run, linking will fail.
    QbsRunParameters failedRunParams;
    failedRunParams.expectFailure = true;
    QVERIFY(runQbs(failedRunParams) != 0);

    // Add a file. qbs must schedule it for rule application on the next build.
    waitForNewTimestamp();
    QFile projectFile("project.qbs");
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    const QByteArray originalContent = projectFile.readAll();
    QByteArray addedFileContent = originalContent;
    addedFileContent.replace("/* 'file.cpp' */", "'file.cpp'");
    projectFile.resize(0);
    projectFile.write(addedFileContent);
    projectFile.flush();
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);

    // Remove the file again. qbs must unschedule the rule application again.
    // Consequently, the linking step must fail as in the initial run.
    waitForNewTimestamp();
    projectFile.resize(0);
    projectFile.write(originalContent);
    projectFile.flush();
    QVERIFY(runQbs(failedRunParams) != 0);

    // Add the file again. qbs must schedule it for rule application on the next build.
    waitForNewTimestamp();
    projectFile.resize(0);
    projectFile.write(addedFileContent);
    projectFile.close();
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);

    // qbs must remember that a file was scheduled for rule application. The build must then
    // succeed, as now all necessary symbols are linked in.
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::addQObjectMacroToCppFile()
{
    QDir::setCurrent(testDataDir + QLatin1String("/add-qobject-macro-to-cpp-file"));
    QCOMPARE(runQbs(), 0);

    waitForNewTimestamp();
    QFile cppFile("object.cpp");
    QVERIFY2(cppFile.open(QIODevice::ReadWrite), qPrintable(cppFile.errorString()));
    QByteArray contents = cppFile.readAll();
    contents.replace("// ", "");
    cppFile.resize(0);
    cppFile.write(contents);
    cppFile.close();
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::baseProperties()
{
    QDir::setCurrent(testDataDir + QLatin1String("/baseProperties"));
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::build_project_data()
{
    QTest::addColumn<QString>("projectSubDir");
    QTest::addColumn<QString>("productFileName");
    QTest::newRow("BPs in Sources")
            << QString("buildproperties_source")
            << QString(HostOsInfo::appendExecutableSuffix(buildDir + "/HelloWorld"));
    QTest::newRow("code generator")
            << QString("codegen")
            << QString(HostOsInfo::appendExecutableSuffix(buildDir + "/codegen"));
    QTest::newRow("link static libs")
            << QString("link_staticlib")
            << QString(buildDir + QLatin1String("/")
                       + HostOsInfo::appendExecutableSuffix("HelloWorld"));
    QTest::newRow("precompiled header")
            << QString("precompiledHeader")
            << QString(buildDir + QLatin1String("/")
                       + HostOsInfo::appendExecutableSuffix("MyApp"));
    QTest::newRow("lots of dots")
            << QString("lotsofdots")
            << QString(buildDir + QLatin1String("/")
                       + HostOsInfo::appendExecutableSuffix("lots.of.dots"));
    QTest::newRow("Qt5 plugin")
            << QString("qt5plugin")
            << QString(buildDir + QLatin1String("/") + HostOsInfo::dynamicLibraryName("echoplugin"));
    QTest::newRow("Q_OBJECT in source")
            << QString("moc_cpp")
            << QString(HostOsInfo::appendExecutableSuffix(buildDir + "/moc_cpp"));
    QTest::newRow("Q_OBJECT in header")
            << QString("moc_hpp")
            << QString(HostOsInfo::appendExecutableSuffix(buildDir + "/moc_hpp"));
    QTest::newRow("Q_OBJECT in header, moc_XXX.cpp included")
            << QString("moc_hpp_included")
            << QString(HostOsInfo::appendExecutableSuffix(buildDir + "/moc_hpp_included"));
    QTest::newRow("app and lib with same source file")
            << QString("lib_samesource")
            << QString(HostOsInfo::appendExecutableSuffix(buildDir + "/HelloWorldApp"));
    QTest::newRow("source files with the same base name but different extensions")
            << QString("sameBaseName")
            << QString(HostOsInfo::appendExecutableSuffix(buildDir + "/basename"));
    QTest::newRow("static library dependencies")
            << QString("staticLibDeps")
            << QString(HostOsInfo::appendExecutableSuffix(buildDir + "/staticLibDeps"));
}

void TestBlackbox::build_project()
{
    QFETCH(QString, projectSubDir);
    QFETCH(QString, productFileName);
    if (!projectSubDir.startsWith('/'))
        projectSubDir.prepend('/');
    QVERIFY2(QFile::exists(testDataDir + projectSubDir), qPrintable(testDataDir + projectSubDir));
    QDir::setCurrent(testDataDir + projectSubDir);
    rmDirR(buildDir);

    QCOMPARE(runQbs(), 0);
    QVERIFY2(QFile::exists(productFileName), qPrintable(productFileName));
    QVERIFY(QFile::exists(buildGraphPath));
    QVERIFY2(QFile::remove(productFileName), qPrintable(productFileName));
    waitForNewTimestamp();
    QCOMPARE(runQbs(QbsRunParameters(QStringList("--check-timestamps"))), 0);
    QVERIFY2(QFile::exists(productFileName), qPrintable(productFileName));
    QVERIFY(QFile::exists(buildGraphPath));
}

void TestBlackbox::build_project_dry_run_data()
{
    build_project_data();
}

void TestBlackbox::build_project_dry_run()
{
    QFETCH(QString, projectSubDir);
    QFETCH(QString, productFileName);
    if (!projectSubDir.startsWith('/'))
        projectSubDir.prepend('/');
    QVERIFY2(QFile::exists(testDataDir + projectSubDir), qPrintable(testDataDir + projectSubDir));
    QDir::setCurrent(testDataDir + projectSubDir);
    rmDirR(buildDir);

    QCOMPARE(runQbs(QbsRunParameters(QStringList("-n"))), 0);
    const QStringList &buildDirContents
            = QDir(buildDir).entryList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
    QVERIFY2(buildDirContents.isEmpty(), qPrintable(buildDirContents.join(" ")));
}

void TestBlackbox::changeDependentLib()
{
    QDir::setCurrent(testDataDir + "/change-dependent-lib");
    QCOMPARE(runQbs(), 0);
    waitForNewTimestamp();
    const QString qbsFileName("change-dependent-lib.qbs");
    QFile qbsFile(qbsFileName);
    QVERIFY(qbsFile.open(QIODevice::ReadWrite));
    const QByteArray content1 = qbsFile.readAll();
    QByteArray content2 = content1;
    content2.replace("cpp.defines: [\"XXXX\"]", "cpp.defines: [\"ABCD\"]");
    QVERIFY(content1 != content2);
    qbsFile.seek(0);
    qbsFile.write(content2);
    qbsFile.close();
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::changedFiles()
{
    QDir::setCurrent(testDataDir + "/changed-files");
    const QString changedFile = QDir::cleanPath(QDir::currentPath() + "/file1.cpp");
    QbsRunParameters params(QStringList("--changed-files") << changedFile);

    // Initial run: Build all files, even though only one of them was marked as changed.
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStdout.count("compiling"), 3);

    waitForNewTimestamp();
    touch(QDir::currentPath() + "/main.cpp");

    // Now only the file marked as changed must be compiled, even though it hasn't really
    // changed and another one has.
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStdout.count("compiling"), 1);
    QVERIFY2(m_qbsStdout.contains("file1.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::dependenciesProperty()
{
    QDir::setCurrent(testDataDir + QLatin1String("/dependenciesProperty"));
    QCOMPARE(runQbs(), 0);
    QFile depsFile(buildDir + QLatin1String("/product1.deps"));
    QVERIFY(depsFile.open(QFile::ReadOnly));
    QString deps = QString::fromLatin1(depsFile.readAll());
    QVERIFY(!deps.isEmpty());
    QScriptEngine scriptEngine;
    QScriptValue scriptValue = scriptEngine.evaluate(deps);
    QScriptValue product2;
    QScriptValue qbs;
    int c = scriptValue.property(QLatin1String("length")).toInt32();
    QCOMPARE(c, 2);
    for (int i = 0; i < c; ++i) {
        QScriptValue dep = scriptValue.property(i);
        QString name = dep.property(QLatin1String("name")).toVariant().toString();
        if (name == QLatin1String("product2"))
            product2 = dep;
        else if (name == QLatin1String("qbs"))
            qbs = dep;
    }
    QVERIFY(qbs.isObject());
    QVERIFY(product2.isObject());
    QCOMPARE(product2.property(QLatin1String("type")).toString(), QLatin1String("application"));
    QCOMPARE(product2.property(QLatin1String("narf")).toString(), QLatin1String("zort"));
    QScriptValue product2_deps = product2.property(QLatin1String("dependencies"));
    QVERIFY(product2_deps.isObject());
    c = product2_deps.property(QLatin1String("length")).toInt32();
    QCOMPARE(c, 2);
    QScriptValue product2_qbs;
    QScriptValue product2_cpp;
    for (int i = 0; i < c; ++i) {
        QScriptValue dep = product2_deps.property(i);
        QString name = dep.property(QLatin1String("name")).toVariant().toString();
        if (name == QLatin1String("cpp"))
            product2_cpp = dep;
        else if (name == QLatin1String("qbs"))
            product2_qbs = dep;
    }
    QVERIFY(product2_qbs.isObject());
    QVERIFY(product2_cpp.isObject());
    QCOMPARE(product2_cpp.property("defines").toString(), QLatin1String("SMURF"));
}

void TestBlackbox::resolve_project_data()
{
    return build_project_data();
}

void TestBlackbox::resolve_project()
{
    QFETCH(QString, projectSubDir);
    QFETCH(QString, productFileName);
    if (!projectSubDir.startsWith('/'))
        projectSubDir.prepend('/');
    QVERIFY2(QFile::exists(testDataDir + projectSubDir), qPrintable(testDataDir + projectSubDir));
    QDir::setCurrent(testDataDir + projectSubDir);
    rmDirR(buildDir);

    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    QVERIFY2(!QFile::exists(productFileName), qPrintable(productFileName));
    QVERIFY(QFile::exists(buildGraphPath));
}

void TestBlackbox::resolve_project_dry_run_data()
{
    return resolve_project_data();
}

void TestBlackbox::resolve_project_dry_run()
{
    QFETCH(QString, projectSubDir);
    QFETCH(QString, productFileName);
    if (!projectSubDir.startsWith('/'))
        projectSubDir.prepend('/');
    QVERIFY2(QFile::exists(testDataDir + projectSubDir), qPrintable(testDataDir + projectSubDir));
    QDir::setCurrent(testDataDir + projectSubDir);
    rmDirR(buildDir);

    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("resolve"), QStringList("-n"))), 0);
    QVERIFY2(!QFile::exists(productFileName), qPrintable(productFileName));
    QVERIFY2(!QFile::exists(buildGraphPath), qPrintable(buildGraphPath));
}

static bool symlinkExists(const QString &linkFilePath)
{
    return QFileInfo(linkFilePath).isSymLink();
}

void TestBlackbox::clean()
{
    const QString appObjectFilePath = buildDir + "/.obj/app/main.cpp" + QTC_HOST_OBJECT_SUFFIX;
    const QString appExeFilePath = buildDir + "/app" + QTC_HOST_EXE_SUFFIX;
    const QString depObjectFilePath = buildDir + "/.obj/dep/dep.cpp" + QTC_HOST_OBJECT_SUFFIX;
    const QString depLibBase = buildDir + '/' + QTC_HOST_DYNAMICLIB_PREFIX + "dep";
    QString depLibFilePath;
    QStringList symlinks;
    if (qbs::Internal::HostOsInfo::isOsxHost()) {
        depLibFilePath = depLibBase + ".1.1.0" + QTC_HOST_DYNAMICLIB_SUFFIX;
        symlinks << depLibBase + ".1.1" + QTC_HOST_DYNAMICLIB_SUFFIX
                 << depLibBase + ".1"  + QTC_HOST_DYNAMICLIB_SUFFIX
                 << depLibBase + QTC_HOST_DYNAMICLIB_SUFFIX;
    } else if (qbs::Internal::HostOsInfo::isAnyUnixHost()) {
        depLibFilePath = depLibBase + QTC_HOST_DYNAMICLIB_SUFFIX + ".1.1.0";
        symlinks << depLibBase + QTC_HOST_DYNAMICLIB_SUFFIX + ".1.1"
                 << depLibBase + QTC_HOST_DYNAMICLIB_SUFFIX + ".1"
                 << depLibBase + QTC_HOST_DYNAMICLIB_SUFFIX;
    } else {
        depLibFilePath = depLibBase + QTC_HOST_DYNAMICLIB_SUFFIX;
    }

    QDir::setCurrent(testDataDir + "/clean");

    // Default behavior: Remove only temporaries.
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFile(appObjectFilePath).exists());
    QVERIFY(QFile(appExeFilePath).exists());
    QVERIFY(QFile(depObjectFilePath).exists());
    QVERIFY(QFile(depLibFilePath).exists());
    foreach (const QString &symLink, symlinks)
        QVERIFY2(QFile(symLink).exists(), qPrintable(symLink));
    QCOMPARE(runQbs(QbsRunParameters("clean")), 0);
    QVERIFY(!QFile(appObjectFilePath).exists());
    QVERIFY(QFile(appExeFilePath).exists());
    QVERIFY(!QFile(depObjectFilePath).exists());
    QVERIFY(QFile(depLibFilePath).exists());
    foreach (const QString &symLink, symlinks)
        QVERIFY2(symlinkExists(symLink), qPrintable(symLink));

    // Remove all.
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFile(appObjectFilePath).exists());
    QVERIFY(QFile(appExeFilePath).exists());
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("clean"), QStringList("--all-artifacts"))), 0);
    QVERIFY(!QFile(appObjectFilePath).exists());
    QVERIFY(!QFile(appExeFilePath).exists());
    QVERIFY(!QFile(depObjectFilePath).exists());
    QVERIFY(!QFile(depLibFilePath).exists());
    foreach (const QString &symLink, symlinks)
        QVERIFY2(!symlinkExists(symLink), qPrintable(symLink));

    // Dry run.
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFile(appObjectFilePath).exists());
    QVERIFY(QFile(appExeFilePath).exists());
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("clean"),
                                     QStringList("--all-artifacts") << "-n")), 0);
    QVERIFY(QFile(appObjectFilePath).exists());
    QVERIFY(QFile(appExeFilePath).exists());
    QVERIFY(QFile(depObjectFilePath).exists());
    QVERIFY(QFile(depLibFilePath).exists());
    foreach (const QString &symLink, symlinks)
        QVERIFY2(symlinkExists(symLink), qPrintable(symLink));

    // Product-wise, dependency only.
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFile(appObjectFilePath).exists());
    QVERIFY(QFile(appExeFilePath).exists());
    QVERIFY(QFile(depObjectFilePath).exists());
    QVERIFY(QFile(depLibFilePath).exists());
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("clean"),
                                     QStringList("--all-artifacts") << "-p" << "dep")),
             0);
    QVERIFY(QFile(appObjectFilePath).exists());
    QVERIFY(QFile(appExeFilePath).exists());
    QVERIFY(!QFile(depObjectFilePath).exists());
    QVERIFY(!QFile(depLibFilePath).exists());
    foreach (const QString &symLink, symlinks)
        QVERIFY2(!symlinkExists(symLink), qPrintable(symLink));

    // Product-wise, dependent product only.
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFile(appObjectFilePath).exists());
    QVERIFY(QFile(appExeFilePath).exists());
    QVERIFY(QFile(depObjectFilePath).exists());
    QVERIFY(QFile(depLibFilePath).exists());
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("clean"),
                                     QStringList("--all-artifacts") << "-p" << "app")),
             0);
    QVERIFY(!QFile(appObjectFilePath).exists());
    QVERIFY(!QFile(appExeFilePath).exists());
    QVERIFY(QFile(depObjectFilePath).exists());
    QVERIFY(QFile(depLibFilePath).exists());
    foreach (const QString &symLink, symlinks)
        QVERIFY2(symlinkExists(symLink), qPrintable(symLink));
}

void TestBlackbox::exportSimple()
{
    QDir::setCurrent(testDataDir + "/exportSimple");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::exportWithRecursiveDepends()
{
    QDir::setCurrent(testDataDir + "/exportWithRecursiveDepends");
    QEXPECT_FAIL("", "currently broken", Abort);
    QbsRunParameters params;
    params.expectFailure = true; // Remove when test no longer fails.
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::fileTagger()
{
    QDir::setCurrent(testDataDir + "/fileTagger");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("moc bla.cpp"));
}

void TestBlackbox::rc()
{
    QDir::setCurrent(testDataDir + "/rc");
    QCOMPARE(runQbs(), 0);
    const bool rcFileWasCompiled = m_qbsStdout.contains("compiling test.rc");
    QCOMPARE(rcFileWasCompiled, HostOsInfo::isWindowsHost());
}

void TestBlackbox::renameProduct()
{
    QDir::setCurrent(testDataDir + "/renameProduct");

    // Initial run.
    QCOMPARE(runQbs(), 0);

    // Rename lib and adapt Depends item.
    waitForNewTimestamp();
    QFile f("rename.qbs");
    QVERIFY(f.open(QIODevice::ReadWrite));
    QByteArray contents = f.readAll();
    contents.replace("TheLib", "thelib");
    f.resize(0);
    f.write(contents);
    f.close();
    QCOMPARE(runQbs(), 0);

    // Rename lib and don't adapt Depends item.
    waitForNewTimestamp();
    QVERIFY(f.open(QIODevice::ReadWrite));
    contents = f.readAll();
    const int libNameIndex = contents.lastIndexOf("thelib");
    QVERIFY(libNameIndex != -1);
    contents.replace(libNameIndex, 6, "TheLib");
    f.resize(0);
    f.write(contents);
    f.close();
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
}

void TestBlackbox::renameTargetArtifact()
{
    QDir::setCurrent(testDataDir + "/renameTargetArtifact");

    // Initial run.
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling"));
    QCOMPARE(m_qbsStdout.count("linking"), 2);

    // Rename library file name.
    waitForNewTimestamp();
    QFile f("rename.qbs");
    QVERIFY(f.open(QIODevice::ReadWrite));
    QByteArray contents = f.readAll();
    contents.replace("the_lib", "TheLib");
    f.resize(0);
    f.write(contents);
    f.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("compiling"));
    QCOMPARE(m_qbsStdout.count("linking"), 2);
}

void TestBlackbox::softDependency()
{
    QDir::setCurrent(testDataDir + "/soft-dependency");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::subProjects()
{
    QDir::setCurrent(testDataDir + "/subprojects");

    // Check all three types of subproject creation, plus property overrides.
    QCOMPARE(runQbs(), 0);

    // Disabling both the project with the dependency and the one with the dependent
    // should not cause an error.
    waitForNewTimestamp();
    QFile f(testDataDir + "/subprojects/toplevelproject.qbs");
    QVERIFY(f.open(QIODevice::ReadWrite));
    QByteArray contents = f.readAll();
    contents.replace("condition: true", "condition: false");
    f.resize(0);
    f.write(contents);
    f.close();
    f.setFileName(testDataDir + "/subprojects/subproject2/subproject2.qbs");
    QVERIFY(f.open(QIODevice::ReadWrite));
    contents = f.readAll();
    contents.replace("condition: true", "condition: false");
    f.resize(0);
    f.write(contents);
    f.close();
    QCOMPARE(runQbs(), 0);

    // Disabling the project with the dependency only is an error.
    // This tests also whether changes in sub-projects are detected.
    waitForNewTimestamp();
    f.setFileName(testDataDir + "/subprojects/toplevelproject.qbs");
    QVERIFY(f.open(QIODevice::ReadWrite));
    contents = f.readAll();
    contents.replace("condition: false", "condition: true");
    f.resize(0);
    f.write(contents);
    f.close();
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
}

void TestBlackbox::track_qrc()
{
    QDir::setCurrent(testDataDir + "/qrc");
    QCOMPARE(runQbs(), 0);
    const QString fileName = HostOsInfo::appendExecutableSuffix(buildDir + "/i");
    QVERIFY2(QFile(fileName).exists(), qPrintable(fileName));
    QDateTime dt = QFileInfo(fileName).lastModified();
    QTest::qSleep(2020);
    {
        QFile f("stuff.txt");
        f.remove();
        QVERIFY(f.open(QFile::WriteOnly));
        f.write("bla");
        f.close();
    }
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFile(fileName).exists());
    QVERIFY(dt < QFileInfo(fileName).lastModified());
}

void TestBlackbox::track_qobject_change()
{
    QDir::setCurrent(testDataDir + "/trackQObjChange");
    QFile("bla.h").remove();
    QVERIFY(QFile("bla_qobject.h").copy("bla.h"));
    touch("bla.h");
    QCOMPARE(runQbs(), 0);
    const QString productFilePath = HostOsInfo::appendExecutableSuffix(buildDir + "/i");
    QVERIFY2(QFile(productFilePath).exists(), qPrintable(productFilePath));
    QString moc_bla_objectFileName
            = buildDir + "/.obj/i/GeneratedFiles/i/moc_bla.cpp" QTC_HOST_OBJECT_SUFFIX;
    QVERIFY(QFile(moc_bla_objectFileName).exists());

    QTest::qSleep(1000);
    QFile("bla.h").remove();
    QVERIFY(QFile("bla_noqobject.h").copy("bla.h"));
    touch("bla.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFile(productFilePath).exists());
    QVERIFY(!QFile(moc_bla_objectFileName).exists());
}

void TestBlackbox::trackAddFile()
{
    QProcess process;
    QList<QByteArray> output;
    QDir::setCurrent(testDataDir + "/trackAddFile");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDataDir + "/trackAddFile/work");
    QCOMPARE(runQbs(), 0);

    process.start(buildDir + "/someapp");
    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(), qPrintable(process.errorString()));
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");
    QString unchangedObjectFile = buildDir + "/someapp/narf.cpp" QTC_HOST_OBJECT_SUFFIX;
    QDateTime unchangedObjectFileTime1 = QFileInfo(unchangedObjectFile).lastModified();

    waitForNewTimestamp();
    ccp("../after", ".");
    touch("project.qbs");
    touch("main.cpp");
    QCOMPARE(runQbs(), 0);

    process.start(buildDir + "/someapp");
    QVERIFY(process.waitForStarted());
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "ZORT!");

    // the object file of the untouched source should not have changed
    QDateTime unchangedObjectFileTime2 = QFileInfo(unchangedObjectFile).lastModified();
    QCOMPARE(unchangedObjectFileTime1, unchangedObjectFileTime2);
}

void TestBlackbox::trackExternalProductChanges()
{
    QDir::setCurrent(testDataDir + "/trackExternalProductChanges");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));

    QbsRunParameters params;
    params.environment.insert("QBS_TEST_PULL_IN_FILE_VIA_ENV", "1");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));

    rmDirR(buildDir);
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));

    waitForNewTimestamp();
    QFile jsFile("fileList.js");
    QVERIFY(jsFile.open(QIODevice::ReadWrite));
    QByteArray jsCode = jsFile.readAll();
    jsCode.replace("return []", "return ['jsFileChange.cpp']");
    jsFile.resize(0);
    jsFile.write(jsCode);
    jsFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));

    rmDirR(buildDir);
    QVERIFY(jsFile.open(QIODevice::ReadWrite));
    jsCode = jsFile.readAll();
    jsCode.replace("['jsFileChange.cpp']", "[]");
    jsFile.resize(0);
    jsFile.write(jsCode);
    jsFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));

    QFile cppFile("fileExists.cpp");
    QVERIFY(cppFile.open(QIODevice::WriteOnly));
    cppFile.write("void fileExists() { }\n");
    cppFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling fileExists.cpp"));
}

void TestBlackbox::trackRemoveFile()
{
    QProcess process;
    QList<QByteArray> output;
    QDir::setCurrent(testDataDir + "/trackAddFile");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    ccp("after", "work");
    QDir::setCurrent(testDataDir + "/trackAddFile/work");
    QCOMPARE(runQbs(), 0);

    process.start(buildDir + "/someapp");
    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(), qPrintable(process.errorString()));
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "ZORT!");
    QString unchangedObjectFile = buildDir + "/someapp/narf.cpp" QTC_HOST_OBJECT_SUFFIX;
    QDateTime unchangedObjectFileTime1 = QFileInfo(unchangedObjectFile).lastModified();

    waitForNewTimestamp();
    QFile::remove("project.qbs");
    QFile::remove("main.cpp");
    QFile::copy("../before/project.qbs", "project.qbs");
    QFile::copy("../before/main.cpp", "main.cpp");
    QVERIFY(QFile::remove("zort.h"));
    QVERIFY(QFile::remove("zort.cpp"));
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("resolve"))), 0);

    touch("main.cpp");
    touch("project.qbs");
    QCOMPARE(runQbs(), 0);

    process.start(buildDir + "/someapp");
    QVERIFY(process.waitForStarted());
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");

    // the object file of the untouched source should not have changed
    QDateTime unchangedObjectFileTime2 = QFileInfo(unchangedObjectFile).lastModified();
    QCOMPARE(unchangedObjectFileTime1, unchangedObjectFileTime2);

    // the object file for the removed cpp file should have vanished too
    QCOMPARE(QFile::exists(buildDir + "/someapp/zort.cpp" QTC_HOST_OBJECT_SUFFIX), false);
}

void TestBlackbox::trackAddFileTag()
{
    QProcess process;
    QList<QByteArray> output;
    QDir::setCurrent(testDataDir + "/trackFileTags");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDataDir + "/trackFileTags/work");
    QCOMPARE(runQbs(), 0);

    process.start(buildDir + "/someapp");
    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(), qPrintable(process.errorString()));
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's no foo here");

    waitForNewTimestamp();
    ccp("../after", ".");
    touch("main.cpp");
    touch("project.qbs");
    QCOMPARE(runQbs(), 0);

    process.start(buildDir + "/someapp");
    QVERIFY(process.waitForStarted());
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's 15 foo here");
}

void TestBlackbox::trackRemoveFileTag()
{
    QProcess process;
    QList<QByteArray> output;
    QDir::setCurrent(testDataDir + "/trackFileTags");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("after", "work");
    QDir::setCurrent(testDataDir + "/trackFileTags/work");
    QCOMPARE(runQbs(), 0);

    // check if the artifacts are here that will become stale in the 2nd step
    QVERIFY2(QFile::exists(buildDir + "/.obj/someapp/main_foo.cpp" QTC_HOST_OBJECT_SUFFIX),
            qPrintable(buildDir + "/.obj/someapp/main_foo.cpp" QTC_HOST_OBJECT_SUFFIX));
    QVERIFY2(QFile::exists(buildDir + "/main_foo.cpp"), qPrintable(buildDir + "/main_foo.cpp"));
    QVERIFY2(QFile::exists(buildDir + "/main.foo"), qPrintable(buildDir + "/main.foo"));

    process.start(buildDir + "/someapp");
    QVERIFY(process.waitForStarted());
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's 15 foo here");

    waitForNewTimestamp();
    ccp("../before", ".");
    touch("main.cpp");
    touch("project.qbs");
    QCOMPARE(runQbs(), 0);

    process.start(buildDir + "/someapp");
    QVERIFY(process.waitForStarted());
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's no foo here");

    // check if stale artifacts have been removed
    QCOMPARE(QFile::exists(buildDir + "/someapp/main_foo.cpp" QTC_HOST_OBJECT_SUFFIX), false);
    QCOMPARE(QFile::exists(buildDir + "/someapp/main_foo.cpp"), false);
    QCOMPARE(QFile::exists(buildDir + "/someapp/main.foo"), false);
}

void TestBlackbox::trackAddMocInclude()
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

    waitForNewTimestamp();
    ccp("../after", ".");
    touch("main.cpp");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::trackAddProduct()
{
    QDir::setCurrent(testDataDir + "/trackProducts");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDataDir + "/trackProducts/work");
    QbsRunParameters params(QStringList() << "-f" << "trackProducts.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling foo.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling bar.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product1"));
    QVERIFY(m_qbsStdout.contains("linking product2"));

    waitForNewTimestamp();
    ccp("../after", ".");
    touch("trackProducts.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling zoo.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product3"));
    QVERIFY(!m_qbsStdout.contains("compiling foo.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling bar.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product1"));
    QVERIFY(!m_qbsStdout.contains("linking product2"));
}

void TestBlackbox::trackRemoveProduct()
{
    QDir::setCurrent(testDataDir + "/trackProducts");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    ccp("after", "work");
    QDir::setCurrent(testDataDir + "/trackProducts/work");
    QbsRunParameters params(QStringList() << "-f" << "trackProducts.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling foo.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling bar.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling zoo.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product1"));
    QVERIFY(m_qbsStdout.contains("linking product2"));
    QVERIFY(m_qbsStdout.contains("linking product3"));

    waitForNewTimestamp();
    QFile::remove("zoo.cpp");
    QFile::remove("product3.qbs");
    QFile::remove("trackProducts.qbs");
    QFile::copy("../before/trackProducts.qbs", "trackProducts.qbs");
    touch("trackProducts.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling foo.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling bar.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling zoo.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product1"));
    QVERIFY(!m_qbsStdout.contains("linking product2"));
    QVERIFY(!m_qbsStdout.contains("linking product3"));
}

void TestBlackbox::transformers()
{
    QDir::setCurrent(testDataDir + "/transformers");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::uic()
{
    QDir::setCurrent(testDataDir + "/uic");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::wildcardRenaming()
{
    QDir::setCurrent(testDataDir + "/wildcard_renaming");
    QCOMPARE(runQbs(QbsRunParameters("install")), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/pioniere.txt").exists());
    QFile::rename(QDir::currentPath() + "/pioniere.txt", QDir::currentPath() + "/fdj.txt");
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("install"), QStringList("--remove-first"))), 0);
    QVERIFY(!QFileInfo(defaultInstallRoot + "/pioniere.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/fdj.txt").exists());
}

void TestBlackbox::recursiveRenaming()
{
    QDir::setCurrent(testDataDir + "/recursive_renaming");
    QCOMPARE(runQbs(QbsRunParameters("install")), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/wasser.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/subdir/blubb.txt").exists());
    waitForNewTimestamp();
    QVERIFY(QFile::rename(QDir::currentPath() + "/dir/wasser.txt", QDir::currentPath() + "/dir/wein.txt"));
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("install"), QStringList("--remove-first"))), 0);
    QVERIFY(!QFileInfo(defaultInstallRoot + "/dir/wasser.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/wein.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/subdir/blubb.txt").exists());
}

void TestBlackbox::recursiveWildcards()
{
    QDir::setCurrent(testDataDir + "/recursive_wildcards");
    QCOMPARE(runQbs(QbsRunParameters("install")), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/file1.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/file2.txt").exists());
}

void TestBlackbox::ruleConditions()
{
    QDir::setCurrent(testDataDir + "/ruleConditions");
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFileInfo(buildDir + HostOsInfo::appendExecutableSuffix("/zorted")).exists());
    QVERIFY(QFileInfo(buildDir + HostOsInfo::appendExecutableSuffix("/unzorted")).exists());
    QVERIFY(QFileInfo(buildDir + "/zorted.foo.narf.zort").exists());
    QVERIFY(!QFileInfo(buildDir + "/unzorted.foo.narf.zort").exists());
}

void TestBlackbox::ruleCycle()
{
    QDir::setCurrent(testDataDir + "/ruleCycle");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("Cycle detected in rule dependencies"));
}

void TestBlackbox::trackAddQObjectHeader()
{
    QDir::setCurrent(testDataDir + "/missingqobjectheader");
    const QString qbsFileName("missingheader.qbs");
    QFile qbsFile(qbsFileName);
    QVERIFY(qbsFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    qbsFile.write("import qbs.base 1.0\nCppApplication {\n    Depends { name: 'Qt.core' }\n"
                  "    files: ['main.cpp', 'myobject.cpp']\n}");
    qbsFile.close();
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    waitForNewTimestamp();
    QVERIFY(qbsFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    qbsFile.write("import qbs.base 1.0\nCppApplication {\n    Depends { name: 'Qt.core' }\n"
                  "    files: ['main.cpp', 'myobject.cpp','myobject.h']\n}");
    qbsFile.close();
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::trackRemoveQObjectHeader()
{
    QDir::setCurrent(testDataDir + "/missingqobjectheader");
    const QString qbsFileName("missingheader.qbs");
    QFile qbsFile(qbsFileName);
    QVERIFY(qbsFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    qbsFile.write("import qbs.base 1.0\nCppApplication {\n    Depends { name: 'Qt.core' }\n"
                  "    files: ['main.cpp', 'myobject.cpp','myobject.h']\n}");
    qbsFile.close();
    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);
    waitForNewTimestamp();
    QVERIFY(qbsFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    qbsFile.write("import qbs.base 1.0\nCppApplication {\n    Depends { name: 'Qt.core' }\n"
                  "    files: ['main.cpp', 'myobject.cpp']\n}");
    qbsFile.close();
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
}

void TestBlackbox::overrideProjectProperties()
{
    QDir::setCurrent(testDataDir + "/overrideProjectProperties");
    QCOMPARE(runQbs(QbsRunParameters(QStringList()
                                     << QLatin1String("-f")
                                     << QLatin1String("project.qbs")
                                     << QLatin1String("project.nameSuffix:ForYou")
                                     << QLatin1String("project.someBool:false")
                                     << QLatin1String("project.someInt:156")
                                     << QLatin1String("project.someStringList:one")
                                     << QLatin1String("MyAppForYou.mainFile:main.cpp"))), 0);
    QVERIFY(QFile::exists(buildDir + HostOsInfo::appendExecutableSuffix("/MyAppForYou")));

    QVERIFY(QFile::remove(buildGraphPath));
    QbsRunParameters params;
    params.arguments << QLatin1String("-f") << QLatin1String("project_using_helper_lib.qbs");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);

    rmDirR(buildDir);
    params.arguments = QStringList() << QLatin1String("-f")
            << QLatin1String("project_using_helper_lib.qbs")
            << QLatin1String("project.linkSuccessfully:true");
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::productProperties()
{
    QDir::setCurrent(testDataDir + "/productproperties");
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << QLatin1String("-f")
                                     << QLatin1String("project.qbs"))), 0);
    QVERIFY(QFile::exists(buildDir + HostOsInfo::appendExecutableSuffix("/blubb_user")));
}

void TestBlackbox::propertyChanges()
{
    QDir::setCurrent(testDataDir + "/propertyChanges");
    QFile projectFile("project.qbs");
    QbsRunParameters params(QStringList() << "-f" << "project.qbs");

    // Initial build.
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product 1.debug"));
    QVERIFY(m_qbsStdout.contains("generated.txt"));
    QVERIFY(m_qbsStdout.contains("Making output from input"));
    QFile generatedFile(buildDir + QLatin1String("/generated.txt"));
    QVERIFY(generatedFile.open(QIODevice::ReadOnly));
    QCOMPARE(generatedFile.readAll(), QByteArray("prefix 1contents 1suffix 1"));
    generatedFile.close();

    // Incremental build with no changes.
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));

    // Incremental build with no changes, but updated project file timestamp.
    waitForNewTimestamp();
    QVERIFY(projectFile.open(QIODevice::ReadWrite | QIODevice::Append));
    projectFile.write("\n");
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));

    // Incremental build, input property changed for first product
    waitForNewTimestamp();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    QByteArray contents = projectFile.readAll();
    contents.replace("blubb1", "blubb01");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product 1.debug"));
    QVERIFY(!m_qbsStdout.contains("linking product 2"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("linking library"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));

    // Incremental build, input property changed via project for second product.
    waitForNewTimestamp();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("blubb2", "blubb02");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));

    // Incremental build, input property changed via command line for second product.
    params.arguments << "project.projectDefines:blubb002";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    params.arguments.removeLast();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));

    // Incremental build, input property changed via environment for third product.
    params.environment.insert("QBS_BLACKBOX_DEFINE", "newvalue");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(!m_qbsStdout.contains("linking product 2"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    params.environment.clear();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(!m_qbsStdout.contains("linking product 2"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));

    // Incremental build, module property changed via command line.
    params.arguments << "qbs.enableDebugCode:false";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product 1.release"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    params.arguments.removeLast();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product 1.debug"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));

    // Not actually necessary, but qbs cannot know that, since a property change is potentially
    // relevant to all rules.
    QVERIFY(m_qbsStdout.contains("Making output from input"));

    // Incremental build, non-essential dependency removed.
    waitForNewTimestamp();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("Depends { name: 'library' }", "// Depends { name: 'library' }");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(m_qbsStdout.contains("linking product 2"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("linking library"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));

    // Incremental build, prepare script of a transformer changed.
    waitForNewTimestamp();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("contents 1", "contents 2");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(generatedFile.open(QIODevice::ReadOnly));
    QCOMPARE(generatedFile.readAll(), QByteArray("prefix 1contents 2suffix 1"));
    generatedFile.close();

    // Incremental build, product property used in JavaScript command changed.
    waitForNewTimestamp();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("prefix 1", "prefix 2");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(generatedFile.open(QIODevice::ReadOnly));
    QCOMPARE(generatedFile.readAll(), QByteArray("prefix 2contents 2suffix 1"));
    generatedFile.close();

    // Incremental build, project property used in JavaScript command changed.
    waitForNewTimestamp();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("suffix 1", "suffix 2");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(generatedFile.open(QIODevice::ReadOnly));
    QCOMPARE(generatedFile.readAll(), QByteArray("prefix 2contents 2suffix 2"));
    generatedFile.close();

    // Incremental build, prepare script of a rule in a module changed.
    waitForNewTimestamp();
    QFile moduleFile("modules/TestModule/module.qbs");
    QVERIFY(moduleFile.open(QIODevice::ReadWrite));
    contents = moduleFile.readAll();
    contents.replace("// print('Change in source code')", "print('Change in source code')");
    moduleFile.resize(0);
    moduleFile.write(contents);
    moduleFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(m_qbsStdout.contains("Making output from input"));
}

void TestBlackbox::disabledProduct()
{
    QDir::setCurrent(testDataDir + "/disabledProduct");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::disabledProject()
{
    QDir::setCurrent(testDataDir + "/disabledProject");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::disableProduct()
{
    QDir::setCurrent(testDataDir + "/disable-product");
    QCOMPARE(runQbs(), 0);
    waitForNewTimestamp();
    QFile projectFile("project.qbs");
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    QByteArray content = projectFile.readAll();
    content.replace("// condition: false", "condition: false");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::duplicateProductNames()
{
    QDir::setCurrent(testDataDir + "/duplicateProductNames");
    QFETCH(QString, projectFileName);
    QbsRunParameters params;
    params.expectFailure = true;
    params.arguments = QStringList() << "-f" << projectFileName;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("Duplicate product name"));
}

void TestBlackbox::duplicateProductNames_data()
{
    QTest::addColumn<QString>("projectFileName");
    QTest::newRow("Names explicitly set") << QString("explicit.qbs");
    QTest::newRow("Unnamed products in same file") << QString("implicit.qbs");
    QTest::newRow("Unnamed products in files of the same name") << QString("implicit-indirect.qbs");
}

void TestBlackbox::dynamicLibs()
{
    QDir::setCurrent(testDataDir + "/dynamicLibs");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::dynamicRuleOutputs()
{
    const QString testDir = testDataDir + "/dynamicRuleOutputs";
    QDir::setCurrent(testDir);
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDir + "/work");
    QCOMPARE(runQbs(), 0);

    const QString appFile = buildDir + "/genlexer" + QTC_HOST_EXE_SUFFIX;
    const QString headerFile1 = buildDir + "/GeneratedFiles/genlexer/numberscanner.h";
    const QString sourceFile1 = buildDir + "/GeneratedFiles/genlexer/numberscanner.c";
    const QString sourceFile2 = buildDir + "/GeneratedFiles/genlexer/lex.yy.c";

    // Check build #1: source and header file name are specified in numbers.l
    QVERIFY(QFile::exists(appFile));
    QVERIFY(QFile::exists(headerFile1));
    QVERIFY(QFile::exists(sourceFile1));
    QVERIFY(!QFile::exists(sourceFile2));

    QDateTime appFileTimeStamp1 = QFileInfo(appFile).lastModified();
    waitForNewTimestamp();
    QFile::remove("numbers.l");
    QFile::copy("../after/numbers.l", "numbers.l");
    touch("numbers.l");
    QCOMPARE(runQbs(), 0);

    // Check build #2: no file names are specified in numbers.l
    //                 flex will default to lex.yy.c without header file.
    QDateTime appFileTimeStamp2 = QFileInfo(appFile).lastModified();
    QVERIFY(appFileTimeStamp1 < appFileTimeStamp2);
    QVERIFY(!QFile::exists(headerFile1));
    QVERIFY(!QFile::exists(sourceFile1));
    QVERIFY(QFile::exists(sourceFile2));

    waitForNewTimestamp();
    QFile::remove("numbers.l");
    QFile::copy("../before/numbers.l", "numbers.l");
    touch("numbers.l");
    QCOMPARE(runQbs(), 0);

    // Check build #3: source and header file name are specified in numbers.l
    QDateTime appFileTimeStamp3 = QFileInfo(appFile).lastModified();
    QVERIFY(appFileTimeStamp2 < appFileTimeStamp3);
    QVERIFY(QFile::exists(appFile));
    QVERIFY(QFile::exists(headerFile1));
    QVERIFY(QFile::exists(sourceFile1));
    QVERIFY(!QFile::exists(sourceFile2));
}

void TestBlackbox::erroneousFiles_data()
{
    QTest::addColumn<QString>("errorMessage");
    QTest::newRow("nonexistentWorkingDir")
            << "The working directory '/does/not/exist' for process '.*ls' is invalid.";
}

void TestBlackbox::erroneousFiles()
{
    QFETCH(QString, errorMessage);
    QDir::setCurrent(testDataDir + "/erroneous/" + QTest::currentDataTag());
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QString err = QString::fromLocal8Bit(m_qbsStderr);
    if (!err.contains(QRegExp(errorMessage))) {
        qDebug() << "Output:  " << err;
        qDebug() << "Expected: " << errorMessage;
        QFAIL("Unexpected error message.");
    }
}

void TestBlackbox::explicitlyDependsOn()
{
    QDir::setCurrent(testDataDir + "/explicitlyDependsOn");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("Creating output artifact"));
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("Creating output artifact"));
    waitForNewTimestamp();
    touch("dependency.txt");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("Creating output artifact"));
}

void TestBlackbox::fileDependencies()
{
    QDir::setCurrent(testDataDir + "/fileDependencies");
    rmDirR(buildDir);
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling narf.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling zort.cpp"));
    const QString productFileName = HostOsInfo::appendExecutableSuffix(buildDir + "/myapp");
    QVERIFY2(QFile::exists(productFileName), qPrintable(productFileName));

    // Incremental build without changes.
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("compiling"));
    QVERIFY(!m_qbsStdout.contains("linking"));

    // Incremental build with changed file dependency.
    waitForNewTimestamp();
    touch("awesomelib/awesome.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling narf.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling zort.cpp"));

    // Incremental build with changed 2nd level file dependency.
    waitForNewTimestamp();
    touch("awesomelib/magnificent.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling narf.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling zort.cpp"));
}

void TestBlackbox::jsExtensionsFile()
{
    QDir::setCurrent(testDataDir + "/jsextensions");
    QbsRunParameters params(QStringList() << "-nf" << "file.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!QFileInfo("original.txt").exists());
    QFile copy("copy.txt");
    QVERIFY(copy.exists());
    QVERIFY(copy.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = copy.readAll().trimmed().split('\n');
    QCOMPARE(lines.count(), 2);
    QCOMPARE(lines.at(0).trimmed().constData(), "false");
    QCOMPARE(lines.at(1).trimmed().constData(), "true");
}

void TestBlackbox::jsExtensionsFileInfo()
{
    QDir::setCurrent(testDataDir + "/jsextensions");
    QbsRunParameters params(QStringList() << "-nf" << "fileinfo.qbs");
    QCOMPARE(runQbs(params), 0);
    QFile output("output.txt");
    QVERIFY(output.exists());
    QVERIFY(output.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = output.readAll().trimmed().split('\n');
    QCOMPARE(lines.count(), 19);
    QCOMPARE(lines.at(0).trimmed().constData(), "blubb");
    QCOMPARE(lines.at(1).trimmed().constData(), "blubb.tar");
    QCOMPARE(lines.at(2).trimmed().constData(), "blubb.tar.gz");
    QCOMPARE(lines.at(3).trimmed().constData(), "/tmp/blubb.tar.gz");
    QCOMPARE(lines.at(4).trimmed().constData(), "c:/tmp/blubb.tar.gz");
    QCOMPARE(lines.at(5).trimmed().constData(), "true");
    QCOMPARE(lines.at(6).trimmed().constData(), "true");
    QCOMPARE(lines.at(7).trimmed().constData(), "false");
    QCOMPARE(lines.at(8).trimmed().constData(), "false");
    QCOMPARE(lines.at(9).trimmed().constData(), "/tmp/blubb.tar.gz");
    QCOMPARE(lines.at(10).trimmed().constData(), "/tmp");
    QCOMPARE(lines.at(11).trimmed().constData(), "/tmp");
    QCOMPARE(lines.at(12).trimmed().constData(), "/");
    QCOMPARE(lines.at(13).trimmed().constData(), "d:/");
    QCOMPARE(lines.at(14).trimmed().constData(), "blubb.tar.gz");
    QCOMPARE(lines.at(15).trimmed().constData(), "tmp/blubb.tar.gz");
    QCOMPARE(lines.at(16).trimmed().constData(), "../blubb.tar.gz");
    QCOMPARE(lines.at(17).trimmed().constData(), "\\tmp\\blubb.tar.gz");
    QCOMPARE(lines.at(18).trimmed().constData(), "c:\\tmp\\blubb.tar.gz");
}

void TestBlackbox::jsExtensionsProcess()
{
    QDir::setCurrent(testDataDir + "/jsextensions");
    QbsRunParameters params(QStringList() << "-nf" << "process.qbs" << "project.qbsFilePath:"
                            + qbsExecutableFilePath);
    QCOMPARE(runQbs(params), 0);
    QFile output("output.txt");
    QVERIFY(output.exists());
    QVERIFY(output.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = output.readAll().trimmed().split('\n');
    QCOMPARE(lines.count(), 7);
    QCOMPARE(lines.at(0).trimmed().constData(), "0");
    QVERIFY(lines.at(1).startsWith("qbs "));
    QCOMPARE(lines.at(2).trimmed().constData(), "true");
    QCOMPARE(lines.at(3).trimmed().constData(), "true");
    QCOMPARE(lines.at(4).trimmed().constData(), "0");
    QVERIFY(lines.at(5).startsWith("qbs "));
    QCOMPARE(lines.at(6).trimmed().constData(), "false");
}

void TestBlackbox::jsExtensionsPropertyList()
{
    if (!HostOsInfo::isOsxHost())
        SKIP_TEST("temporarily only applies on OS X");

    QDir::setCurrent(testDataDir + "/jsextensions");
    QbsRunParameters params(QStringList() << "-nf" << "propertylist.qbs");
    QCOMPARE(runQbs(params), 0);
    QFile file1("test.json");
    QVERIFY(file1.exists());
    QVERIFY(file1.open(QIODevice::ReadOnly));
    QFile file2("test.xml");
    QVERIFY(file2.exists());
    QVERIFY(file2.open(QIODevice::ReadOnly));
    QFile file3("test2.json");
    QVERIFY(file3.exists());
    QVERIFY(file3.open(QIODevice::ReadOnly));
    QByteArray file1Contents = file1.readAll();
    QCOMPARE(file3.readAll(), file1Contents);
    QCOMPARE(file1Contents, file2.readAll());
    QFile file4("test.openstep.plist");
    QVERIFY(file4.exists());
    QFile file5("test3.json");
    QVERIFY(file5.exists());
    QVERIFY(file5.open(QIODevice::ReadOnly));
    QVERIFY(file1Contents != file5.readAll());
}

void TestBlackbox::jsExtensionsTextFile()
{
    QDir::setCurrent(testDataDir + "/jsextensions");
    QbsRunParameters params(QStringList() << "-nf" << "textfile.qbs");
    QCOMPARE(runQbs(params), 0);
    QFile file1("file1.txt");
    QVERIFY(file1.exists());
    QVERIFY(file1.open(QIODevice::ReadOnly));
    QCOMPARE(file1.size(), qint64(0));
    QFile file2("file2.txt");
    QVERIFY(file2.exists());
    QVERIFY(file2.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = file2.readAll().trimmed().split('\n');
    QCOMPARE(lines.count(), 5);
    QCOMPARE(lines.at(0).trimmed().constData(), "false");
    QCOMPARE(lines.at(1).trimmed().constData(), "First line.");
    QCOMPARE(lines.at(2).trimmed().constData(), "Second line.");
    QCOMPARE(lines.at(3).trimmed().constData(), "Third line.");
    QCOMPARE(lines.at(4).trimmed().constData(), "true");
}

void TestBlackbox::inheritQbsSearchPaths()
{
    QDir::setCurrent(testDataDir + "/inheritQbsSearchPaths");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::mocCppIncluded()
{
    QDir::setCurrent(testDataDir + "/moc_hpp_included");
    QCOMPARE(runQbs(), 0); // Initial build.

    // Touch header and try again.
    waitForNewTimestamp();
    QFile headerFile("object.h");
    QVERIFY2(headerFile.open(QIODevice::WriteOnly | QIODevice::Append),
             qPrintable(headerFile.errorString()));
    headerFile.write("\n");
    headerFile.close();
    QCOMPARE(runQbs(), 0);

    // Touch cpp file and try again.
    waitForNewTimestamp();
    QFile cppFile("object.cpp");
    QVERIFY2(cppFile.open(QIODevice::WriteOnly | QIODevice::Append),
             qPrintable(cppFile.errorString()));
    cppFile.write("\n");
    cppFile.close();
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::objC()
{
    QDir::setCurrent(testDataDir + "/objc");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::qmlDebugging()
{
    QDir::setCurrent(testDataDir + "/qml-debugging");
    QCOMPARE(runQbs(), 0);
    QProcess nm;
    nm.start("nm", QStringList(HostOsInfo::appendExecutableSuffix(buildDir + "/debuggable-app")));
    if (nm.waitForStarted()) { // Let's ignore hosts without nm.
        QVERIFY2(nm.waitForFinished(), qPrintable(nm.errorString()));
        QVERIFY2(nm.exitCode() == 0, nm.readAllStandardError().constData());
        const QByteArray output = nm.readAllStandardOutput();
        QVERIFY2(output.toLower().contains("debugginghelper"), output.constData());
    }
}

void TestBlackbox::projectWithPropertiesItem()
{
    QDir::setCurrent(testDataDir + "/project-with-properties-item");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::properQuoting()
{
    QDir::setCurrent(testDataDir + "/proper quoting");
    QCOMPARE(runQbs(), 0);
    QbsRunParameters params(QLatin1String("run"), QStringList() << "-q" << "-p" << "Hello World");
    params.expectFailure = true; // Because the exit code is non-zero.
    QCOMPARE(runQbs(params), 156);
    const char * const expectedOutput = "whitespaceless\ncontains space\ncontains\ttab\n"
            "backslash\\\nHello World! The magic number is 156.";
    QCOMPARE(unifiedLineEndings(m_qbsStdout).constData(), expectedOutput);
}

void TestBlackbox::propertiesBlocks()
{
    QDir::setCurrent(testDataDir + "/propertiesBlocks");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::installedApp()
{
    QDir::setCurrent(testDataDir + "/installed_artifact");

    QCOMPARE(runQbs(QbsRunParameters("install")), 0);
    QVERIFY(QFile::exists(defaultInstallRoot
            + HostOsInfo::appendExecutableSuffix(QLatin1String("/usr/bin/installedApp"))));

    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("install"), QStringList("--install-root")
                                     << (testDataDir + "/installed-app"))), 0);
    QVERIFY(QFile::exists(testDataDir
            + HostOsInfo::appendExecutableSuffix("/installed-app/usr/bin/installedApp")));

    QFile addedFile(defaultInstallRoot + QLatin1String("/blubb.txt"));
    QVERIFY(addedFile.open(QIODevice::WriteOnly));
    addedFile.close();
    QVERIFY(addedFile.exists());
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("install"), QStringList("--remove-first"))), 0);
    QVERIFY(QFile::exists(defaultInstallRoot
            + HostOsInfo::appendExecutableSuffix(QLatin1String("/usr/bin/installedApp"))));
    QVERIFY(QFile::exists(defaultInstallRoot + QLatin1String("/usr/src/main.cpp")));
    QVERIFY(!addedFile.exists());

    // Check whether changing install parameters on the product causes re-installation.
    QFile projectFile("installed_artifact.qbs");
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    QByteArray content = projectFile.readAll();
    content.replace("qbs.installPrefix: \"/usr\"", "qbs.installPrefix: '/usr/local'");
    waitForNewTimestamp();
    projectFile.resize(0);
    projectFile.write(content);
    QVERIFY(projectFile.flush());
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("install"))), 0);
    QVERIFY(QFile::exists(defaultInstallRoot
            + HostOsInfo::appendExecutableSuffix(QLatin1String("/usr/local/bin/installedApp"))));
    QVERIFY(QFile::exists(defaultInstallRoot + QLatin1String("/usr/local/src/main.cpp")));

    // Check whether changing install parameters on the artifact causes re-installation.
    content.replace("qbs.installDir: \"bin\"", "qbs.installDir: 'custom'");
    waitForNewTimestamp();
    projectFile.resize(0);
    projectFile.write(content);
    QVERIFY(projectFile.flush());
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("install"))), 0);
    QVERIFY(QFile::exists(defaultInstallRoot
            + HostOsInfo::appendExecutableSuffix(QLatin1String("/usr/local/custom/installedApp"))));

    // Check whether changing install parameters on a source file causes re-installation.
    content.replace("qbs.installDir: \"src\"", "qbs.installDir: 'source'");
    waitForNewTimestamp();
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("install"))), 0);
    QVERIFY(QFile::exists(defaultInstallRoot + QLatin1String("/usr/local/source/main.cpp")));

    rmDirR(buildDir);
    QbsRunParameters params;
    params.command = "install";
    params.arguments << "--no-build";
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("No build graph"));
}

void TestBlackbox::toolLookup()
{
    QbsRunParameters params(QLatin1String("setup-toolchains"), QStringList("--help"));
    params.useProfile = false;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::checkProjectFilePath()
{
    QDir::setCurrent(testDataDir + "/project_filepath_check");
    QbsRunParameters params(QStringList("-f") << "project1.qbs");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(params), 0);

    params.arguments = QStringList("-f") << "project2.qbs";
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("project file"));

    params.arguments = QStringList("-f") << "project2.qbs" << "--force";
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStderr.contains("project file"));
}

class TemporaryDefaultProfileRemover
{
public:
    TemporaryDefaultProfileRemover(const SettingsPtr &settings)
        : m_settings(settings), m_defaultProfile(settings->defaultProfile())
    {
        m_settings->remove(QLatin1String("defaultProfile"));
    }

    ~TemporaryDefaultProfileRemover()
    {
        if (!m_defaultProfile.isEmpty())
            m_settings->setValue(QLatin1String("defaultProfile"), m_defaultProfile);
    }

private:
    SettingsPtr m_settings;
    const QString m_defaultProfile;
};

void TestBlackbox::missingProfile()
{
    SettingsPtr settings = qbsSettings(QString());
    TemporaryDefaultProfileRemover dpr(settings);
    QVERIFY(settings->defaultProfile().isEmpty());
    QDir::setCurrent(testDataDir + "/project_filepath_check");
    QbsRunParameters params;
    params.arguments = QStringList("-f") << "project1.qbs";
    params.expectFailure = true;
    params.useProfile = false;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("No profile"));
}

void TestBlackbox::testAssembly()
{
    SettingsPtr settings = qbsSettings(QString());
    Profile profile(buildProfileName, settings.data());
    bool haveGcc = profile.value("qbs.toolchain").toStringList().contains("gcc");
    QDir::setCurrent(testDataDir + "/assembly");
    QVERIFY(runQbs() == 0);
    QCOMPARE((bool)m_qbsStdout.contains("compiling testa.s"), haveGcc);
    QCOMPARE((bool)m_qbsStdout.contains("compiling testb.S"), haveGcc);
    QCOMPARE((bool)m_qbsStdout.contains("compiling testc.sx"), haveGcc);
    QCOMPARE((bool)m_qbsStdout.contains("creating libtesta.a"), haveGcc);
    QCOMPARE((bool)m_qbsStdout.contains("creating libtestb.a"), haveGcc);
    QCOMPARE((bool)m_qbsStdout.contains("creating libtestc.a"), haveGcc);
}

void TestBlackbox::testNsis()
{
    QStringList regKeys;
    regKeys << QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\NSIS")
            << QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\NSIS");

    QStringList paths = QProcessEnvironment::systemEnvironment().value("PATH")
            .split(HostOsInfo::pathListSeparator(), QString::SkipEmptyParts);

    foreach (const QString &key, regKeys) {
        QSettings settings(key, QSettings::NativeFormat);
        QString str = settings.value(QLatin1String(".")).toString();
        if (!str.isEmpty())
            paths.prepend(str);
    }

    bool haveMakeNsis = false;
    foreach (const QString &path, paths) {
        if (QFile::exists(QDir::fromNativeSeparators(path) +
                          HostOsInfo::appendExecutableSuffix(QLatin1String("/makensis")))) {
            haveMakeNsis = true;
            break;
        }
    }

    if (!haveMakeNsis) {
        SKIP_TEST("makensis is not installed");
        return;
    }

    SettingsPtr settings = qbsSettings(QString());
    Profile profile(buildProfileName, settings.data());
    bool targetIsWindows = profile.value("qbs.targetOS").toStringList().contains("windows");
    QDir::setCurrent(testDataDir + "/nsis");
    QVERIFY(runQbs() == 0);
    QCOMPARE((bool)m_qbsStdout.contains("compiling hello.nsi"), targetIsWindows);
    QCOMPARE((bool)m_qbsStdout.contains("SetCompressor ignored due to previous call with the /FINAL switch"), targetIsWindows);
    QVERIFY(!QFile::exists(defaultInstallRoot + "/you-should-not-see-a-file-with-this-name.exe"));
}

void TestBlackbox::testEmbedInfoPlist()
{
    if (!HostOsInfo::isOsxHost())
        SKIP_TEST("only applies on OS X");

    QDir::setCurrent(testDataDir + QLatin1String("/embedInfoPlist"));

    QbsRunParameters params;
    params.command = QLatin1String("run");
    QCOMPARE(runQbs(params), 0);

    params.arguments = QStringList(QLatin1String("cpp.embedInfoPlist:false"));
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
}

static bool haveWiX()
{
    QStringList regKeys;
    regKeys << QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows Installer XML\\")
            << QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Installer XML\\");

    const QStringList versions = QStringList() << "4.0" << "3.9" << "3.8" << "3.7"
                                               << "3.6" << "3.5" << "3.0" << "2.0";

    QStringList paths = QProcessEnvironment::systemEnvironment().value("PATH")
            .split(HostOsInfo::pathListSeparator(), QString::SkipEmptyParts);

    foreach (const QString &version, versions) {
        foreach (const QString &key, regKeys) {
            QSettings settings(key + version, QSettings::NativeFormat);
            QString str = settings.value(QLatin1String("InstallRoot")).toString();
            if (!str.isEmpty())
                paths.prepend(str);
        }
    }

    foreach (const QString &path, paths) {
        if (QFile::exists(QDir::fromNativeSeparators(path) +
                          HostOsInfo::appendExecutableSuffix(QLatin1String("/candle"))) &&
            QFile::exists(QDir::fromNativeSeparators(path) +
                          HostOsInfo::appendExecutableSuffix(QLatin1String("/light")))) {
            return true;
        }
    }

    return false;
}

void TestBlackbox::testWiX()
{
    if (!HostOsInfo::isWindowsHost()) {
        SKIP_TEST("only applies on Windows");
        return;
    }

    if (!haveWiX()) {
        SKIP_TEST("WiX is not installed");
        return;
    }

    SettingsPtr settings = qbsSettings(QString());
    Profile profile(buildProfileName, settings.data());
    const QByteArray arch = profile.value("qbs.architecture").toString().toLatin1();

    QDir::setCurrent(testDataDir + "/wix");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling QbsSetup.wxs"));
    QVERIFY(m_qbsStdout.contains("compiling QbsBootstrapper.wxs"));
    QVERIFY(m_qbsStdout.contains("linking qbs-" + arch + ".msi"));
    QVERIFY(m_qbsStdout.contains("linking qbs-setup-" + arch + ".exe"));
    QVERIFY(QFile::exists(buildDir + "/qbs-" + arch + ".msi"));
    QVERIFY(QFile::exists(buildDir + "/qbs-setup-" + arch + ".exe"));
}

QTEST_MAIN(TestBlackbox)
