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

#include "tst_blackbox.h"
#include <app/shared/qbssettings.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/installoptions.h>

#include <QLocale>
#include <QTemporaryFile>
#include <QScriptEngine>
#include <QScriptValue>

#if QT_VERSION >= 0x050000
#define SKIP_TEST(message) QSKIP(message)
#else
#define SKIP_TEST(message) QSKIP(message, SkipAll)
#endif

using qbs::InstallOptions;
using qbs::Internal::HostOsInfo;
using qbs::Internal::removeDirectoryWithContents;

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
      buildProfile(QLatin1String("qbs_autotests")),
      buildDir(buildProfile + QLatin1String("-debug")),
      defaultInstallRoot(buildDir + QLatin1Char('/') + InstallOptions::defaultInstallRoot()),
      buildGraphPath(buildDir + QLatin1Char('/') + buildDir + QLatin1String(".bg"))
{
    QLocale::setDefault(QLocale::c());
}

int TestBlackbox::runQbs(QStringList arguments, bool expectFailure, bool useProfile)
{
    if (useProfile)
        arguments.append(QLatin1String("profile:") + buildProfile);
    QString cmdLine = qbsExecutableFilePath;
    foreach (const QString &str, arguments)
        cmdLine += QLatin1String(" \"") + str + QLatin1Char('"');
    QProcess process;
    process.start(cmdLine);
    if (!process.waitForStarted() || !process.waitForFinished()) {
        m_qbsStderr = process.readAllStandardError();
        if (!expectFailure)
            qDebug("%s", qPrintable(process.errorString()));
        return -1;
    }

    m_qbsStderr = process.readAllStandardError();
    m_qbsStdout = process.readAllStandardOutput();
    if ((process.exitStatus() != QProcess::NormalExit
             || process.exitCode() != 0) && !expectFailure) {
        qDebug("%s", m_qbsStderr.constData());
        qDebug("%s", process.readAllStandardOutput().constData());
    }
    return process.exitCode();
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

void TestBlackbox::initTestCase()
{
    QVERIFY(QFile::exists(qbsExecutableFilePath));
    QProcess process;
    process.start(qbsExecutableFilePath, QStringList() << "config" << "--list");
    QVERIFY(process.waitForStarted());
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitCode(), 0);
    bool found = false;
    forever {
        QByteArray line = process.readLine();
        if (line.isEmpty())
            break;
        if (line.startsWith(QByteArray("profiles.") + buildProfile.toLatin1() + QByteArray("."))) {
            found = true;
            break;
        }
    }
    if (!found)
        qWarning("The build profile '%s' could not be found. Please set it up on your machine.",
                 qPrintable(buildProfile));
    QVERIFY(found);

    // Initialize the test data directory.
    QVERIFY(testDataDir != testSourceDir);
    rmDirR(testDataDir);
    QDir().mkpath(testDataDir);
    ccp(testSourceDir, testDataDir);
}

void TestBlackbox::build_project_data()
{
    QTest::addColumn<QString>("projectSubDir");
    QTest::addColumn<QString>("productFileName");
    QTest::newRow("BPs in Sources")
            << QString("buildproperties_source")
            << QString(HostOsInfo::appendExecutableSuffix(buildDir + "/HelloWorld"));
    QTest::newRow("link static libs")
            << QString("link_staticlib")
            << QString(buildDir + QLatin1String("/")
                       + HostOsInfo::appendExecutableSuffix("HelloWorld"));
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
    QCOMPARE(runQbs(), 0);
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

    QCOMPARE(runQbs(QStringList() << "-n"), 0);
    const QStringList &buildDirContents
            = QDir(buildDir).entryList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
    QVERIFY2(buildDirContents.isEmpty(), qPrintable(buildDirContents.join(" ")));
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

    QCOMPARE(runQbs(QStringList() << QLatin1String("resolve")), 0);
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

    QCOMPARE(runQbs(QStringList() << QLatin1String("resolve") << QLatin1String("-n")), 0);
    QVERIFY2(!QFile::exists(productFileName), qPrintable(productFileName));
    QVERIFY2(!QFile::exists(buildGraphPath), qPrintable(buildGraphPath));
}

void TestBlackbox::clean()
{
    const QString appObjectFilePath = buildDir + "/.obj/app/main.cpp" + QTC_HOST_OBJECT_SUFFIX;
    const QString appExeFilePath = buildDir + "/app" + QTC_HOST_EXE_SUFFIX;
    const QString depObjectFilePath = buildDir + "/.obj/dep/dep.cpp" + QTC_HOST_OBJECT_SUFFIX;
    const QString depExeFilePath = buildDir + "/dep" + QTC_HOST_EXE_SUFFIX;

    QDir::setCurrent(testDataDir + "/clean");

    // Default behavior: Remove only temporaries.
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFile(appObjectFilePath).exists());
    QVERIFY(QFile(appExeFilePath).exists());
    QVERIFY(QFile(depObjectFilePath).exists());
    QVERIFY(QFile(depExeFilePath).exists());
    QCOMPARE(runQbs(QStringList("clean")), 0);
    QVERIFY(!QFile(appObjectFilePath).exists());
    QVERIFY(QFile(appExeFilePath).exists());
    QVERIFY(!QFile(depObjectFilePath).exists());
    QVERIFY(QFile(depExeFilePath).exists());

    // Remove all.
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFile(appObjectFilePath).exists());
    QVERIFY(QFile(appExeFilePath).exists());
    QCOMPARE(runQbs(QStringList("clean") << "--all-artifacts"), 0);
    QVERIFY(!QFile(appObjectFilePath).exists());
    QVERIFY(!QFile(appExeFilePath).exists());
    QVERIFY(!QFile(depObjectFilePath).exists());
    QVERIFY(!QFile(depExeFilePath).exists());

    // Dry run.
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFile(appObjectFilePath).exists());
    QVERIFY(QFile(appExeFilePath).exists());
    QCOMPARE(runQbs(QStringList("clean") << "--all-artifacts" << "-n"), 0);
    QVERIFY(QFile(appObjectFilePath).exists());
    QVERIFY(QFile(appExeFilePath).exists());
    QVERIFY(QFile(depObjectFilePath).exists());
    QVERIFY(QFile(depExeFilePath).exists());

    // Product-wise, dependency only.
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFile(appObjectFilePath).exists());
    QVERIFY(QFile(appExeFilePath).exists());
    QVERIFY(QFile(depObjectFilePath).exists());
    QVERIFY(QFile(depExeFilePath).exists());
    QCOMPARE(runQbs(QStringList("clean") << "--all-artifacts" << "-p" << "dep"), 0);
    QVERIFY(QFile(appObjectFilePath).exists());
    QVERIFY(QFile(appExeFilePath).exists());
    QVERIFY(!QFile(depObjectFilePath).exists());
    QVERIFY(!QFile(depExeFilePath).exists());

    // Product-wise, dependent product only.
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFile(appObjectFilePath).exists());
    QVERIFY(QFile(appExeFilePath).exists());
    QVERIFY(QFile(depObjectFilePath).exists());
    QVERIFY(QFile(depExeFilePath).exists());
    QCOMPARE(runQbs(QStringList("clean") << "--all-artifacts" << "-p" << "app"), 0);
    QVERIFY(!QFile(appObjectFilePath).exists());
    QVERIFY(!QFile(appExeFilePath).exists());
    QVERIFY(QFile(depObjectFilePath).exists());
    QVERIFY(QFile(depExeFilePath).exists());
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
    QVERIFY(runQbs(QStringList(), true) != 0);

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
    const QStringList args = QStringList() << "-f" << "trackProducts.qbs";
    QCOMPARE(runQbs(args), 0);
    QVERIFY(m_qbsStdout.contains("compiling foo.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling bar.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product1"));
    QVERIFY(m_qbsStdout.contains("linking product2"));

    waitForNewTimestamp();
    ccp("../after", ".");
    touch("trackProducts.qbs");
    QCOMPARE(runQbs(args), 0);
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
    const QStringList args = QStringList() << "-f" << "trackProducts.qbs";
    QCOMPARE(runQbs(args), 0);
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
    QCOMPARE(runQbs(args), 0);
    QVERIFY(!m_qbsStdout.contains("compiling foo.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling bar.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling zoo.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product1"));
    QVERIFY(!m_qbsStdout.contains("linking product2"));
    QVERIFY(!m_qbsStdout.contains("linking product3"));
}

void TestBlackbox::wildcardRenaming()
{
    QDir::setCurrent(testDataDir + "/wildcard_renaming");
    QCOMPARE(runQbs(QStringList("install")), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/pioniere.txt").exists());
    QFile::rename(QDir::currentPath() + "/pioniere.txt", QDir::currentPath() + "/fdj.txt");
    QCOMPARE(runQbs(QStringList("install") << "--remove-first"), 0);
    QVERIFY(!QFileInfo(defaultInstallRoot + "/pioniere.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/fdj.txt").exists());
}

void TestBlackbox::recursiveRenaming()
{
    QDir::setCurrent(testDataDir + "/recursive_renaming");
    QCOMPARE(runQbs(QStringList("install")), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/wasser.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/subdir/blubb.txt").exists());
    waitForNewTimestamp();
    QVERIFY(QFile::rename(QDir::currentPath() + "/dir/wasser.txt", QDir::currentPath() + "/dir/wein.txt"));
    QCOMPARE(runQbs(QStringList("install") << "--remove-first"), 0);
    QVERIFY(!QFileInfo(defaultInstallRoot + "/dir/wasser.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/wein.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/subdir/blubb.txt").exists());
}

void TestBlackbox::recursiveWildcards()
{
    QDir::setCurrent(testDataDir + "/recursive_wildcards");
    QCOMPARE(runQbs(QStringList("install")), 0);
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

void TestBlackbox::codegen()
{
    QDir::setCurrent(testDataDir + "/codegen");
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFile::exists(buildDir + HostOsInfo::appendExecutableSuffix("/codegen")));
}

void TestBlackbox::missingQObjectHeader()
{
    QDir::setCurrent(testDataDir + "/missingqobjectheader");
    const QString qbsFileName("missingheader.qbs");
    QFile qbsFile(qbsFileName);
    QVERIFY(qbsFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    qbsFile.write("import qbs.base 1.0\nCppApplication {\n    Depends { name: 'Qt.core' }\n"
                  "    files: ['main.cpp', 'myobject.cpp']\n}");
    qbsFile.close();
    QVERIFY(runQbs(QStringList(), true) != 0);
    QVERIFY(qbsFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    qbsFile.write("import qbs.base 1.0\nCppApplication {\n    Depends { name: 'Qt.core' }\n"
                  "    files: ['main.cpp', 'myobject.cpp','myobject.h']\n}");
    qbsFile.close();
    QEXPECT_FAIL("", "FIXME: This case is known to be broken", Abort);
    QCOMPARE(runQbs(QStringList(), true), 0);
}

void TestBlackbox::productProperties()
{
    QDir::setCurrent(testDataDir + "/productproperties");
    QEXPECT_FAIL("", "FIXME: project property not known in referenced products", Abort);
    QCOMPARE(runQbs(QStringList() << QLatin1String("-f") << QLatin1String("project.qbs"), true), 0);
    QVERIFY(QFile::exists(buildDir + HostOsInfo::appendExecutableSuffix("/blubb_user")));
}

void TestBlackbox::propertyChanges()
{
    QDir::setCurrent(testDataDir + "/propertyChanges");
    QFile projectFile("project.qbs");

    // Initial build.
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));

    // Incremental build with no changes.
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));

    // Incremental build with no changes, but updated project file timestamp.
    waitForNewTimestamp();
    QVERIFY(projectFile.open(QIODevice::ReadWrite | QIODevice::Append));
    projectFile.write("\n");
    projectFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));

    // Incremental build, input property changed for first product
    waitForNewTimestamp();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    QByteArray contents = projectFile.readAll();
    contents.replace("blubb1", "blubb01");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));

    // Incremental build, input property changed indirectly for second build.
    waitForNewTimestamp();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("blubb2", "blubb02");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
}

void TestBlackbox::installedApp()
{
    QDir::setCurrent(testDataDir + "/installed_artifact");

    QCOMPARE(runQbs(QStringList("install")), 0);
    QVERIFY(QFile::exists(defaultInstallRoot
            + HostOsInfo::appendExecutableSuffix(QLatin1String("/bin/installedApp"))));

    QCOMPARE(runQbs(QStringList("install") << "--install-root" << (testDataDir + "/installed-app")), 0);
    QVERIFY(QFile::exists(testDataDir
            + HostOsInfo::appendExecutableSuffix("/installed-app/bin/installedApp")));

    QFile addedFile(defaultInstallRoot + QLatin1String("/blubb.txt"));
    QVERIFY(addedFile.open(QIODevice::WriteOnly));
    addedFile.close();
    QVERIFY(addedFile.exists());
    QCOMPARE(runQbs(QStringList("install") << "--remove-first"), 0);
    QVERIFY(QFile::exists(defaultInstallRoot
            + HostOsInfo::appendExecutableSuffix(QLatin1String("/bin/installedApp"))));
    QVERIFY(!addedFile.exists());
}

void TestBlackbox::toolLookup()
{
    QCOMPARE(runQbs(QStringList("detect-toolchains") << "--help", false, false), 0);
}

void TestBlackbox::checkProjectFilePath()
{
    QDir::setCurrent(testDataDir + "/project_filepath_check");
    QCOMPARE(runQbs(QStringList("-f") << "project1.qbs"), 0);
    QCOMPARE(runQbs(QStringList("-f") << "project1.qbs"), 0);

    QVERIFY(runQbs(QStringList("-f") << "project2.qbs", true) != 0);
    QVERIFY(m_qbsStderr.contains("project file"));

    QCOMPARE(runQbs(QStringList("-f") << "project2.qbs" << "--force"), 0);
    QVERIFY(m_qbsStderr.contains("project file"));
}

void TestBlackbox::missingProfile()
{
    SettingsPtr settings = qbsSettings();
    if (!settings->defaultProfile().isEmpty())
        SKIP_TEST("default profile exists");
    QDir::setCurrent(testDataDir + "/project_filepath_check");
    QVERIFY(runQbs(QStringList("-f") << "project1.qbs", true, false) != 0);
    QVERIFY(m_qbsStderr.contains("No profile"));
}

QTEST_MAIN(TestBlackbox)
