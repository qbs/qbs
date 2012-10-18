/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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
#include <tools/hostosinfo.h>


static QString initQbsExecutableFilePath()
{
    QString filePath = QCoreApplication::applicationDirPath() + QLatin1String("/../../../");
    filePath += "bin/qbs";
    filePath = qbs::HostOsInfo::appendExecutableSuffix(QDir::cleanPath(filePath));
    return filePath;
}

TestBlackbox::TestBlackbox()
    : testDataDir(QCoreApplication::applicationDirPath() + "/testWorkDir"),
      testSourceDir(QDir::cleanPath(SRCDIR "/testdata")),
      qbsExecutableFilePath(initQbsExecutableFilePath()),
      buildProfile(QLatin1String("qbs_autotests")),
      buildDir(QLatin1String("build/") + buildProfile + QLatin1String("-debug")),
#ifdef Q_OS_WIN
        objectSuffix(QLatin1String(".obj"))
#else
        objectSuffix(QLatin1String(".o"))
#endif
{
}

int TestBlackbox::runQbs(QStringList arguments, bool expectFailure)
{
    arguments.prepend(QLatin1String("profile:") + buildProfile);
    QString cmdLine = qbsExecutableFilePath;
    foreach (const QString &str, arguments)
        cmdLine += QLatin1String(" \"") + str + QLatin1Char('"');
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(cmdLine);
    if (!process.waitForStarted() || !process.waitForFinished()) {
        if (!expectFailure)
            qDebug("%s", qPrintable(process.errorString()));
        return -1;
    }

    if ((process.exitStatus() != QProcess::NormalExit
             || process.exitCode() != 0) && !expectFailure) {
        qDebug("%s", process.readAll().constData());
    }
    return process.exitCode();
}

/*!
  Recursive copy from directory to another.
  Note that this updates the file stamps on Linux but not on Windows.
  */
static void ccp(const QString &from, const QString &to)
{
    QDirIterator it(from, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    while (it.hasNext()) {
        it.next();
        QDir().mkpath(to +  "/" + it.fileName());
        ccp(from + "/" + it.fileName(), to +  "/" + it.fileName());
    }

    QDirIterator it2(from, QDir::Files| QDir::NoDotAndDotDot  | QDir::Hidden);
    while (it2.hasNext()) {
        it2.next();
        const QString dstFilePath = to +  "/" + it2.fileName();
        if (QFile::exists(dstFilePath))
            QFile::remove(dstFilePath);
        QFile(from  + "/" + it2.fileName()).copy(dstFilePath);
    }
}

void TestBlackbox::rmDirR(const QString &dir)
{
    QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot  | QDir::Hidden);
    while (it.hasNext()) {
        QFile(it.next()).remove();
    }

    QDirIterator it2(dir, QDir::Dirs | QDir::NoDotAndDotDot  | QDir::Hidden);
    while (it2.hasNext()) {
        rmDirR(it2.next());
    }
    QDir().rmdir(dir);
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

void TestBlackbox::initTestCase()
{
    QVERIFY(QFile::exists(qbsExecutableFilePath));
    QProcess process;
    process.start(qbsExecutableFilePath, QStringList() << "config" << "--global" << "--list");
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
}

void TestBlackbox::init()
{
    QVERIFY(testDataDir != testSourceDir);
    rmDirR(testDataDir);
    QDir().mkpath(testDataDir);
    ccp(testSourceDir, testDataDir);
}

void TestBlackbox::cleanup()
{
//        rmDirR(testDataDir);
}

void TestBlackbox::build_project_data()
{
    QTest::addColumn<QString>("projectSubDir");
    QTest::addColumn<QString>("productFileName");
    QTest::newRow("BPs in Sources")
            << QString("buildproperties_source")
            << QString(qbs::HostOsInfo::appendExecutableSuffix(buildDir + "/HelloWorld"));
    QTest::newRow("Qt5 plugin")
            << QString("qt5plugin")
            << QString(buildDir + QLatin1String("/") + qbs::HostOsInfo::dynamicLibraryName("echoplugin"));
    QTest::newRow("Q_OBJECT in source")
            << QString("moc_cpp")
            << QString(qbs::HostOsInfo::appendExecutableSuffix(buildDir + "/moc_cpp"));
    QTest::newRow("Q_OBJECT in header")
            << QString("moc_hpp")
            << QString(qbs::HostOsInfo::appendExecutableSuffix(buildDir + "/moc_hpp"));
    QTest::newRow("Q_OBJECT in header, moc_XXX.cpp included")
            << QString("moc_hpp_included")
            << QString(qbs::HostOsInfo::appendExecutableSuffix(buildDir + "/moc_hpp_included"));
}

void TestBlackbox::build_project()
{
    QFETCH(QString, projectSubDir);
    QFETCH(QString, productFileName);
    if (!projectSubDir.startsWith('/'))
        projectSubDir.prepend('/');
    QVERIFY2(QFile::exists(testDataDir + projectSubDir), qPrintable(testDataDir + projectSubDir));
    QDir::setCurrent(testDataDir + projectSubDir);

    QCOMPARE(runQbs(), 0);
    QVERIFY2(QFile::exists(productFileName), qPrintable(productFileName));
    QVERIFY2(QFile::remove(productFileName), qPrintable(productFileName));
    QCOMPARE(runQbs(), 0);
    QVERIFY2(QFile::exists(productFileName), qPrintable(productFileName));
}

void TestBlackbox::track_qrc()
{
    QDir::setCurrent(testDataDir + "/qrc");
    QCOMPARE(runQbs(), 0);
    const QString fileName = qbs::HostOsInfo::appendExecutableSuffix("build/" + buildProfile + "-debug/i");
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
    const QString productFilePath = qbs::HostOsInfo::appendExecutableSuffix(buildDir + "/i");
    QVERIFY2(QFile(productFilePath).exists(), qPrintable(productFilePath));
    QString moc_bla_objectFileName = buildDir + "/.obj/i/GeneratedFiles/i/moc_bla" + objectSuffix;
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
    QString unchangedObjectFile = buildDir + "/someapp/narf" + objectSuffix;
    QDateTime unchangedObjectFileTime1 = QFileInfo(unchangedObjectFile).lastModified();

    QTest::qWait(1000); // for file systems with low resolution timestamps
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
    QString unchangedObjectFile = buildDir + "/someapp/narf" + objectSuffix;
    QDateTime unchangedObjectFileTime1 = QFileInfo(unchangedObjectFile).lastModified();

    QTest::qWait(1000); // for file systems with low resolution timestamps
    QFile::remove("project.qbs");
    QFile::remove("main.cpp");
    ccp("../before/project.qbs", ".");
    ccp("../before/main.cpp", ".");
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
    QCOMPARE(QFile::exists(buildDir + "/someapp/zort" + objectSuffix), false);
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

    QTest::qWait(1000); // for file systems with low resolution timestamps
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
    QVERIFY2(QFile::exists(buildDir + "/.obj/someapp/main_foo" + objectSuffix),
            qPrintable(buildDir + "/.obj/someapp/main_foo" + objectSuffix));
    QVERIFY2(QFile::exists(buildDir + "/main_foo.cpp"), qPrintable(buildDir + "/main_foo.cpp"));
    QVERIFY2(QFile::exists(buildDir + "/main.foo"), qPrintable(buildDir + "/main.foo"));

    process.start(buildDir + "/someapp");
    QVERIFY(process.waitForStarted());
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's 15 foo here");

    QTest::qWait(1000); // for file systems with low resolution timestamps
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
    QCOMPARE(QFile::exists(buildDir + "/someapp/main_foo" + objectSuffix), false);
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

    QTest::qWait(1000); // for file systems with low resolution timestamps
    ccp("../after", ".");
    touch("main.cpp");
    QCOMPARE(runQbs(), 0);
}

QTEST_MAIN(TestBlackbox)
