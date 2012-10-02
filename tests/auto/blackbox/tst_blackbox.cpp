/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QtTest>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

class TestBlackbox : public QObject
{
    Q_OBJECT
    const QString testDataDir;
    const QString testSourceDir;
    const QString executableSuffix;
    const QString objectSuffix;
public:
    TestBlackbox()
        : testDataDir(QCoreApplication::applicationDirPath() + "/testdata"),
          testSourceDir(QDir::cleanPath(SRCDIR "/testdata")),
    #ifdef Q_OS_WIN
          executableSuffix(QLatin1String(".exe")),
    #else
          executableSuffix(QLatin1String("")),
    #endif
    #ifdef Q_OS_WIN
            objectSuffix(QLatin1String(".obj"))
    #else
            objectSuffix(QLatin1String(".o"))
    #endif
    {
    }

protected:

    int runQbs(QStringList arguments = QStringList(), bool showOutput = false)
    {
        QString cmdLine = QCoreApplication::applicationDirPath() + "/qbs";
        foreach (const QString &str, arguments)
            cmdLine += QLatin1String(" \"") + str + QLatin1Char('"');

        int r;
        if (showOutput) {
            r = system(cmdLine.toLatin1());
        } else {
            QProcess process;
            process.start(cmdLine);
            process.waitForStarted();
            process.waitForFinished();
            r = process.exitCode();
        }
        return r;
    }

    /*!
      Recursive copy from directory to another.
      Note that this updates the file stamps on Linux but not on Windows.
      */
    void ccp(const QString &from, const QString &to)
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

    void rmDirR(const QString &dir)
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

    void touch(const QString &fn)
    {
        QFile f(fn);
        int s = f.size();
        if (!f.open(QFile::ReadWrite))
            qFatal("cannot open file %s", qPrintable(fn));
        f.resize(s+1);
        f.resize(s);
    }

public slots:
    void init()
    {
        rmDirR(testDataDir);
        QDir().mkpath(testDataDir);
        ccp(testSourceDir, testDataDir);
    }

    void cleanup()
    {
//        rmDirR(testDataDir);
    }

private slots:
    void build_project_data()
    {
        QTest::addColumn<QString>("projectSubDir");
        QTest::addColumn<QString>("productFileName");
        QTest::newRow("BPs in Sources")
                << QString("buildproperties_source")
                << QString("build/debug/HelloWorld");
    }

    void build_project()
    {
        QFETCH(QString, projectSubDir);
        QFETCH(QString, productFileName);
        if (!projectSubDir.startsWith('/'))
            projectSubDir.prepend('/');
        QDir::setCurrent(testDataDir + projectSubDir);
        qDebug() << "currentdir set" << testDataDir + projectSubDir;
        qDebug() << "currentdir" << QDir::currentPath();
        productFileName.append(executableSuffix);
        QCOMPARE(runQbs(), 0);
        QVERIFY(QFile::exists(productFileName));
        QVERIFY(QFile::remove(productFileName));
        QCOMPARE(runQbs(), 0);
        QVERIFY(QFile::exists(productFileName));
    }

    void track_qrc()
    {
        QDir::setCurrent(testDataDir + "/qrc");
        QCOMPARE(runQbs(), 0);
        QString fileName = "build/debug/i" + executableSuffix;
        QVERIFY(QFile(fileName).exists());
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

    void track_qobject_change()
    {
        QDir::setCurrent(testDataDir + "/moc_h");
        QFile("bla.h").remove();
        QVERIFY(QFile("bla_qobject.h").copy("bla.h"));
        touch("bla.h");
        QCOMPARE(runQbs(), 0);
        QVERIFY(QFile("build/debug/i" + executableSuffix).exists());
        QString moc_bla_objectFileName = "build/debug/.obj/i/GeneratedFiles/i/moc_bla" + objectSuffix;
        QVERIFY(QFile(moc_bla_objectFileName).exists());

        QTest::qSleep(1000);
        QFile("bla.h").remove();
        QVERIFY(QFile("bla_noqobject.h").copy("bla.h"));
        touch("bla.h");
        QCOMPARE(runQbs(), 0);
        QVERIFY(QFile("build/debug/i" + executableSuffix).exists());
        QVERIFY(!QFile(moc_bla_objectFileName).exists());
    }

    void trackAddFile()
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

        process.start("build/debug/someapp");
        QVERIFY(process.waitForStarted());
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitCode(), 0);
        output = process.readAllStandardOutput().split('\n');
        QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
        QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");
        QString unchangedObjectFile = "build/debug/someapp/narf" + objectSuffix;
        QDateTime unchangedObjectFileTime1 = QFileInfo(unchangedObjectFile).lastModified();

        QTest::qWait(1000); // for file systems with low resolution timestamps
        ccp("../after", ".");
        touch("project.qbp");
        touch("main.cpp");
        QCOMPARE(runQbs(), 0);

        process.start("build/debug/someapp");
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

    void trackRemoveFile()
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

        process.start("build/debug/someapp");
        QVERIFY(process.waitForStarted());
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitCode(), 0);
        output = process.readAllStandardOutput().split('\n');
        QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
        QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");
        QCOMPARE(output.takeFirst().trimmed().constData(), "ZORT!");
        QString unchangedObjectFile = "build/debug/someapp/narf" + objectSuffix;
        QDateTime unchangedObjectFileTime1 = QFileInfo(unchangedObjectFile).lastModified();

        QTest::qWait(1000); // for file systems with low resolution timestamps
        QFile::remove("project.qbp");
        QFile::remove("main.cpp");
        ccp("../before/project.qbp", ".");
        ccp("../before/main.cpp", ".");
        QFile::copy("../before/project.qbp", "project.qbp");
        QFile::copy("../before/main.cpp", "main.cpp");
        QVERIFY(QFile::remove("zort.h"));
        QVERIFY(QFile::remove("zort.cpp"));
        touch("main.cpp");
        touch("project.qbp");
        QCOMPARE(runQbs(), 0);

        process.start("build/debug/someapp");
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
        QCOMPARE(QFile::exists("build/debug/someapp/zort" + objectSuffix), false);
    }

    void trackAddFileTag()
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

        process.start("build/debug/someapp");
        QVERIFY(process.waitForStarted());
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitCode(), 0);
        output = process.readAllStandardOutput().split('\n');
        QCOMPARE(output.takeFirst().trimmed().constData(), "there's no foo here");

        QTest::qWait(1000); // for file systems with low resolution timestamps
        ccp("../after", ".");
        touch("main.cpp");
        touch("project.qbp");
        QCOMPARE(runQbs(), 0);

        process.start("build/debug/someapp");
        QVERIFY(process.waitForStarted());
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitCode(), 0);
        output = process.readAllStandardOutput().split('\n');
        QCOMPARE(output.takeFirst().trimmed().constData(), "there's 15 foo here");
    }

    void trackRemoveFileTag()
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
        QCOMPARE(QFile::exists("build/debug/.obj/someapp/main_foo" + objectSuffix), true);
        QCOMPARE(QFile::exists("build/debug/main_foo.cpp"), true);
        QCOMPARE(QFile::exists("build/debug/main.foo"), true);

        process.start("build/debug/someapp");
        QVERIFY(process.waitForStarted());
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitCode(), 0);
        output = process.readAllStandardOutput().split('\n');
        QCOMPARE(output.takeFirst().trimmed().constData(), "there's 15 foo here");

        QTest::qWait(1000); // for file systems with low resolution timestamps
        ccp("../before", ".");
        touch("main.cpp");
        touch("project.qbp");
        QCOMPARE(runQbs(), 0);

        process.start("build/debug/someapp");
        QVERIFY(process.waitForStarted());
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitCode(), 0);
        output = process.readAllStandardOutput().split('\n');
        QCOMPARE(output.takeFirst().trimmed().constData(), "there's no foo here");

        // check if stale artifacts have been removed
        QCOMPARE(QFile::exists("build/debug/someapp/main_foo" + objectSuffix), false);
        QCOMPARE(QFile::exists("build/debug/someapp/main_foo.cpp"), false);
        QCOMPARE(QFile::exists("build/debug/someapp/main.foo"), false);
    }

    void trackAddMocInclude()
    {
        QDir::setCurrent(testDataDir + "/trackAddMocInclude");
        if (QFile::exists("work"))
            rmDirR("work");
        QDir().mkdir("work");
        ccp("before", "work");
        QDir::setCurrent(testDataDir + "/trackAddMocInclude/work");
        // The build must fail because the main.moc include is missing.
        QVERIFY(runQbs() != 0);

        QTest::qWait(1000); // for file systems with low resolution timestamps
        ccp("../after", ".");
        touch("main.cpp");
        QCOMPARE(runQbs(), 0);
    }
};

QTEST_MAIN(TestBlackbox)

#include "tst_blackbox.moc"
