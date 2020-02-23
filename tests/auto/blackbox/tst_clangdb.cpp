/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Christian Gagneraud.
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

#include "tst_clangdb.h"

#include "../shared.h"

#include <tools/hostosinfo.h>
#include <tools/installoptions.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qregexp.h>

#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>

#include <QtTest/qtest.h>

using qbs::InstallOptions;
using qbs::Internal::HostOsInfo;

int TestClangDb::runProcess(const QString &exec, const QStringList &args, QByteArray &stdErr,
                            QByteArray &stdOut)
{
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(processEnvironment);
    process.setProcessEnvironment(env);

    process.start(exec, args);
    const int waitTime = 10 * 60000;
    if (!process.waitForStarted() || !process.waitForFinished(waitTime)) {
        stdErr = process.readAllStandardError();
        return -1;
    }

    stdErr = process.readAllStandardError();
    stdOut = process.readAllStandardOutput();
    sanitizeOutput(&stdErr);
    sanitizeOutput(&stdOut);

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (!stdErr.isEmpty())
            qDebug("%s", stdErr.constData());
        if (!stdOut.isEmpty())
            qDebug("%s", stdOut.constData());
    }

    return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
}

qbs::Version TestClangDb::clangVersion()
{
    QByteArray stdErr;
    QByteArray stdOut;
    if (runProcess("clang-check", QStringList("--version"), stdErr, stdOut) != 0)
        return qbs::Version();
    stdOut.remove(0, stdOut.indexOf("LLVM version ") + 13);
    stdOut.truncate(stdOut.indexOf('\n'));
    return qbs::Version::fromString(QString::fromLocal8Bit(stdOut));
}


TestClangDb::TestClangDb() : TestBlackboxBase(SRCDIR "/testdata-clangdb", "blackbox-clangdb"),
    projectDir(QDir::cleanPath(testDataDir + "/project1")),
    projectFileName("project.qbs"),
    buildDir(QDir::cleanPath(projectDir + "/" + relativeBuildDir())),
    sourceFilePath(QDir::cleanPath(projectDir + "/i like spaces.cpp")),
    dbFilePath(QDir::cleanPath(buildDir + "/compile_commands.json"))
{
}

void TestClangDb::initTestCase()
{
    TestBlackboxBase::initTestCase();
    QDir::setCurrent(projectDir);
}

void TestClangDb::ensureBuildTreeCreated()
{
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFile::exists(buildDir));

    if (m_qbsStdout.contains("is msvc") || m_qbsStdout.contains("is mingw")) {
        sanitizeOutput(&m_qbsStdout);
        const auto lines = m_qbsStdout.split('\n');
        for (const auto &line : lines) {
            static const QByteArray includeEnv = "INCLUDE=";
            static const QByteArray libEnv = "LIB=";
            static const QByteArray pathEnv = "PATH=";
            if (line.startsWith(includeEnv))
                processEnvironment.insert("INCLUDE", line.mid(includeEnv.size()));
            if (line.startsWith(libEnv))
                processEnvironment.insert("LIB", line.mid(libEnv.size()));
            if (line.startsWith(pathEnv))
                processEnvironment.insert("PATH", line.mid(pathEnv.size()));
        }
    }
}

void TestClangDb::checkCanGenerateDb()
{
    QbsRunParameters params;
    params.command = "generate";
    params.arguments << "--generator" << "clangdb";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(QFile::exists(dbFilePath));
}

void TestClangDb::checkDbIsValidJson()
{
    QFile file(dbFilePath);
    QVERIFY(file.open(QFile::ReadOnly));
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QVERIFY(!doc.isNull());
    QVERIFY(doc.isArray());
}

void TestClangDb::checkDbIsConsistentWithProject()
{
    QFile file(dbFilePath);
    QVERIFY(file.open(QFile::ReadOnly));
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());

    // We expect only one command for now
    const QJsonArray array = doc.array();
    QVERIFY(array.size() == 1);

    // Validate the "command object"
    const QJsonObject entry = array.at(0).toObject();
    QVERIFY(entry.contains("directory"));
    QVERIFY(entry.value("directory").isString());
    QVERIFY(entry.contains("arguments"));
    QVERIFY(entry.value("arguments").isArray());
    QVERIFY(entry.value("arguments").toArray().size() >= 2);
    QVERIFY(entry.contains("file"));
    QVERIFY(entry.value("file").isString());
    QVERIFY(entry.value("file").toString() == sourceFilePath);

    // Validate the compile command itself, this requires a previous build since the command
    // line contains 'deep' path that are created during Qbs build run
    QByteArray stdErr;
    QByteArray stdOut;
    QStringList arguments;
    const QJsonArray jsonArguments = entry.value("arguments").toArray();
    QString executable = jsonArguments.at(0).toString();
    for (int i=1; i<jsonArguments.size(); i++)
        arguments.push_back(jsonArguments.at(i).toString());
    QVERIFY(runProcess(executable, arguments, stdErr, stdOut) == 0);
}

// Run clang-check, should give 2 warnings:
// <...>/i like spaces.cpp:11:5: warning: Assigned value is garbage or undefined
//     int unused = garbage;
//     ^~~~~~~~~~   ~~~~~~~
// <...>/i like spaces.cpp:11:9: warning: Value stored to 'unused' during its initialization is never read
//     int unused = garbage;
//         ^~~~~~   ~~~~~~~
// 2 warnings generated.
void TestClangDb::checkClangDetectsSourceCodeProblems()
{
    QByteArray stdErr;
    QByteArray stdOut;
    QStringList arguments;
    const QString executable = findExecutable(QStringList("clang-check"));
    if (executable.isEmpty())
        QSKIP("No working clang-check executable found");

    // Older clang versions do not support the "arguments" array in the compilation database.
    // Should we really want to support them, we would have to fall back to "command" instead.
    if (clangVersion() < qbs::Version(3, 7))
        QSKIP("This test requires clang-check to be based on at least LLVM 3.7.0.");

    // clang-check.exe does not understand MSVC command-line syntax
    const SettingsPtr s = settings();
    qbs::Profile profile(profileName(), s.get());
    if (profileToolchain(profile).contains("msvc")) {
        arguments << "-extra-arg-before=--driver-mode=cl";
    } else if (profileToolchain(profile).contains("mingw")) {
        arguments << "-extra-arg-before=--driver-mode=g++";
    }

    arguments << "-analyze" << "-p" << relativeBuildDir() << sourceFilePath;
    QVERIFY(runProcess(executable, arguments, stdErr, stdOut) == 0);
    const QString output = QString::fromLocal8Bit(stdErr);
    QVERIFY(output.contains(QRegExp(QStringLiteral("warning.*undefined"), Qt::CaseInsensitive)));
    QVERIFY(output.contains(QRegExp(QStringLiteral("warning.*never read"), Qt::CaseInsensitive)));
}

QTEST_MAIN(TestClangDb)
