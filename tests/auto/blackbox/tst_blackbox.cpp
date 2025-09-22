/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2019 Jochen Ulrich <jochenulrich@t-online.de>
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

#include "tst_blackbox.h"

#include "../shared.h"

#include <api/languageinfo.h>
#include <tools/hostosinfo.h>
#include <tools/installoptions.h>
#include <tools/preferences.h>
#include <tools/profile.h>
#include <tools/qttools.h>
#include <tools/settings.h>
#include <tools/shellutils.h>
#include <tools/stlutils.h>
#include <tools/version.h>

#include <QtCore/qdebug.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qlocale.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qsettings.h>
#include <QtCore/qtemporarydir.h>
#include <QtCore/qtemporaryfile.h>
#include <QtCore/qversionnumber.h>

#include <algorithm>
#include <functional>
#include <regex>
#include <utility>

#define WAIT_FOR_NEW_TIMESTAMP() waitForNewTimestamp(testDataDir)

using qbs::Internal::HostOsInfo;

class MacosTarHealer {
public:
    MacosTarHealer() {
        if (HostOsInfo::hostOs() == HostOsInfo::HostOsMacos) {
            // work around absurd tar behavior on macOS
            qputenv("COPY_EXTENDED_ATTRIBUTES_DISABLE", "true");
            qputenv("COPYFILE_DISABLE", "true");
        }
    }

    ~MacosTarHealer() {
        if (HostOsInfo::hostOs() == HostOsInfo::HostOsMacos) {
            qunsetenv("COPY_EXTENDED_ATTRIBUTES_DISABLE");
            qunsetenv("COPYFILE_DISABLE");
        }
    }
};

QMap<QString, QString> TestBlackbox::findCli(int *status)
{
    QTemporaryDir temp;
    QDir::setCurrent(testDataDir + "/find");
    QbsRunParameters params = QStringList() << "-f" << "find-cli.qbs";
    params.buildDirectory = temp.path();
    const int res = runQbs(params);
    if (status)
        *status = res;
    QFile file(temp.path() + "/" + relativeProductBuildDir("find-cli") + "/cli.json");
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const auto tools = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    return {{"path", QDir::fromNativeSeparators(tools["path"].toString())}};
}

QMap<QString, QString> TestBlackbox::findNodejs(int *status)
{
    QTemporaryDir temp;
    QDir::setCurrent(testDataDir + "/find");
    QbsRunParameters params = QStringList() << "-f" << "find-nodejs.qbs";
    params.buildDirectory = temp.path();
    const int res = runQbs(params);
    if (status)
        *status = res;
    QFile file(temp.path() + "/" + relativeProductBuildDir("find-nodejs") + "/nodejs.json");
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const auto tools = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    return {{"node", QDir::fromNativeSeparators(tools["node"].toString())}};
}

QMap<QString, QString> TestBlackbox::findTypeScript(int *status)
{
    QTemporaryDir temp;
    QDir::setCurrent(testDataDir + "/find");
    QbsRunParameters params = QStringList() << "-f" << "find-typescript.qbs";
    params.buildDirectory = temp.path();
    const int res = runQbs(params);
    if (status)
        *status = res;
    QFile file(temp.path() + "/" + relativeProductBuildDir("find-typescript") + "/typescript.json");
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const auto tools = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    return {{"tsc", QDir::fromNativeSeparators(tools["tsc"].toString())}};
}

QString TestBlackbox::findArchiver(const QString &fileName, int *status)
{
    if (fileName == "jar")
        return findJdkTools(status).value(fileName);

    QString binary = findExecutable(QStringList(fileName));
    if (binary.isEmpty()) {
        const SettingsPtr s = settings();
        qbs::Profile p(profileName(), s.get());
        binary = findExecutable(p.value("archiver.command").toStringList());
    }
    return binary;
}

bool TestBlackbox::prepareAndRunConan()
{
    QString executable = findExecutable({"conan"});
    if (executable.isEmpty()) {
        qInfo() << "conan is not installed or not available in PATH.";
        return false;
    }

    const auto conanVersion = this->conanVersion(executable);
    if (!conanVersion.isValid()) {
        qInfo() << "Can't get conan version.";
        return false;
    }
    if (compare(conanVersion, qbs::Version(2, 6)) < 0) {
        qInfo() << "This test apples only to conan 2.6 and newer.";
        return false;
    }

    const auto profilePath = QDir::homePath() + "/.conan2/profiles/qbs-test-libs";
    if (!QFileInfo(profilePath).exists()) {
        qInfo() << "conan profile is not installed, run './scripts/setup-conan-profiles.sh'";
        return false;
    }
    QProcess conan;
    QDir::setCurrent(testDataDir + "/conan-provider/testlibdep");
    rmDirR("build");
    QStringList arguments{"install", ".", "--profile:all=qbs-test-libs", "--output-folder=build"};
    conan.start(executable, arguments);
    return waitForProcessSuccess(conan, 60000);
}

bool TestBlackbox::lexYaccExist()
{
    return !findExecutable(QStringList("lex")).isEmpty()
            && !findExecutable(QStringList("yacc")).isEmpty();
}

qbs::Version TestBlackbox::bisonVersion()
{
    const auto yaccBinary = findExecutable(QStringList("yacc"));
    QProcess process;
    process.start(yaccBinary, QStringList() << "--version");
    if (!process.waitForStarted())
        return qbs::Version();
    if (!process.waitForFinished())
        return qbs::Version();
    const auto processStdOut = process.readAllStandardOutput();
    if (processStdOut.isEmpty())
        return qbs::Version();
    const auto line = processStdOut.split('\n')[0];
    const auto words = line.split(' ');
    if (words.empty())
        return qbs::Version();
    return qbs::Version::fromString(words.last());
}

QMap<QString, QByteArray> TestBlackbox::getRepoStateFromApp() const
{
    QMap<QString, QByteArray> result;
    const int startIndex = m_qbsStdout.indexOf("==");
    if (startIndex == -1)
        return result;
    const int endIndex = m_qbsStdout.indexOf("==", startIndex + 2);
    if (endIndex == -1)
        return result;
    const QByteArray data = m_qbsStdout.mid(startIndex + 2, endIndex - startIndex - 2);
    const QList<QByteArray> entries = data.split(';');
    for (const QByteArray &entry : entries) {
        const int eqPos = entry.indexOf('=');
        if (eqPos != -1)
            result.insert(QString::fromLatin1(entry.left(eqPos)), entry.mid(eqPos + 1));
    }
    return result;
}

void TestBlackbox::sevenZip()
{
    QDir::setCurrent(testDataDir + "/archiver");
    QString binary = findArchiver("7z");
    if (binary.isEmpty())
        QSKIP("7zip not found");
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "modules.archiver.type:7zip")), 0);
    const QString outputFile = relativeProductBuildDir("archivable") + "/archivable.7z";
    QVERIFY2(regularFileExists(outputFile), qPrintable(outputFile));
    QProcess listContents;
    listContents.start(binary, QStringList() << "l" << outputFile);
    QVERIFY2(listContents.waitForStarted(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.waitForFinished(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.exitCode() == 0, listContents.readAllStandardError().constData());
    const QByteArray output = listContents.readAllStandardOutput();
    QVERIFY2(output.contains("2 files"), output.constData());
    QVERIFY2(output.contains("test.txt"), output.constData());
    QVERIFY2(output.contains("archivable.qbs"), output.constData());
}

void TestBlackbox::sourceArtifactInInputsFromDependencies()
{
    QDir::setCurrent(testDataDir + "/source-artifact-in-inputs-from-dependencies");
    QCOMPARE(runQbs(), 0);
    QFile outFile(relativeProductBuildDir("p") + "/output.txt");
    QVERIFY2(outFile.exists(), qPrintable(outFile.fileName()));
    QVERIFY2(outFile.open(QIODevice::ReadOnly), qPrintable(outFile.errorString()));
    const QByteArrayList output = outFile.readAll().trimmed().split('\n');
    QCOMPARE(output.size(), 2);
    bool header1Found = false;
    bool header2Found = false;
    for (const QByteArray &line : output) {
        const QByteArray &path = line.trimmed();
        if (path == "include1/header.h")
            header1Found = true;
        else if (path == "include2/header.h")
            header2Found = true;
    }
    QVERIFY(header1Found);
    QVERIFY(header2Found);
}

void TestBlackbox::staticLibWithoutSources()
{
    QDir::setCurrent(testDataDir + "/static-lib-without-sources");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::suspiciousCalls()
{
    const QString projectDir = testDataDir + "/suspicious-calls";
    QDir::setCurrent(projectDir);
    rmDirR(relativeBuildDir());
    QFETCH(QString, projectFile);
    QbsRunParameters params(QStringList() << "-f" << projectFile);
    QCOMPARE(runQbs(params), 0);
    QFETCH(QByteArray, expectedWarning);
    QVERIFY2(m_qbsStderr.contains(expectedWarning), m_qbsStderr.constData());
}

void TestBlackbox::suspiciousCalls_data()
{
    QTest::addColumn<QString>("projectFile");
    QTest::addColumn<QByteArray>("expectedWarning");
    QTest::newRow("File.copy() in Probe") << "copy-probe.qbs" << QByteArray();
    QTest::newRow("File.copy() during evaluation") << "copy-eval.qbs" << QByteArray("File.copy()");
    QTest::newRow("File.copy() in prepare script")
            << "copy-prepare.qbs" << QByteArray("File.copy()");
    QTest::newRow("File.copy() in command") << "copy-command.qbs" << QByteArray();
    QTest::newRow("File.directoryEntries() in Probe") << "direntries-probe.qbs" << QByteArray();
    QTest::newRow("File.directoryEntries() during evaluation")
            << "direntries-eval.qbs" << QByteArray("File.directoryEntries()");
    QTest::newRow("File.directoryEntries() in prepare script")
            << "direntries-prepare.qbs" << QByteArray();
    QTest::newRow("File.directoryEntries() in command") << "direntries-command.qbs" << QByteArray();
}

void TestBlackbox::systemIncludePaths()
{
    const QString projectDir = testDataDir + "/system-include-paths";
    QDir::setCurrent(projectDir);
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::distributionIncludePaths()
{
    const QString projectDir = testDataDir + "/distribution-include-paths";
    QDir::setCurrent(projectDir);
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::tar()
{
    if (HostOsInfo::hostOs() == HostOsInfo::HostOsWindows)
        QSKIP("Beware of the msys tar");
    MacosTarHealer tarHealer;
    QDir::setCurrent(testDataDir + "/archiver");
    rmDirR(relativeBuildDir());
    QString binary = findArchiver("tar");
    if (binary.isEmpty())
        QSKIP("tar not found");
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "modules.archiver.type:tar")), 0);
    const QString outputFile = relativeProductBuildDir("archivable") + "/archivable.tar.gz";
    QVERIFY2(regularFileExists(outputFile), qPrintable(outputFile));
    QProcess listContents;
    listContents.start(binary, QStringList() << "tf" << outputFile);
    QVERIFY2(listContents.waitForStarted(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.waitForFinished(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.exitCode() == 0, listContents.readAllStandardError().constData());
    QFile listFile("list.txt");
    QVERIFY2(listFile.open(QIODevice::ReadOnly), qPrintable(listFile.errorString()));
    QCOMPARE(listContents.readAllStandardOutput(), listFile.readAll());
}

void TestBlackbox::textTemplate()
{
    QVERIFY(QDir::setCurrent(testDataDir + "/texttemplate"));
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(), 0);
    QString outputFilePath = relativeProductBuildDir("one") + "/output.txt";
    QString expectedOutputFilePath = QFINDTESTDATA("expected/output.txt");
    TEXT_FILE_COMPARE(outputFilePath, expectedOutputFilePath);
    outputFilePath = relativeProductBuildDir("one") + "/lalala.txt";
    expectedOutputFilePath = QFINDTESTDATA("expected/lalala.txt");
    TEXT_FILE_COMPARE(outputFilePath, expectedOutputFilePath);
    // Test @var@ syntax
    outputFilePath = relativeProductBuildDir("one") + "/output_at.txt";
    expectedOutputFilePath = QFINDTESTDATA("expected/output_at.txt");
    TEXT_FILE_COMPARE(outputFilePath, expectedOutputFilePath);
}

static QStringList sortedFileList(const QByteArray &ba)
{
    auto list = QString::fromUtf8(ba).split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts);
    std::sort(list.begin(), list.end());
    return list;
}

void TestBlackbox::zip()
{
    QFETCH(QString, binaryName);
    int status = 0;
    const QString binary = findArchiver(binaryName, &status);
    QCOMPARE(status, 0);
    if (binary.isEmpty())
        QSKIP("zip tool not found");

    QDir::setCurrent(testDataDir + "/archiver");
    rmDirR(relativeBuildDir());
    QbsRunParameters params(QStringList()
                            << "modules.archiver.type:zip" << "modules.archiver.command:" + binary);
    QCOMPARE(runQbs(params), 0);
    const QString outputFile = relativeProductBuildDir("archivable") + "/archivable.zip";
    QVERIFY2(regularFileExists(outputFile), qPrintable(outputFile));
    QProcess listContents;
    if (binaryName == "zip") {
        // zipinfo is part of Info-Zip
        listContents.start("zipinfo", QStringList() << "-1" << outputFile);
    } else {
        listContents.start(binary, QStringList() << "tf" << outputFile);
    }
    QVERIFY2(listContents.waitForStarted(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.waitForFinished(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.exitCode() == 0, listContents.readAllStandardError().constData());
    QFile listFile("list.txt");
    QVERIFY2(listFile.open(QIODevice::ReadOnly), qPrintable(listFile.errorString()));
    QCOMPARE(sortedFileList(listContents.readAllStandardOutput()),
             sortedFileList(listFile.readAll()));

    // Make sure the module is still loaded when the java/jar fallback is not available
    params.command = "resolve";
    params.arguments << "modules.java.jdkPath:/blubb";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::zip_data()
{
    QTest::addColumn<QString>("binaryName");
    QTest::newRow("zip") << "zip";
    QTest::newRow("jar") << "jar";
}

void TestBlackbox::zipInvalid()
{
    QDir::setCurrent(testDataDir + "/archiver");
    rmDirR(relativeBuildDir());
    QbsRunParameters params(QStringList() << "modules.archiver.type:zip"
                            << "modules.archiver.command:/bin/something");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("unrecognized archive tool: 'something'"), m_qbsStderr.constData());
}

TestBlackbox::TestBlackbox() : TestBlackboxBase (SRCDIR "/testdata", "blackbox")
{
}

void TestBlackbox::allowedValues()
{
    QFETCH(QString, property);
    QFETCH(QString, value);
    QFETCH(QString, invalidValue);

    QDir::setCurrent(testDataDir + "/allowed-values");
    rmDirR(relativeBuildDir());

    QbsRunParameters params;
    if (!property.isEmpty() && !value.isEmpty()) {
        params.arguments << QStringLiteral("%1:%2").arg(property, value);
    }

    params.expectFailure = !invalidValue.isEmpty();
    QCOMPARE(runQbs(params) == 0, !params.expectFailure);
    if (params.expectFailure) {
        const auto errorString =
                QStringLiteral("Value '%1' is not allowed for property").arg(invalidValue);
        QVERIFY2(m_qbsStderr.contains(errorString.toUtf8()), m_qbsStderr.constData());
    }
}

void TestBlackbox::allowedValues_data()
{
    QTest::addColumn<QString>("property");
    QTest::addColumn<QString>("value");
    QTest::addColumn<QString>("invalidValue");

    QTest::newRow("default") << QString() << QString() << QString();

    QTest::newRow("allowed (product, CLI)") << "products.p.prop" << "foo" << QString();
    QTest::newRow("not allowed (product, CLI)") << "products.p.prop" << "bar" << "bar";
    QTest::newRow("allowed (product, JS)") << "products.p.prop2" << "foo" << QString();
    QTest::newRow("not allowed (product, JS)") << "products.p.prop2" << "bar" << "bar";

    QTest::newRow("allowed single (module, CLI)") << "modules.a.prop" << "foo" << QString();
    QTest::newRow("not allowed single (module, CLI)") << "modules.a.prop" << "baz" << "baz";
    QTest::newRow("allowed mult (module, CLI)") << "modules.a.prop" << "foo,bar" << QString();
    QTest::newRow("not allowed mult (module, CLI)") << "modules.a.prop" << "foo,baz" << "baz";

    QTest::newRow("allowed single (module, JS)") << "modules.a.prop2" << "foo" << QString();
    QTest::newRow("not allowed single (module, JS)") << "modules.a.prop2" << "baz" << "baz";
    QTest::newRow("allowed mult (module, JS)") << "modules.a.prop2" << "foo,bar" << QString();
    QTest::newRow("not allowed mult (module, JS)") << "modules.a.prop2" << "foo,baz" << "baz";

    // undefined should always be allowed
    QTest::newRow("undefined (product)") << "products.p.prop" << "undefined" << QString();
    QTest::newRow("undefined (module)") << "modules.a.prop" << "undefined" << QString();
}

void TestBlackbox::addFileTagToGeneratedArtifact()
{
    QDir::setCurrent(testDataDir + "/add-filetag-to-generated-artifact");
    QCOMPARE(runQbs(QStringList("project.enableTagging:true")), 0);
    QVERIFY2(m_qbsStdout.contains("compressing my_app"), m_qbsStdout.constData());
    const QString compressedAppFilePath = relativeProductBuildDir("my_compressed_app") + '/'
                                          + appendExecSuffix("compressed-my_app", m_qbsStdout);
    QVERIFY2(regularFileExists(compressedAppFilePath), qPrintable(compressedAppFilePath));
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("project.enableTagging:false"))), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(!regularFileExists(compressedAppFilePath));
}

void TestBlackbox::alwaysRun()
{
    QFETCH(QString, projectFile);

    QDir::setCurrent(testDataDir + "/always-run");
    rmDirR(relativeBuildDir());
    QbsRunParameters params("build", QStringList() << "-f" << projectFile);
    if (projectFile.contains("transformer")) {
        params.expectFailure = true;
        QVERIFY(runQbs(params) != 0);
        QVERIFY2(m_qbsStderr.contains("removed"), m_qbsStderr.constData());
        return;
    }
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("yo"));
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("yo"));
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFile, "alwaysRun: false", "alwaysRun: true");

    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("yo"));
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("yo"));
}

void TestBlackbox::alwaysRun_data()
{
    QTest::addColumn<QString>("projectFile");
    QTest::newRow("Transformer") << "transformer.qbs";
    QTest::newRow("Rule") << "rule.qbs";
}

void TestBlackbox::archiverFlags()
{
    QDir::setCurrent(testDataDir + "/archiver-flags");
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("-n"))), 0);
    if (!m_qbsStdout.contains("toolchain is MSVC"))
        QSKIP("Test applies to MSVC toolchain only");
    QCOMPARE(runQbs(QStringList({"-n", "--command-echo-mode", "command-line"})), 0);
    const QByteArrayList output = m_qbsStdout.split('\n');
    QByteArray archiveLine;
    for (const QByteArray &line : output) {
        if (line.contains("lib.exe") && line.contains(".lib")) {
            archiveLine = line;
            break;
        }
    }
    QVERIFY(!archiveLine.isEmpty());
    QVERIFY2(archiveLine.contains("/WX"), archiveLine.constData());
}

void TestBlackbox::artifactsMapChangeTracking()
{
    QDir::setCurrent(testDataDir + "/artifacts-map-change-tracking");
    QCOMPARE(runQbs(QStringList{"-p", "TheApp"}), 0);
    QVERIFY2(m_qbsStdout.contains("running rule for test.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("creating test.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("linking"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QStringList{"-p", "meta,p"}), 0);
    QVERIFY2(m_qbsStdout.contains("printing artifacts"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("test.txt"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("main.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("test.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("TheBinary"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("dummy1"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("dummy2"), m_qbsStdout.constData());

    // Change name of target binary. Command must be re-run, because the file name of an
    // artifact changed.
    WAIT_FOR_NEW_TIMESTAMP();
    const QString projectFile("artifacts-map-change-tracking.qbs");
    REPLACE_IN_FILE(projectFile, "TheBinary", "TheNewBinary");
    QCOMPARE(runQbs(QStringList{"-p", "TheApp"}), 0);

    QVERIFY2(!m_qbsStdout.contains("running rule for test.cpp"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("creating test.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("linking"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QStringList{"-p", "meta,p"}), 0);
    QVERIFY2(m_qbsStdout.contains("printing artifacts"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("test.txt"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("main.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("test.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("TheNewBinary"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("TheBinary"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("dummy1"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("dummy2"), m_qbsStdout.constData());

    // Add file tag to generated artifact. Command must be re-run, because it enumerates the keys
    // of the artifacts map.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFile, "fileTags: 'cpp'", "fileTags: ['cpp', 'blubb']");
    QCOMPARE(runQbs(QStringList{"-p", "TheApp"}), 0);
    QVERIFY2(m_qbsStdout.contains("running rule for test.cpp"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("creating test.cpp"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("linking"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QStringList{"-p", "meta,p"}), 0);
    QVERIFY2(m_qbsStdout.contains("printing artifacts"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("test.txt"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("main.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("test.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("TheNewBinary"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("dummy1"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("dummy2"), m_qbsStdout.constData());

    // Add redundant file tag to generated artifact. Command must not be re-run, because
    // the artifacts map has not changed.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFile, "fileTags: ['cpp', 'blubb']",
                    "fileTags: ['cpp', 'blubb', 'blubb']");
    QCOMPARE(runQbs(QStringList{"-p", "TheApp"}), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running rule for test.cpp"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("creating test.cpp"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("linking"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QStringList{"-p", "meta,p"}), 0);
    QVERIFY2(!m_qbsStdout.contains("printing artifacts"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("dummy1"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("dummy2"), m_qbsStdout.constData());

    // Rebuild the app. Command must not be re-run, because the artifacts map has not changed.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("main.cpp");
    QCOMPARE(runQbs(QStringList{"-p", "TheApp"}), 0);
    QVERIFY2(!m_qbsStdout.contains("running rule for test.cpp"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("creating test.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("linking"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QStringList{"-p", "meta,p"}), 0);
    QVERIFY2(!m_qbsStdout.contains("printing artifacts"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("dummy1"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("dummy2"), m_qbsStdout.constData());

    // Add source file to app. Command must be re-run, because the artifacts map has changed.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFile, "/* 'test.txt' */", "'test.txt'");
    QCOMPARE(runQbs(QStringList{"-p", "TheApp"}), 0);
    QEXPECT_FAIL("", "change tracking could become even more fine-grained", Continue);
    QVERIFY2(!m_qbsStdout.contains("running rule for test.cpp"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("creating test.cpp"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("linking"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QStringList{"-p", "meta,p"}), 0);
    QVERIFY2(m_qbsStdout.contains("printing artifacts"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("test.txt"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("main.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("test.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("TheNewBinary"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("dummy1"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("dummy2"), m_qbsStdout.constData());
}

void TestBlackbox::artifactsMapInvalidation()
{
    const QString projectDir = testDataDir + "/artifacts-map-invalidation";
    QDir::setCurrent(projectDir);
    QCOMPARE(runQbs(), 0);
    TEXT_FILE_COMPARE(relativeProductBuildDir("p") + "/myfile.out", "file.in");
}

void TestBlackbox::artifactsMapRaceCondition()
{
    QDir::setCurrent(testDataDir + "/artifacts-map-race-condition");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::artifactScanning()
{
    const QString projectDir = testDataDir + "/artifact-scanning";
    QDir::setCurrent(projectDir);
    QbsRunParameters params(QStringList("-vv"));

    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"external1.h\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("p1.cpp");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external1.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("p2.cpp");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external1.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("p3.cpp");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external1.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("shared.h");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"external1.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("external1.h");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external1.h\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("subdir/external2.h");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external1.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("external-indirect.h");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external1.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("p1.cpp");
    params.command = "resolve";
    params.arguments << "modules.cpp.treatSystemHeadersAsDependencies:true";
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p1.cpp\""), 1);
    QCOMPARE(m_qbsStderr.count("scanning \"p2.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"p3.cpp\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"shared.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external1.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external2.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"external-indirect.h\""), 0);
    QCOMPARE(m_qbsStderr.count("scanning \"iostream\""), 1);
}

void TestBlackbox::buildDirectories()
{
    const QString projectDir
            = QDir::cleanPath(testDataDir + QLatin1String("/build-directories"));
    const QString projectBuildDir = projectDir + '/' + relativeBuildDir();
    QDir::setCurrent(projectDir);
    QCOMPARE(runQbs(), 0);
    const QStringList outputLines
            = QString::fromLocal8Bit(m_qbsStdout.trimmed()).split('\n', Qt::SkipEmptyParts);
    QVERIFY2(outputLines.contains(projectDir + '/' + relativeProductBuildDir("p1")),
             m_qbsStdout.constData());
    QVERIFY2(outputLines.contains(projectDir + '/' + relativeProductBuildDir("p2")),
             m_qbsStdout.constData());
    QVERIFY2(outputLines.contains(projectBuildDir), m_qbsStdout.constData());
    QVERIFY2(outputLines.contains(projectDir), m_qbsStdout.constData());
}

void TestBlackbox::buildDirPlaceholders_data()
{
    QTest::addColumn<QString>("buildDir");
    QTest::addColumn<bool>("setProjectFile");
    QTest::addColumn<bool>("successExpected");

    QTest::newRow("normal dir, with project file") << "somedir" << true << true;
    QTest::newRow("normal dir, without project file") << "somedir" << false << true;
    QTest::newRow("@project, with project file") << "somedir/@project" << true << true;
    QTest::newRow("@project, without project file") << "somedir/@project" << false << false;
    QTest::newRow("@path, with project file") << "somedir/@path" << true << true;
    QTest::newRow("@path, without project file") << "somedir/@path" << false << false;
}

void TestBlackbox::buildDirPlaceholders()
{
    QFETCH(QString, buildDir);
    QFETCH(bool, setProjectFile);
    QFETCH(bool, successExpected);

    const QString projectDir = testDataDir + "/build-dir-placeholders";
    rmDirR(projectDir);
    QVERIFY(QDir().mkpath(projectDir));
    QDir::setCurrent(projectDir);
    QFile projectFile("build-dir-placeholders.qbs");
    QVERIFY(projectFile.open(QIODevice::WriteOnly));
    projectFile.write("Product {\n}\n");
    projectFile.flush();
    rmDirR(relativeBuildDir());
    QbsRunParameters params;
    params.buildDirectory = buildDir;
    if (setProjectFile) {
        params.arguments << "-f"
                         << "build-dir-placeholders.qbs";
    }
    params.expectFailure = !successExpected;
    QCOMPARE(runQbs(params) == 0, successExpected);
}

void TestBlackbox::buildEnvChange()
{
    QDir::setCurrent(testDataDir + "/buildenv-change");
    QbsRunParameters params;
    params.expectFailure = true;
    params.arguments << "-k";
    QVERIFY(runQbs(params) != 0);
    const bool isMsvc = m_qbsStdout.contains("msvc");
    QVERIFY2(m_qbsStdout.contains("compiling file.c"), m_qbsStdout.constData());
    QString includePath = QDir::currentPath() + "/subdir";
    params.environment.insert("CPLUS_INCLUDE_PATH", includePath);
    params.environment.insert("CL", "/I" + includePath);
    QVERIFY(runQbs(params) != 0);
    params.command = "resolve";
    params.expectFailure = false;
    params.arguments.clear();
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QCOMPARE(m_qbsStdout.contains("compiling file.c"), isMsvc);
    includePath = QDir::currentPath() + "/subdir2";
    params.environment.insert("CPLUS_INCLUDE_PATH", includePath);
    params.environment.insert("CL", "/I" + includePath);
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QCOMPARE(m_qbsStdout.contains("compiling file.c"), isMsvc);
    params.environment = QProcessEnvironment::systemEnvironment();
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
}

void TestBlackbox::buildGraphVersions()
{
    QDir::setCurrent(testDataDir + "/build-graph-versions");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QFile bgFile(relativeBuildGraphFilePath());
    QVERIFY2(bgFile.open(QIODevice::ReadWrite), qPrintable(bgFile.errorString()));
    bgFile.write("blubb");
    bgFile.close();

    // The first attempt at simple rebuilding as well as subsequent ones must fail.
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Cannot use stored build graph"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.contains("Use the 'resolve' command"), m_qbsStderr.constData());
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Cannot use stored build graph"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.contains("Use the 'resolve' command"), m_qbsStderr.constData());

    // On re-resolving, the error turns into a warning and a new build graph is created.
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    QVERIFY2(m_qbsStderr.contains("Cannot use stored build graph"), m_qbsStderr.constData());
    QVERIFY2(!m_qbsStderr.contains("Use the 'resolve' command"), m_qbsStderr.constData());

    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStderr.contains("Cannot use stored build graph"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::buildVariantDefaults_data()
{
    QTest::addColumn<QString>("buildVariant");
    QTest::newRow("default") << QString();
    QTest::newRow("debug") << QStringLiteral("debug");
    QTest::newRow("release") << QStringLiteral("release");
    QTest::newRow("profiling") << QStringLiteral("profiling");
}

void TestBlackbox::buildVariantDefaults()
{
    QFETCH(QString, buildVariant);
    QDir::setCurrent(testDataDir + "/build-variant-defaults");
    QbsRunParameters params{QStringLiteral("resolve")};
    if (!buildVariant.isEmpty())
        params.arguments << ("modules.qbs.buildVariant:" + buildVariant);
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::capnproto()
{
    QFETCH(QString, projectFile);
    QFETCH(QStringList, arguments);
    QDir::setCurrent(testDataDir + "/capnproto");
    rmDirR(relativeBuildDir());

    if (QTest::currentDataTag() == QLatin1String("cpp-conan")
        || QTest::currentDataTag() == QLatin1String("rpc-conan")) {
        if (!prepareAndRunConan())
            QSKIP("conan is not prepared, check messages above");
    }

    QbsRunParameters params{QStringLiteral("resolve"), {QStringLiteral("-f"), projectFile}};
    params.arguments << arguments;
    QCOMPARE(runQbs(params), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    if (m_qbsStdout.contains("capnproto is not present"))
        QSKIP("capnproto is not present");

    params.command = QStringLiteral("build");
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::capnproto_data()
{
    QTest::addColumn<QString>("projectFile");
    QTest::addColumn<QStringList>("arguments");

    QStringList pkgConfigArgs({"project.qbsModuleProviders:qbspkgconfig"});
    QTest::newRow("cpp-pkgconfig") << QStringLiteral("capnproto_cpp.qbs") << pkgConfigArgs;
    QTest::newRow("rpc-pkgconfig") << QStringLiteral("greeter_cpp.qbs") << pkgConfigArgs;
    QTest::newRow("relative import")
        << QStringLiteral("capnproto_relative_import.qbs") << pkgConfigArgs;
    QTest::newRow("absolute import")
        << QStringLiteral("capnproto_absolute_import.qbs") << pkgConfigArgs;

    QStringList conanArgs(
        {"project.qbsModuleProviders:conan", "moduleProviders.conan.installDirectory:build"});
    QTest::newRow("cpp-conan") << QStringLiteral("capnproto_cpp.qbs") << conanArgs;
    QTest::newRow("rpc-conan") << QStringLiteral("greeter_cpp.qbs") << conanArgs;
}

void TestBlackbox::changedFiles_data()
{
    QTest::addColumn<bool>("useChangedFilesForInitialBuild");
    QTest::newRow("initial build with changed files") << true;
    QTest::newRow("initial build without changed files") << false;
}

void TestBlackbox::changedFiles()
{
    QFETCH(bool, useChangedFilesForInitialBuild);

    QDir::setCurrent(testDataDir + "/changed-files");
    rmDirR(relativeBuildDir());
    const QString changedFile = QDir::cleanPath(QDir::currentPath() + "/file1.cpp");
    QbsRunParameters params1;
    if (useChangedFilesForInitialBuild)
        params1 = QbsRunParameters(QStringList("--changed-files") << changedFile);

    // Initial run: Build all files, even though only one of them was marked as changed
    //              (if --changed-files was used).
    QCOMPARE(runQbs(params1), 0);
    QCOMPARE(m_qbsStdout.count("compiling"), 3);
    QCOMPARE(m_qbsStdout.count("creating"), 3);

    WAIT_FOR_NEW_TIMESTAMP();
    touch(QDir::currentPath() + "/main.cpp");

    // Now only the file marked as changed must be compiled, even though it hasn't really
    // changed and another one has.
    QbsRunParameters params2(QStringList("--changed-files") << changedFile);
    QCOMPARE(runQbs(params2), 0);
    QCOMPARE(m_qbsStdout.count("compiling"), 1);
    QCOMPARE(m_qbsStdout.count("creating"), 1);
    QVERIFY2(m_qbsStdout.contains("file1.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::changedInputsFromDependencies()
{
    QDir::setCurrent(testDataDir + "/changed-inputs-from-dependencies");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("final prepare script"), m_qbsStdout.constData());
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("final prepare script"), m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    touch("input.txt");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("final prepare script"), m_qbsStdout.constData());
}

void TestBlackbox::changedRuleInputs()
{
    QDir::setCurrent(testDataDir + "/changed-rule-inputs");

    // Initial build.
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("generating p1-dummy"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("generating p2-dummy"), m_qbsStdout.constData());

    // Re-build: p1 is always regenerated, and p2 has a dependency on it.
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("generating p1-dummy"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("generating p2-dummy"), m_qbsStdout.constData());

    // Remove the dependency. p2 gets re-generated one last time, because its set of
    // inputs changed.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("changed-rule-inputs.qbs", "inputsFromDependencies: \"p1\"",
                    "inputsFromDependencies: \"p3\"");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("generating p1-dummy"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("generating p2-dummy"), m_qbsStdout.constData());

    // Now the artifacts are no longer connected, and p2 must not get rebuilt anymore.
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("generating p1-dummy"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("generating p2-dummy"), m_qbsStdout.constData());
}

void TestBlackbox::changeInDisabledProduct()
{
    QDir::setCurrent(testDataDir + "/change-in-disabled-product");
    QCOMPARE(runQbs(), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("change-in-disabled-product.qbs", "// 'test2.txt'", "'test2.txt'");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::changeInImportedFile()
{
    QDir::setCurrent(testDataDir + "/change-in-imported-file");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("old output"), m_qbsStdout.constData());

    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("prepare.js", "old output", "new output");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("new output"), m_qbsStdout.constData());

    WAIT_FOR_NEW_TIMESTAMP();
    touch("prepare.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("output"), m_qbsStdout.constData());
}

void TestBlackbox::changeTrackingAndMultiplexing()
{
    QDir::setCurrent(testDataDir + "/change-tracking-and-multiplexing");
    QCOMPARE(runQbs(QStringList("modules.cpp.staticLibraryPrefix:prefix1")), 0);
    QCOMPARE(m_qbsStdout.count("compiling lib.cpp"), 2);
    QCOMPARE(m_qbsStdout.count("creating prefix1l"), 2);
    QCOMPARE(runQbs(QbsRunParameters("resolve",
                                     QStringList("modules.cpp.staticLibraryPrefix:prefix2"))), 0);
    QCOMPARE(runQbs(), 0);
    QCOMPARE(m_qbsStdout.count("compiling lib.cpp"), 0);
    QCOMPARE(m_qbsStdout.count("creating prefix2l"), 2);
}

static QJsonObject findByName(const QJsonArray &objects, const QString &name)
{
    for (const QJsonValue &v : objects) {
        if (!v.isObject())
            continue;
        QJsonObject obj = v.toObject();
        const QString objName = obj.value(QStringLiteral("name")).toString();
        if (objName == name)
            return obj;
    }
    return {};
}

static void readDepsOutput(const QString &depsFilePath, QJsonDocument &jsonDocument)
{
    jsonDocument = QJsonDocument();
    QFile depsFile(depsFilePath);
    QVERIFY2(depsFile.open(QFile::ReadOnly), qPrintable(depsFile.errorString()));
    QJsonParseError jsonerror;
    jsonDocument = QJsonDocument::fromJson(depsFile.readAll(), &jsonerror);
    if (jsonerror.error != QJsonParseError::NoError) {
        qDebug() << jsonerror.errorString();
        QFAIL("JSON parsing failed.");
    }
}

void TestBlackbox::dependenciesProperty()
{
    QDir::setCurrent(testDataDir + QLatin1String("/dependenciesProperty"));
    QCOMPARE(runQbs(), 0);
    const QString depsFile(relativeProductBuildDir("product1") + "/product1.deps");
    QJsonDocument jsondoc;
    readDepsOutput(depsFile, jsondoc);
    QVERIFY(jsondoc.isArray());
    QJsonArray dependencies = jsondoc.array();
    QCOMPARE(dependencies.size(), 2);
    QJsonObject product2 = findByName(dependencies, QStringLiteral("product2"));
    QJsonArray product2_type = product2.value(QStringLiteral("type")).toArray();
    QCOMPARE(product2_type.size(), 1);
    QCOMPARE(product2_type.first().toString(), QLatin1String("application"));
    QCOMPARE(product2.value(QLatin1String("narf")).toString(), QLatin1String("zort"));
    QJsonArray product2_cppArtifacts
            = product2.value("artifacts").toObject().value("cpp").toArray();
    QCOMPARE(product2_cppArtifacts.size(), 1);
    QJsonArray product2_deps = product2.value(QStringLiteral("dependencies")).toArray();
    QVERIFY(!product2_deps.empty());
    QJsonObject product2_qbs = findByName(product2_deps, QStringLiteral("qbs"));
    QVERIFY(!product2_qbs.empty());
    QJsonObject product2_cpp = findByName(product2_deps, QStringLiteral("cpp"));
    QJsonArray product2_cpp_defines = product2_cpp.value(QLatin1String("defines")).toArray();
    QCOMPARE(product2_cpp_defines.size(), 1);
    QCOMPARE(product2_cpp_defines.first().toString(), QLatin1String("SMURF"));
    QJsonArray cpp_dependencies = product2_cpp.value("dependencies").toArray();
    QVERIFY(!cpp_dependencies.isEmpty());
    int qbsCount = 0;
    for (const auto dep : cpp_dependencies) {
        if (dep.toObject().value("name").toString() == "qbs")
            ++qbsCount;
    }
    QCOMPARE(qbsCount, 1);

    // Add new dependency, check that command is re-run.
    const QString projectFile("dependenciesProperty.qbs");
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFile, "// Depends { name: 'newDependency' }",
                    "Depends { name: 'newDependency' }");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("generating product1.deps"), m_qbsStdout.constData());
    readDepsOutput(depsFile, jsondoc);
    dependencies = jsondoc.array();
    QCOMPARE(dependencies.size(), 3);

    // Add new Depends item that does not actually introduce a new dependency, check
    // that command is not re-run.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFile, "// Depends { name: 'product2' }", "Depends { name: 'product2' }");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("generating product1.deps"), m_qbsStdout.constData());
    readDepsOutput(depsFile, jsondoc);
    dependencies = jsondoc.array();
    QCOMPARE(dependencies.size(), 3);

    // Change property of dependency, check that command is re-run.
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList{"products.product2.narf:zortofsky"})),
             0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling product2.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("generating product1.deps"), m_qbsStdout.constData());
    readDepsOutput(depsFile, jsondoc);
    dependencies = jsondoc.array();
    QCOMPARE(dependencies.size(), 3);
    product2 = findByName(dependencies, QStringLiteral("product2"));
    QCOMPARE(product2.value(QLatin1String("narf")).toString(), QLatin1String("zortofsky"));

    // Change module property of dependency, check that command is re-run.
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList{"products.product2.narf:zortofsky",
                                     "products.product2.cpp.defines:DIGEDAG"})), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling product2.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("generating product1.deps"), m_qbsStdout.constData());
    readDepsOutput(depsFile, jsondoc);
    dependencies = jsondoc.array();
    QCOMPARE(dependencies.size(), 3);
    product2 = findByName(dependencies, QStringLiteral("product2"));
    product2_deps = product2.value(QStringLiteral("dependencies")).toArray();
    product2_cpp = findByName(product2_deps, QStringLiteral("cpp"));
    product2_cpp_defines = product2_cpp.value(QStringLiteral("defines")).toArray();
    QCOMPARE(product2_cpp_defines.size(), 1);
    QCOMPARE(product2_cpp_defines.first().toString(), QLatin1String("DIGEDAG"));
}

void TestBlackbox::dependencyScanningLoop()
{
    QDir::setCurrent(testDataDir + "/dependency-scanning-loop");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::deprecatedProperty()
{
    QFETCH(QString, version);
    QFETCH(QString, mode);
    QFETCH(bool, expiringWarning);
    QFETCH(bool, expiringError);

    QDir::setCurrent(testDataDir + "/deprecated-property");
    QbsRunParameters params(QStringList("-q"));
    params.expectFailure = true;
    params.environment.insert("REMOVAL_VERSION", version);
    if (!mode.isEmpty())
        params.arguments << "--deprecation-warnings" << mode;
    QVERIFY(runQbs(params) != 0);
    m_qbsStderr = QDir::fromNativeSeparators(QString::fromLocal8Bit(m_qbsStderr)).toLocal8Bit();
    const bool hasExpiringWarning = m_qbsStderr.contains(QByteArray(
            "deprecated-property.qbs:4:29 The property 'expiringProp' is "
            "deprecated and will be removed in Qbs ") + version.toLocal8Bit());
    QVERIFY2(expiringWarning == hasExpiringWarning, m_qbsStderr.constData());
    const bool hasRemovedOutput = m_qbsStderr.contains(
                "deprecated-property.qbs:5:28 The property 'veryOldProp' can no "
                "longer be used. It was removed in Qbs 1.3.0.");
    QVERIFY2(hasRemovedOutput == !expiringError, m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.contains("Property 'forgottenProp' was scheduled for removal in version "
                                  "1.8.0, but is still present."), m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.contains("themodule/m.qbs:22:5 Removal version for 'forgottenProp' "
                                  "specified here."), m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.count("Use newProp instead.") == 1
             + int(expiringWarning && !expiringError), m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.count("is deprecated") == int(expiringWarning), m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.count("was removed") == int(!expiringError), m_qbsStderr.constData());
}

void TestBlackbox::deprecatedProperty_data()
{
    QTest::addColumn<QString>("version");
    QTest::addColumn<QString>("mode");
    QTest::addColumn<bool>("expiringWarning");
    QTest::addColumn<bool>("expiringError");

    const auto current = QVersionNumber::fromString(QBS_VERSION);
    const QString next = QVersionNumber(current.majorVersion(), current.minorVersion() + 1)
            .toString();
    const QString nextNext = QVersionNumber(current.majorVersion(), current.minorVersion() + 2)
            .toString();
    const QString nextMajor = QVersionNumber(current.majorVersion() + 1).toString();

    QTest::newRow("default/next") << next << QString() << true << false;
    QTest::newRow("default/nextnext") << nextNext << QString() << false << false;
    QTest::newRow("default/nextmajor") << nextMajor << QString() << true << false;
    QTest::newRow("error/next") << next << QString("error") << true << true;
    QTest::newRow("error/nextnext") << nextNext << QString("error") << true << true;
    QTest::newRow("error/nextmajor") << nextMajor << QString("error") << true << true;
    QTest::newRow("on/next") << next << QString("on") << true << false;
    QTest::newRow("on/nextnext") << nextNext << QString("on") << true << false;
    QTest::newRow("on/nextmajor") << nextMajor << QString("on") << true << false;
    QTest::newRow("before-removal/next") << next << QString("before-removal") << true << false;
    QTest::newRow("before-removal/nextnext") << nextNext << QString("before-removal")
                                             << false << false;
    QTest::newRow("before-removal/nextmajor") << nextMajor << QString("before-removal")
                                             << true << false;
    QTest::newRow("off/next") << next << QString("off") << false << false;
    QTest::newRow("off/nextnext") << nextNext << QString("off") << false << false;
    QTest::newRow("off/nextmajor") << nextMajor << QString("off") << false << false;
}

void TestBlackbox::disappearedProfile()
{
    QDir::setCurrent(testDataDir + "/disappeared-profile");
    QbsRunParameters resolveParams;

    // First, we need to fail, because we don't tell qbs where the module is.
    resolveParams.expectFailure = true;
    QVERIFY(runQbs(resolveParams) != 0);

    // Now we set up a profile with all the necessary information, and qbs succeeds.
    qbs::Settings settings(QDir::currentPath() + "/settings-dir");
    qbs::Profile profile("p", &settings);
    profile.setValue("m.p1", "p1 from profile");
    profile.setValue("m.p2", "p2 from profile");
    profile.setValue("preferences.qbsSearchPaths",
                     QStringList({QDir::currentPath() + "/modules-dir"}));
    settings.sync();
    resolveParams.command = "resolve";
    resolveParams.expectFailure = false;
    resolveParams.settingsDir = settings.baseDirectory();
    resolveParams.profile = profile.name();
    QCOMPARE(runQbs(resolveParams), 0);

    // Now we change a property in the profile, but because we don't use the "resolve" command,
    // the old profile contents stored in the build graph are used.
    profile.setValue("m.p2", "p2 new from profile");
    settings.sync();
    QbsRunParameters buildParams;
    buildParams.profile.clear();
    QCOMPARE(runQbs(buildParams), 0);
    QVERIFY2(m_qbsStdout.contains("creating dummy1.txt with p1 from profile"),
             m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("creating dummy2.txt with p2 from profile"),
             m_qbsStdout.constData());

    // Now we do use the "resolve" command, so the new property value is taken into account.
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(buildParams), 0);
    QVERIFY2(!m_qbsStdout.contains("creating dummy1.txt"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("creating dummy2.txt with p2 new from profile"),
             m_qbsStdout.constData());

    // Now we change the profile again without a "resolve" command. However, this time we
    // force re-resolving indirectly by changing a project file. The updated property value
    // must still not be taken into account.
    profile.setValue("m.p1", "p1 new from profile");
    settings.sync();
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("modules-dir/modules/m/m.qbs", "property string p1",
                    "property string p1: 'p1 from module'");
    QCOMPARE(runQbs(buildParams), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("creating dummy1.txt"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("creating dummy2.txt"), m_qbsStdout.constData());

    // Now we run the "resolve" command without giving the necessary settings path to find
    // the profile.
    resolveParams.expectFailure = true;
    resolveParams.settingsDir.clear();
    resolveParams.profile.clear();
    QVERIFY(runQbs(resolveParams) != 0);
    QVERIFY2(m_qbsStderr.contains("profile"), m_qbsStderr.constData());
}

void TestBlackbox::discardUnusedData()
{
    QDir::setCurrent(testDataDir + "/discard-unused-data");
    rmDirR(relativeBuildDir());
    QFETCH(QString, discardString);
    QFETCH(bool, symbolPresent);
    QbsRunParameters params;
    if (!discardString.isEmpty())
        params.arguments << ("modules.cpp.discardUnusedData:" + discardString);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("is Darwin"), m_qbsStdout.constData());
    const bool isDarwin = m_qbsStdout.contains("is Darwin: true");
    const QString output = QString::fromLocal8Bit(m_qbsStdout);
    const QRegularExpression pattern(QRegularExpression::anchoredPattern(".*---(.*)---.*"),
                                     QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = pattern.match(output);
    QVERIFY2(match.hasMatch(), qPrintable(output));
    QCOMPARE(match.lastCapturedIndex(), 1);
    const QString nmPath = match.captured(1);
    if (!QFile::exists(nmPath))
        QSKIP("Cannot check for symbol presence: No nm found.");
    QProcess nm;
    nm.start(
        nmPath,
        QStringList(QDir::currentPath() + '/' + relativeExecutableFilePath("app", m_qbsStdout)));
    QVERIFY(nm.waitForStarted());
    QVERIFY(nm.waitForFinished());
    const QByteArray nmOutput = nm.readAllStandardOutput();
    QVERIFY2(nm.exitCode() == 0, nm.readAllStandardError().constData());
    if (!symbolPresent && !isDarwin)
        QSKIP("Unused symbol detection only supported on Darwin");
    QVERIFY2(nmOutput.contains("unusedFunc") == symbolPresent, nmOutput.constData());
}

void TestBlackbox::discardUnusedData_data()
{
    QTest::addColumn<QString>("discardString");
    QTest::addColumn<bool>("symbolPresent");

    QTest::newRow("discard") << QString("true") << false;
    QTest::newRow("don't discard") << QString("false") << true;
    QTest::newRow("default") << QString() << true;
}

void TestBlackbox::dotDotPcFile()
{
    QDir::setCurrent(testDataDir + "/dot-dot-pc-file");

    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::driverLinkerFlags()
{
    QDir::setCurrent(testDataDir + QLatin1String("/driver-linker-flags"));
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("-n"))), 0);
    if (!m_qbsStdout.contains("toolchain is GCC-like"))
        QSKIP("Test applies on GCC-like toolchains only");
    QFETCH(QString, linkerMode);
    QFETCH(bool, expectDriverOption);
    const QString linkerModeArg = "modules.cpp.linkerMode:" + linkerMode;
    QCOMPARE(runQbs(QStringList({"-n", "--command-echo-mode", "command-line", linkerModeArg})), 0);
    const QByteArray driverArg = "-nostartfiles";
    const QByteArrayList output = m_qbsStdout.split('\n');
    QByteArray compileLine;
    QByteArray linkLine;
    for (const QByteArray &line : output) {
        if (line.contains(" -c "))
            compileLine = line;
        else if (line.contains("main.cpp.o"))
            linkLine = line;
    }
    QVERIFY(!compileLine.isEmpty());
    QVERIFY(!linkLine.isEmpty());
    QVERIFY2(!compileLine.contains(driverArg), compileLine.constData());
    QVERIFY2(linkLine.contains(driverArg) == expectDriverOption, linkLine.constData());
}

void TestBlackbox::driverLinkerFlags_data()
{
    QTest::addColumn<QString>("linkerMode");
    QTest::addColumn<bool>("expectDriverOption");

    QTest::newRow("link using compiler driver") << "automatic" << true;
    QTest::newRow("link using linker") << "manual" << false;
}

void TestBlackbox::dynamicLibraryInModule()
{
    QDir::setCurrent(testDataDir + "/dynamic-library-in-module");
    const QString installRootSpec = QString("qbs.installRoot:") + QDir::currentPath();
    QbsRunParameters libParams(QStringList({"-f", "thelibs.qbs", installRootSpec}));
    libParams.buildDirectory = "libbuild";
    QCOMPARE(runQbs(libParams), 0);
    QbsRunParameters appParams("build", QStringList({"-f", "theapp.qbs", installRootSpec}));
    appParams.buildDirectory = "appbuild";
    QCOMPARE(runQbs(appParams), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    appParams.command = "run";
    QCOMPARE(runQbs(appParams), 0);
    QVERIFY2(m_qbsStdout.contains("Hello from thelib"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("Hello from theotherlib"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("thirdlib"), m_qbsStdout.constData());
    QVERIFY(!QFileInfo::exists(appParams.buildDirectory + '/'
                               + qbs::InstallOptions::defaultInstallRoot()));
}

void TestBlackbox::symlinkRemoval()
{
    if (HostOsInfo::isWindowsHost())
        QSKIP("No symlink support on Windows.");
    QDir::setCurrent(testDataDir + "/symlink-removal");
    QVERIFY(QDir::current().mkdir("dir1"));
    QVERIFY(QDir::current().mkdir("dir2"));
    QVERIFY(QFile::link("dir2", "dir1/broken-link"));
    QVERIFY(QFile::link(QFileInfo("dir2").absoluteFilePath(), "dir1/valid-link-to-dir"));
    QVERIFY(QFile::link(QFileInfo("symlink-removal.qbs").absoluteFilePath(),
                        "dir1/valid-link-to-file"));
    QCOMPARE(runQbs(), 0);
    QVERIFY(!QFile::exists("dir1"));
    QVERIFY(QFile::exists("dir2"));
    QVERIFY(QFile::exists("symlink-removal.qbs"));
}

void TestBlackbox::usingsAsSoleInputsNonMultiplexed()
{
    QDir::setCurrent(testDataDir + QLatin1String("/usings-as-sole-inputs-non-multiplexed"));
    QCOMPARE(runQbs(), 0);
    const QString p3BuildDir = relativeProductBuildDir("p3");
    QVERIFY(regularFileExists(p3BuildDir + "/custom1.out.plus"));
    QVERIFY(regularFileExists(p3BuildDir + "/custom2.out.plus"));
}

void TestBlackbox::variantSuffix()
{
    QDir::setCurrent(testDataDir + "/variant-suffix");
    QFETCH(bool, multiplex);
    QFETCH(bool, expectFailure);
    QFETCH(QString, variantSuffix);
    QFETCH(QString, buildVariant);
    QFETCH(QVariantMap, fileNames);
    QbsRunParameters params;
    params.command = "resolve";
    params.arguments << "--force-probe-execution";
    if (multiplex)
        params.arguments << "products.l.multiplex:true";
    else
        params.arguments << ("modules.qbs.buildVariant:" + buildVariant);
    if (!variantSuffix.isEmpty())
        params.arguments << ("modules.cpp.variantSuffix:" + variantSuffix);
    QCOMPARE(runQbs(params), 0);
    const QString fileNameMapKey = m_qbsStdout.contains("is Windows: true")
            ? "windows" : m_qbsStdout.contains("is Apple: true") ? "apple" : "unix";
    if (variantSuffix.isEmpty() && multiplex && fileNameMapKey == "unix")
        expectFailure = true;
    params.command = "build";
    params.expectFailure = expectFailure;
    params.arguments = QStringList("--clean-install-root");
    QCOMPARE(runQbs(params) == 0, !expectFailure);
    if (expectFailure)
        return;
    const QStringList fileNameList = fileNames.value(fileNameMapKey).toStringList();
    for (const QString &fileName : fileNameList) {
        QFile libFile("default/install-root/lib/" + fileName);
        QVERIFY2(libFile.exists(), qPrintable(libFile.fileName()));
    }
}

void TestBlackbox::variantSuffix_data()
{
    QTest::addColumn<bool>("multiplex");
    QTest::addColumn<bool>("expectFailure");
    QTest::addColumn<QString>("variantSuffix");
    QTest::addColumn<QString>("buildVariant");
    QTest::addColumn<QVariantMap>("fileNames");

    QTest::newRow("default suffix, debug") << false << false << QString() << QString("debug")
            << QVariantMap({std::make_pair(QString("windows"), QStringList("libl.ext")),
                            std::make_pair(QString("apple"), QStringList("libl.ext")),
                            std::make_pair(QString("unix"), QStringList("libl.ext"))});
    QTest::newRow("default suffix, release") << false << false << QString() << QString("release")
            << QVariantMap({std::make_pair(QString("windows"), QStringList("libl.ext")),
                            std::make_pair(QString("apple"), QStringList("libl.ext")),
                            std::make_pair(QString("unix"), QStringList("libl.ext"))});
    QTest::newRow("custom suffix, debug") << false << false << QString("blubb") << QString("debug")
            << QVariantMap({std::make_pair(QString("windows"), QStringList("liblblubb.ext")),
                            std::make_pair(QString("apple"), QStringList("liblblubb.ext")),
                            std::make_pair(QString("unix"), QStringList("liblblubb.ext"))});
    QTest::newRow("custom suffix, release") << false << false << QString("blubb")
            << QString("release")
            << QVariantMap({std::make_pair(QString("windows"), QStringList("liblblubb.ext")),
                            std::make_pair(QString("apple"), QStringList("liblblubb.ext")),
                            std::make_pair(QString("unix"), QStringList("liblblubb.ext"))});
    QTest::newRow("default suffix, multiplex") << true << false << QString() << QString()
            << QVariantMap({std::make_pair(QString("windows"),
                            QStringList({"libl.ext", "libld.ext"})),
                            std::make_pair(QString("apple"),
                            QStringList({"libl.ext", "libl_debug.ext"})),
                            std::make_pair(QString("unix"), QStringList())});
    QTest::newRow("custom suffix, multiplex") << true << true << QString("blubb") << QString()
            << QVariantMap({std::make_pair(QString("windows"), QStringList()),
                            std::make_pair(QString("apple"), QStringList()),
                            std::make_pair(QString("unix"), QStringList())});
}

void TestBlackbox::vcsGit()
{
    const QString gitFilePath = findExecutable(QStringList("git"));
    if (gitFilePath.isEmpty())
        QSKIP("git not found");

    // Set up repo.
    QTemporaryDir repoDir;
    QVERIFY(repoDir.isValid());
    ccp(testDataDir + "/vcs", repoDir.path());
    QDir::setCurrent(repoDir.path());

    QProcess git;
    git.start(gitFilePath, QStringList("init"));
    QVERIFY(waitForProcessSuccess(git));
    git.start(gitFilePath, QStringList({"config", "user.name", "My Name"}));
    QVERIFY(waitForProcessSuccess(git));
    git.start(gitFilePath, QStringList({"config", "user.email", "me@example.com"}));
    QVERIFY(waitForProcessSuccess(git));

    QCOMPARE(runQbs({"resolve", {"-f", repoDir.path()}}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    // Run without git metadata.
    QbsRunParameters params("run", QStringList{"-f", repoDir.path()});
    params.workingDir = repoDir.path() + "/..";
    params.buildDirectory = repoDir.path();
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("generating my-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    QMap<QString, QByteArray> newRepoState = getRepoStateFromApp();
    QVERIFY(newRepoState.contains("repoState"));
    QVERIFY(newRepoState.contains("latestTag"));
    QVERIFY(newRepoState.contains("commitsSinceTag"));
    QVERIFY(newRepoState.contains("commitSha"));
    QCOMPARE(newRepoState["repoState"], QByteArray("none"));
    QCOMPARE(newRepoState["latestTag"], QByteArray("none"));
    QCOMPARE(newRepoState["commitsSinceTag"], QByteArray("none"));
    QCOMPARE(newRepoState["commitSha"], QByteArray("none"));
    QMap<QString, QByteArray> oldRepoState = newRepoState;

    // Initial commit
    git.start(gitFilePath, QStringList({"add", "main.cpp"}));
    QVERIFY(waitForProcessSuccess(git));
    git.start(gitFilePath, QStringList({"commit", "-m", "initial commit"}));
    QVERIFY(waitForProcessSuccess(git));

    // Run with git metadata.
    WAIT_FOR_NEW_TIMESTAMP();
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("generating my-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    newRepoState = getRepoStateFromApp();
    QCOMPARE(newRepoState["latestTag"], QByteArray("none"));
    QCOMPARE(newRepoState["commitsSinceTag"], QByteArray("none"));
    QVERIFY(newRepoState["commitSha"] != QByteArray("none"));
    QVERIFY(newRepoState["commitSha"].startsWith(QByteArray("g")));
    QVERIFY(oldRepoState["repoState"] != newRepoState["repoState"]);
    oldRepoState = newRepoState;

    // Run with no changes.
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(!m_qbsStdout.contains("generating my-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    newRepoState = getRepoStateFromApp();
    QCOMPARE(oldRepoState, newRepoState);

    // Run with changed source file.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("main.cpp");
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(!m_qbsStdout.contains("generating my-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    newRepoState = getRepoStateFromApp();
    QCOMPARE(oldRepoState, newRepoState);

    // Add tag with dash
    WAIT_FOR_NEW_TIMESTAMP();
    touch("dummy.txt");
    git.start(gitFilePath, QStringList({"add", "dummy.txt"}));
    QVERIFY(waitForProcessSuccess(git));
    git.start(gitFilePath, QStringList({"commit", "-m", "dummy!"}));
    QVERIFY(waitForProcessSuccess(git));
    git.start(gitFilePath, QStringList({"tag", "-a", "v1.0.0-beta", "-m", "Version 1.0.0-beta"}));
    QVERIFY(waitForProcessSuccess(git));
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("generating my-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    newRepoState = getRepoStateFromApp();
    QCOMPARE(newRepoState["latestTag"], QByteArray("v1.0.0-beta"));
    QCOMPARE(newRepoState["commitsSinceTag"], QByteArray("0"));
    QVERIFY(newRepoState["commitSha"] != QByteArray("none"));
    QVERIFY(newRepoState["commitSha"].startsWith(QByteArray("g")));
    QVERIFY(oldRepoState["repoState"] != newRepoState["repoState"]);
    oldRepoState = newRepoState;

    // Add new file to repo. Add new tag
    WAIT_FOR_NEW_TIMESTAMP();
    touch("blubb.txt");
    git.start(gitFilePath, QStringList({"add", "blubb.txt"}));
    QVERIFY(waitForProcessSuccess(git));
    git.start(gitFilePath, QStringList({"commit", "-m", "blubb!"}));
    QVERIFY(waitForProcessSuccess(git));
    git.start(gitFilePath, QStringList({"tag", "-a", "v1.0.0", "-m", "Version 1.0.0-beta"}));
    QVERIFY(waitForProcessSuccess(git));
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("generating my-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    newRepoState = getRepoStateFromApp();
    QCOMPARE(newRepoState["latestTag"], QByteArray("v1.0.0"));
    QCOMPARE(newRepoState["commitsSinceTag"], QByteArray("0"));
    QVERIFY(newRepoState["commitSha"] != QByteArray("none"));
    QVERIFY(newRepoState["commitSha"].startsWith(QByteArray("g")));
    QVERIFY(oldRepoState["repoState"] != newRepoState["repoState"]);

    // https://bugreports.qt.io/projects/QBS/issues/QBS-1814
    oldRepoState = newRepoState;
    WAIT_FOR_NEW_TIMESTAMP();
    touch("loremipsum.txt");
    git.start(gitFilePath, QStringList({"add", "loremipsum.txt"}));
    QVERIFY(waitForProcessSuccess(git));
    git.start(gitFilePath, QStringList({"commit", "-m", "loremipsum!"}));
    QVERIFY(waitForProcessSuccess(git));
    // Remove .git/logs/HEAD
    QFile::remove(repoDir.path() + "/.git/logs/HEAD");
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("generating my-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    newRepoState = getRepoStateFromApp();
    QCOMPARE(newRepoState["latestTag"], QByteArray("v1.0.0"));
    QCOMPARE(newRepoState["commitsSinceTag"], QByteArray("1"));
    QVERIFY(newRepoState["commitSha"] != QByteArray("none"));
    QVERIFY(newRepoState["commitSha"].startsWith(QByteArray("g")));
    QVERIFY(oldRepoState["repoState"] != newRepoState["repoState"]);
}

void TestBlackbox::vcsSubversion()
{
    const QString svnadminFilePath = findExecutable(QStringList("svnadmin"));
    if (svnadminFilePath.isEmpty())
        QSKIP("svnadmin not found");
    const QString svnFilePath = findExecutable(QStringList("svn"));
    if (svnFilePath.isEmpty())
        QSKIP("svn not found");

    if (HostOsInfo::isWindowsHost() && qEnvironmentVariableIsSet("GITHUB_ACTIONS"))
        QSKIP("Skip this test when running on GitHub");

    // Set up repo.
    QTemporaryDir repoDir;
    QVERIFY(repoDir.isValid());
    QProcess proc;
    proc.setWorkingDirectory(repoDir.path());
    proc.start(svnadminFilePath, QStringList({"create", "vcstest"}));
    QVERIFY(waitForProcessSuccess(proc));
    const QString projectUrl = "file://" + repoDir.path() + "/vcstest/trunk";
    proc.start(svnFilePath, QStringList({"import", testDataDir + "/vcs", projectUrl, "-m",
                                         "initial import"}));
    QVERIFY(waitForProcessSuccess(proc));
    QTemporaryDir checkoutDir;
    QVERIFY(checkoutDir.isValid());
    proc.setWorkingDirectory(checkoutDir.path());
    proc.start(svnFilePath, QStringList({"co", projectUrl, "."}));
    QVERIFY(waitForProcessSuccess(proc));

    // Initial runs
    QDir::setCurrent(checkoutDir.path());
    QbsRunParameters failParams;
    failParams.command = "run";
    failParams.expectFailure = true;
    const int retval = runQbs(failParams);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    if (m_qbsStderr.contains("svn too old"))
        QSKIP("svn too old");
    QCOMPARE(retval, 0);
    QMap<QString, QByteArray> newRepoState = getRepoStateFromApp();
    QVERIFY(newRepoState.contains("repoState"));
    QVERIFY(newRepoState.contains("latestTag"));
    QVERIFY(newRepoState.contains("commitsSinceTag"));
    QVERIFY(newRepoState.contains("commitSha"));
    QCOMPARE(newRepoState["repoState"], QByteArray("1"));
    QCOMPARE(newRepoState["latestTag"], QByteArray("none"));
    QCOMPARE(newRepoState["commitsSinceTag"], QByteArray("none"));
    QCOMPARE(newRepoState["commitSha"], QByteArray("none"));
    QMap<QString, QByteArray> oldRepoState = newRepoState;
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QVERIFY2(!m_qbsStdout.contains("generating my-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    newRepoState = getRepoStateFromApp();
    QCOMPARE(oldRepoState, newRepoState);

    // Run with changed source file.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("main.cpp");
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QVERIFY2(!m_qbsStdout.contains("generating my-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    newRepoState = getRepoStateFromApp();
    QCOMPARE(oldRepoState, newRepoState);

    // Add new file to repo.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("blubb.txt");
    proc.start(svnFilePath, QStringList({"add", "blubb.txt"}));
    QVERIFY(waitForProcessSuccess(proc));
    proc.start(svnFilePath, QStringList({"commit", "-m", "blubb!"}));
    QVERIFY(waitForProcessSuccess(proc));
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    newRepoState = getRepoStateFromApp();
    QVERIFY(oldRepoState["repoState"] != newRepoState["repoState"]);
}

void TestBlackbox::vcsMercurial()
{
    const QString hgFilePath = findExecutable(QStringList("hg"));
    if (hgFilePath.isEmpty())
        QSKIP("hg not found");

    // Set up repo.
    QTemporaryDir repoDir;
    QVERIFY(repoDir.isValid());
    ccp(testDataDir + "/vcs", repoDir.path());
    QDir::setCurrent(repoDir.path());

    QProcess hg;
    hg.start(hgFilePath, QStringList{"init"});
    QVERIFY(waitForProcessSuccess(hg));

    const QStringList credentials{"--config", "ui.username=\"My Name <me@example.com>\""};

    QCOMPARE(runQbs({"resolve", {"-f", repoDir.path()}}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");

    // Run without hg metadata
    QbsRunParameters params("run", QStringList{"-f", repoDir.path()});
    params.workingDir = repoDir.path() + "/..";
    params.buildDirectory = repoDir.path();
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("generating my-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    QMap<QString, QByteArray> newRepoState = getRepoStateFromApp();
    QVERIFY(newRepoState.contains("repoState"));
    QVERIFY(newRepoState.contains("latestTag"));
    QVERIFY(newRepoState.contains("commitsSinceTag"));
    QVERIFY(newRepoState.contains("commitSha"));
    QCOMPARE(newRepoState["repoState"], QByteArray("none"));
    QCOMPARE(newRepoState["latestTag"], QByteArray("none"));
    QCOMPARE(newRepoState["commitsSinceTag"], QByteArray("none"));
    QCOMPARE(newRepoState["commitSha"], QByteArray("none"));
    QMap<QString, QByteArray> oldRepoState = newRepoState;

    // Initial commit
    hg.start(hgFilePath, QStringList{"add", "main.cpp"});
    QVERIFY(waitForProcessSuccess(hg));
    hg.start(hgFilePath, credentials + QStringList{"commit", "-m", "initial commit"});
    QVERIFY(waitForProcessSuccess(hg));

    // Run with hg metadata.
    WAIT_FOR_NEW_TIMESTAMP();
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("generating my-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    newRepoState = getRepoStateFromApp();
    QVERIFY(oldRepoState["repoState"] != newRepoState["repoState"]);
    QCOMPARE(newRepoState["latestTag"], QByteArray("null"));
    QCOMPARE(newRepoState["commitsSinceTag"], QByteArray("1"));
    QVERIFY(newRepoState["commitSha"] != QByteArray("none"));
    QVERIFY(newRepoState["commitSha"].startsWith(QByteArray("m")));
    oldRepoState = newRepoState;

    // Run with no changes.
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(!m_qbsStdout.contains("generating my-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    newRepoState = getRepoStateFromApp();
    QCOMPARE(oldRepoState, newRepoState);

    // Run with changed source file.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("main.cpp");
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(!m_qbsStdout.contains("generating my-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    newRepoState = getRepoStateFromApp();
    QCOMPARE(oldRepoState, newRepoState);

    // Add tag with dash
    WAIT_FOR_NEW_TIMESTAMP();
    hg.start(hgFilePath, credentials + QStringList{"tag", "v1.0.0-beta"});
    QVERIFY(waitForProcessSuccess(hg));
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("generating my-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    newRepoState = getRepoStateFromApp();
    QCOMPARE(newRepoState["latestTag"], QByteArray("v1.0.0-beta"));
    QCOMPARE(newRepoState["commitsSinceTag"], QByteArray("1"));
    QVERIFY(newRepoState["commitSha"] != QByteArray("none"));
    QVERIFY(newRepoState["commitSha"].startsWith(QByteArray("m")));
    QVERIFY(oldRepoState["repoState"] != newRepoState["repoState"]);
    oldRepoState = newRepoState;

    // Add new file to repo. Add new tag
    WAIT_FOR_NEW_TIMESTAMP();
    touch("blubb.txt");
    hg.start(hgFilePath, QStringList{"add", "blubb.txt"});
    QVERIFY(waitForProcessSuccess(hg));
    hg.start(hgFilePath, credentials + QStringList{"commit", "-m", "blubb!"});
    QVERIFY(waitForProcessSuccess(hg));
    hg.start(hgFilePath, credentials + QStringList{"tag", "v1.0.0"});
    QVERIFY(waitForProcessSuccess(hg));
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("generating my-repo-state.h"), m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStderr.constData());
    newRepoState = getRepoStateFromApp();
    QCOMPARE(newRepoState["latestTag"], QByteArray("v1.0.0"));
    QCOMPARE(newRepoState["commitsSinceTag"], QByteArray("1"));
    QVERIFY(newRepoState["commitSha"] != QByteArray("none"));
    QVERIFY(newRepoState["commitSha"].startsWith(QByteArray("m")));
    QVERIFY(oldRepoState["repoState"] != newRepoState["repoState"]);
}

void TestBlackbox::versionCheck()
{
    QDir::setCurrent(testDataDir + "/versioncheck");
    QFETCH(QString, requestedMinVersion);
    QFETCH(QString, requestedMaxVersion);
    QFETCH(QString, actualVersion);
    QFETCH(QString, errorMessage);
    QbsRunParameters params;
    params.expectFailure = !errorMessage.isEmpty();
    params.arguments << "-n"
                     << ("products.versioncheck.requestedMinVersion:'" + requestedMinVersion + "'")
                     << ("products.versioncheck.requestedMaxVersion:'" + requestedMaxVersion + "'")
                     << ("modules.lower.version:'" + actualVersion + "'");
    QCOMPARE(runQbs(params) == 0, errorMessage.isEmpty());
    if (params.expectFailure)
        QVERIFY2(QString(m_qbsStderr).contains(errorMessage), m_qbsStderr.constData());
}

void TestBlackbox::versionCheck_data()
{
    QTest::addColumn<QString>("requestedMinVersion");
    QTest::addColumn<QString>("requestedMaxVersion");
    QTest::addColumn<QString>("actualVersion");
    QTest::addColumn<QString>("errorMessage");

    QTest::newRow("ok1") << "1.0" << "1.1" << "1.0" << QString();
    QTest::newRow("ok2") << "1.0" << "2.0.1" << "2.0" << QString();
    QTest::newRow("ok3") << "1.0" << "2.5" << "1.5" << QString();
    QTest::newRow("ok3") << "1.0" << "2.0" << "1.99" << QString();
    QTest::newRow("bad1") << "2.0" << "2.1" << "1.5" << "needs to be at least";
    QTest::newRow("bad2") << "2.0" << "3.0" << "1.5" << "needs to be at least";
    QTest::newRow("bad3") << "2.0" << "3.0" << "3.5" << "needs to be lower than";
    QTest::newRow("bad4") << "2.0" << "3.0" << "3.0" << "needs to be lower than";

    // "bad" because the "higer" module has stronger requirements.
    QTest::newRow("bad5") << "0.1" << "0.9" << "0.5" << "Impossible version constraint";
}

void TestBlackbox::versionScript()
{
    QDir::setCurrent(testDataDir + "/versionscript");
    QCOMPARE(runQbs(QbsRunParameters("resolve", {"qbs.installRoot:" + QDir::currentPath()})), 0);
    const bool isLinuxGcc = m_qbsStdout.contains("is gcc for Linux: true");
    const bool isNotLinuxGcc = m_qbsStdout.contains("is gcc for Linux: false");
    if (isNotLinuxGcc)
        QSKIP("version script test only applies to Linux");
    QVERIFY(isLinuxGcc);
    QCOMPARE(runQbs(QbsRunParameters(QStringList("-q"))), 0);
    const QString output = QString::fromLocal8Bit(m_qbsStderr);
    const QRegularExpression pattern(QRegularExpression::anchoredPattern(".*---(.*)---.*"),
                                     QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = pattern.match(output);
    QVERIFY2(match.hasMatch(), qPrintable(output));
    QCOMPARE(pattern.captureCount(), 1);
    const QString nmPath = match.captured(1);
    if (!QFile::exists(nmPath))
        QSKIP("Cannot check for symbol presence: No nm found.");
    QProcess nm;
    nm.start(nmPath, QStringList(QDir::currentPath() + "/libversionscript.so"));
    QVERIFY(nm.waitForStarted());
    QVERIFY(nm.waitForFinished());
    const QByteArray allSymbols = nm.readAllStandardOutput();
    QCOMPARE(nm.exitCode(), 0);
    QVERIFY2(allSymbols.contains("dummyLocal"), allSymbols.constData());
    QVERIFY2(allSymbols.contains("dummyGlobal"), allSymbols.constData());
    nm.start(nmPath, QStringList() << "-g" << QDir::currentPath() + "/libversionscript.so");
    QVERIFY(nm.waitForStarted());
    QVERIFY(nm.waitForFinished());
    const QByteArray globalSymbols = nm.readAllStandardOutput();
    QCOMPARE(nm.exitCode(), 0);
    QVERIFY2(!globalSymbols.contains("dummyLocal"), allSymbols.constData());
    QVERIFY2(globalSymbols.contains("dummyGlobal"), allSymbols.constData());
}

void TestBlackbox::wholeArchive()
{
    QDir::setCurrent(testDataDir + "/whole-archive");
    QFETCH(QString, wholeArchiveString);
    QFETCH(bool, ruleInvalidationExpected);
    QFETCH(bool, dllLinkingExpected);
    const QbsRunParameters resolveParams("resolve",
            QStringList("products.dynamiclib.linkWholeArchive:" + wholeArchiveString));
    QCOMPARE(runQbs(QbsRunParameters(resolveParams)), 0);
    const bool linkerSupportsWholeArchive = m_qbsStdout.contains("can link whole archives");
    const bool linkerDoesNotSupportWholeArchive
            = m_qbsStdout.contains("cannot link whole archives");
    QVERIFY(linkerSupportsWholeArchive != linkerDoesNotSupportWholeArchive);
    if (m_qbsStdout.contains("is emscripten: true"))
        QSKIP("Irrelevant for emscripten");
    QVERIFY(m_qbsStdout.contains("is emscripten: false"));
    QCOMPARE(runQbs(QbsRunParameters(QStringList({ "-vvp", "dynamiclib" }))), 0);
    const bool wholeArchive = !wholeArchiveString.isEmpty();
    const bool outdatedVisualStudio = wholeArchive && linkerDoesNotSupportWholeArchive;
    const QByteArray invalidationOutput
            = "Value for property 'staticlib 1:cpp.linkWholeArchive' has changed.";
    if (!outdatedVisualStudio)
        QCOMPARE(m_qbsStderr.contains(invalidationOutput), ruleInvalidationExpected);
    QCOMPARE(m_qbsStdout.contains("linking"), dllLinkingExpected && !outdatedVisualStudio);
    QbsRunParameters buildParams(QStringList("-p"));
    buildParams.expectFailure = !wholeArchive || outdatedVisualStudio;
    buildParams.arguments << "app1";
    QCOMPARE(runQbs(QbsRunParameters(buildParams)) == 0, wholeArchive && !outdatedVisualStudio);
    buildParams.arguments.last() = "app2";
    QCOMPARE(runQbs(QbsRunParameters(buildParams)) == 0, wholeArchive && !outdatedVisualStudio);
    buildParams.arguments.last() = "app4";
    QCOMPARE(runQbs(QbsRunParameters(buildParams)) == 0, wholeArchive && !outdatedVisualStudio);
    buildParams.arguments.last() = "app3";
    buildParams.expectFailure = true;
    QVERIFY(runQbs(QbsRunParameters(buildParams)) != 0);
}

void TestBlackbox::wholeArchive_data()
{
    QTest::addColumn<QString>("wholeArchiveString");
    QTest::addColumn<bool>("ruleInvalidationExpected");
    QTest::addColumn<bool>("dllLinkingExpected");
    QTest::newRow("link normally") << QString() << false << true;
    QTest::newRow("link whole archive") << "true" << true << true;
    QTest::newRow("link whole archive again") << "notfalse" << false << false;
}

static bool symlinkExists(const QString &linkFilePath)
{
    return QFileInfo(linkFilePath).isSymLink();
}

void TestBlackbox::clean()
{
    QDir::setCurrent(testDataDir + "/clean");

    // Can't clean without a build graph.
    QbsRunParameters failParams("clean");
    failParams.expectFailure = true;
    QVERIFY(runQbs(failParams) != 0);

    // Default behavior: Remove all.
    QCOMPARE(runQbs(), 0);
    const QString objectSuffix = parsedObjectSuffix(m_qbsStdout);
    const QString appObjectFilePath = relativeProductBuildDir("app") + '/' + inputDirHash(".")
                                      + "/main.cpp" + objectSuffix;
    const QString appExeFilePath = relativeExecutableFilePath("app", m_qbsStdout);
    const QString depObjectFilePath = relativeProductBuildDir("dep") + '/' + inputDirHash(".")
                                      + "/dep.cpp" + objectSuffix;
    const QString depLibBase = relativeProductBuildDir("dep")
            + '/' + QBS_HOST_DYNAMICLIB_PREFIX + "dep";
    const bool isEmscripten = m_qbsStdout.contains("is emscripten: true");
    const bool isNotEmscripten = m_qbsStdout.contains("is emscripten: false");
    QCOMPARE(isEmscripten, !isNotEmscripten);
    QString depLibFilePath;
    QStringList symlinks;
    if (qbs::Internal::HostOsInfo::isMacosHost()) {
        depLibFilePath = depLibBase + ".1.1.0" + QBS_HOST_DYNAMICLIB_SUFFIX;
        symlinks << depLibBase + ".1.1" + QBS_HOST_DYNAMICLIB_SUFFIX
                 << depLibBase + ".1"  + QBS_HOST_DYNAMICLIB_SUFFIX
                 << depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX;
    } else if (qbs::Internal::HostOsInfo::isAnyUnixHost()) {
        depLibFilePath = depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX + ".1.1.0";
        if (!isEmscripten) {
            symlinks << depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX + ".1.1"
                     << depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX + ".1"
                     << depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX;
        }
    } else {
        depLibFilePath = depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX;
    }
    QVERIFY2(regularFileExists(appObjectFilePath), qPrintable(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QCOMPARE(runQbs(QbsRunParameters(QStringLiteral("clean"))), 0);
    QVERIFY(!QFile(appObjectFilePath).exists());
    QVERIFY(!QFile(appExeFilePath).exists());
    QVERIFY(!QFile(depObjectFilePath).exists());
    QVERIFY(!QFile(depLibFilePath).exists());
    for (const QString &symLink : std::as_const(symlinks))
        QVERIFY2(!symlinkExists(symLink), qPrintable(symLink));

    // Remove all, with a forced re-resolve in between.
    // This checks that rescuable artifacts are also removed.
    QCOMPARE(runQbs(QbsRunParameters("resolve",
                                     QStringList() << "modules.cpp.optimization:none")), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QCOMPARE(runQbs(QbsRunParameters("resolve",
                                     QStringList() << "modules.cpp.optimization:fast")), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QCOMPARE(runQbs(QbsRunParameters("clean")), 0);
    QVERIFY(!QFile(appObjectFilePath).exists());
    QVERIFY(!QFile(appExeFilePath).exists());
    QVERIFY(!QFile(depObjectFilePath).exists());
    QVERIFY(!QFile(depLibFilePath).exists());
    for (const QString &symLink : std::as_const(symlinks))
        QVERIFY2(!symlinkExists(symLink), qPrintable(symLink));

    // Dry run.
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QCOMPARE(runQbs(QbsRunParameters(QStringLiteral("clean"), QStringList("-n"))), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QVERIFY(regularFileExists(depObjectFilePath));
    QVERIFY(regularFileExists(depLibFilePath));
    for (const QString &symLink : std::as_const(symlinks))
        QVERIFY2(symlinkExists(symLink), qPrintable(symLink));

    // Product-wise, dependency only.
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QVERIFY(regularFileExists(depObjectFilePath));
    QVERIFY(regularFileExists(depLibFilePath));
    QCOMPARE(runQbs(QbsRunParameters(QStringLiteral("clean"), QStringList("-p") << "dep")), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QVERIFY(!QFile(depObjectFilePath).exists());
    QVERIFY(!QFile(depLibFilePath).exists());
    for (const QString &symLink : std::as_const(symlinks))
        QVERIFY2(!symlinkExists(symLink), qPrintable(symLink));

    // Product-wise, dependent product only.
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QVERIFY(regularFileExists(depObjectFilePath));
    QVERIFY(regularFileExists(depLibFilePath));
    QCOMPARE(runQbs(QbsRunParameters(QStringLiteral("clean"), QStringList("-p") << "app")), 0);
    QVERIFY(!QFile(appObjectFilePath).exists());
    QVERIFY(!QFile(appExeFilePath).exists());
    QVERIFY(regularFileExists(depObjectFilePath));
    QVERIFY(regularFileExists(depLibFilePath));
    for (const QString &symLink : std::as_const(symlinks))
        QVERIFY2(symlinkExists(symLink), qPrintable(symLink));
}

void TestBlackbox::conditionalExport()
{
    QDir::setCurrent(testDataDir + "/conditional-export");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("missing define"), m_qbsStderr.constData());

    params.expectFailure = false;
    params.arguments << "project.enableExport:true";
    params.command = "resolve";
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::conditionalFileTagger()
{
    QDir::setCurrent(testDataDir + "/conditional-filetagger");
    QbsRunParameters params(QStringList("products.theApp.enableTagger:false"));
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling"));
    params.arguments = QStringList("products.theApp.enableTagger:true");
    params.command = "resolve";
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling"));
}

void TestBlackbox::configure()
{
    QDir::setCurrent(testDataDir + "/configure");
    QCOMPARE(runQbs({"resolve"}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QbsRunParameters params;
    params.command = "run";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("Configured at"), m_qbsStdout.constData());
}

void TestBlackbox::conflictingArtifacts()
{
    QDir::setCurrent(testDataDir + "/conflicting-artifacts");
    QbsRunParameters params(QStringList() << "-n");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Conflicting artifacts"), m_qbsStderr.constData());
}

void TestBlackbox::cxxLanguageVersion()
{
    QDir::setCurrent(testDataDir + "/cxx-language-version");
    rmDirR(relativeBuildDir());
    QFETCH(QString, version);
    QFETCH(QVariantMap, requiredFlags);
    QFETCH(QVariantMap, forbiddenFlags);
    QbsRunParameters resolveParams;
    resolveParams.command = "resolve";
    resolveParams.arguments << "--force-probe-execution";
    resolveParams.arguments << "modules.cpp.useLanguageVersionFallback:true";
    if (!version.isEmpty())
        resolveParams.arguments << ("modules.cpp.cxxLanguageVersion:" + version);
    QCOMPARE(runQbs(resolveParams), 0);
    QString mapKey;
    if (version == "c++17" && m_qbsStdout.contains("is even newer MSVC: true"))
        mapKey = "msvc-brandnew";
    else if (m_qbsStdout.contains("is newer MSVC: true"))
        mapKey = "msvc-new";
    else if (m_qbsStdout.contains("is older MSVC: true"))
        mapKey = "msvc_old";
    else if (m_qbsStdout.contains("is GCC: true"))
        mapKey = "gcc";
    QVERIFY2(!mapKey.isEmpty(), m_qbsStdout.constData());
    QbsRunParameters buildParams;
    buildParams.expectFailure = mapKey == "gcc" && (version == "c++17" || version == "c++21");
    buildParams.arguments = QStringList({"--command-echo-mode", "command-line"});
    const int retVal = runQbs(buildParams);
    if (!buildParams.expectFailure)
        QCOMPARE(retVal, 0);
    const QString requiredFlag = requiredFlags.value(mapKey).toString();
    if (!requiredFlag.isEmpty())
        QVERIFY2(m_qbsStdout.contains(requiredFlag.toLocal8Bit()), m_qbsStdout.constData());
    const QString forbiddenFlag = forbiddenFlags.value(mapKey).toString();
    if (!forbiddenFlag.isEmpty())
        QVERIFY2(!m_qbsStdout.contains(forbiddenFlag.toLocal8Bit()), m_qbsStdout.constData());
}

void TestBlackbox::cxxLanguageVersion_data()
{
    QTest::addColumn<QString>("version");
    QTest::addColumn<QVariantMap>("requiredFlags");
    QTest::addColumn<QVariantMap>("forbiddenFlags");

    QTest::newRow("C++98")
            << QString("c++98")
            << QVariantMap({std::make_pair(QString("gcc"), QString("-std=c++98"))})
            << QVariantMap({std::make_pair(QString("msvc-old"), QString("/std:")),
                            std::make_pair(QString("msvc-new"), QString("/std:"))});
    QTest::newRow("C++11")
            << QString("c++11")
            << QVariantMap({std::make_pair(QString("gcc"), QString("-std=c++0x"))})
            << QVariantMap({std::make_pair(QString("msvc-old"), QString("/std:")),
                            std::make_pair(QString("msvc-new"), QString("/std:"))});
    QTest::newRow("C++14")
            << QString("c++14")
            << QVariantMap({std::make_pair(QString("gcc"), QString("-std=c++1y")),
                            std::make_pair(QString("msvc-new"), QString("/std:c++14"))
                           })
            << QVariantMap({std::make_pair(QString("msvc-old"), QString("/std:"))});
    QTest::newRow("C++17")
            << QString("c++17")
            << QVariantMap({std::make_pair(QString("gcc"), QString("-std=c++1z")),
                            std::make_pair(QString("msvc-new"), QString("/std:c++latest")),
                            std::make_pair(QString("msvc-brandnew"), QString("/std:c++17"))
                           })
            << QVariantMap({std::make_pair(QString("msvc-old"), QString("/std:"))});
    QTest::newRow("C++21")
            << QString("c++21")
            << QVariantMap({std::make_pair(QString("gcc"), QString("-std=c++21")),
                            std::make_pair(QString("msvc-new"), QString("/std:c++latest"))
                           })
            << QVariantMap({std::make_pair(QString("msvc-old"), QString("/std:"))});
    QTest::newRow("default")
            << QString()
            << QVariantMap()
            << QVariantMap({std::make_pair(QString("gcc"), QString("-std=")),
                            std::make_pair(QString("msvc-old"), QString("/std:")),
                            std::make_pair(QString("msvc-new"), QString("/std:"))});
}

void TestBlackbox::conanfileProbe_data()
{
    QTest::addColumn<bool>("forceFailure");

    QTest::newRow("success") << false;
    QTest::newRow("failure") << true;
}

void TestBlackbox::conanfileProbe()
{
    QFETCH(bool, forceFailure);

    QString executable = findExecutable({"conan"});
    if (executable.isEmpty())
        QSKIP("conan is not installed or not available in PATH.");

    const auto conanVersion = this->conanVersion(executable);
    if (!conanVersion.isValid())
        QSKIP("Can't get conan version.");
    if (compare(conanVersion, qbs::Version(2, 0)) >= 0)
        QSKIP("This test does not apply to conan 2.0 and newer.");

    // We first build a dummy package testlib and use that as dependency
    // in the testapp package.
    QDir::setCurrent(testDataDir + "/conanfile-probe/testlib");
    QStringList arguments{"create -o opt=True -s os=AIX . testlib/1.2.3@qbs/testing"};
    QProcess conan;
    conan.start(executable, arguments);
    QVERIFY(waitForProcessSuccess(conan));

    QDir::setCurrent(testDataDir + "/conanfile-probe/testapp");
    QCOMPARE(runQbs(QbsRunParameters("resolve",
                                     {"--force-probe-execution",
                                      QStringLiteral("projects.conanfile-probe-project.forceFailure:") +
                                      (forceFailure ? "true" : "false")})), forceFailure ? 1 : 0);

    QFile file(relativeBuildDir() + "/results.json");
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVariantMap actualResults = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    const auto generatedFilesPath = actualResults.take("generatedFilesPath").toString();
    // We want to make sure that generatedFilesPath is under the project directory,
    // but we don't care about the actual name.
    QVERIFY(directoryExists(relativeBuildDir() + "/genconan/"
                            + QFileInfo(generatedFilesPath).baseName()));

    const QVariantMap expectedResults = {
        { "json", "TESTLIB_ENV_VAL" },
        { "dependencies", QVariantList{"testlib1", "testlib2"} },
    };
    QCOMPARE(actualResults, expectedResults);
}

void TestBlackbox::conflictingPropertyValues_data()
{
    QTest::addColumn<bool>("overrideInProduct");
    QTest::newRow("don't override in product") << false;
    QTest::newRow("override in product") << true;
}

void TestBlackbox::conflictingPropertyValues()
{
    QFETCH(bool, overrideInProduct);

    QDir::setCurrent(testDataDir + "/conflicting-property-values");
    if (overrideInProduct)
        REPLACE_IN_FILE("conflicting-property-values.qbs", "// low.prop: name", "low.prop: name");
    else
        REPLACE_IN_FILE("conflicting-property-values.qbs", "low.prop: name", "// low.prop: name");
    WAIT_FOR_NEW_TIMESTAMP();
    QCOMPARE(runQbs(QString("resolve")), 0);
    if (overrideInProduct) {
        // Binding in product itself overrides everything else, module-level conflicts
        // are irrelevant.
        QVERIFY2(m_qbsStdout.contains("final prop value: toplevel"), m_qbsStdout.constData());
        QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    } else {
        // Only the conflicts in the highest-level modules are reported, lower-level conflicts
        // are irrelevant.
        // prop2 does not cause a conflict, because the values are the same.
        QVERIFY2(m_qbsStdout.contains("final prop value: highest"), m_qbsStdout.constData());
        QVERIFY2(m_qbsStderr.contains("Conflicting scalar values for property 'prop'"),
                 m_qbsStderr.constData());
        QVERIFY2(m_qbsStderr.count("values.qbs") == 2, m_qbsStderr.constData());
        QVERIFY2(m_qbsStderr.contains("values.qbs:20:23"), m_qbsStderr.constData());
        QVERIFY2(m_qbsStderr.contains("values.qbs:30:23"), m_qbsStderr.constData());
    }
}

void TestBlackbox::cpuFeatures()
{
    QDir::setCurrent(testDataDir + "/cpu-features");
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    const bool isX86 = m_qbsStdout.contains("is x86: true");
    const bool isX64 = m_qbsStdout.contains("is x64: true");
    if (!isX86 && !isX64) {
        QVERIFY2(m_qbsStdout.contains("is x86: false") && m_qbsStdout.contains("is x64: false"),
                 m_qbsStdout.constData());
        QSKIP("Not an x86 host");
    }
    const bool isGcc = m_qbsStdout.contains("is gcc: true");
    const bool isMsvc = m_qbsStdout.contains("is msvc: true");
    if (!isGcc && !isMsvc) {
        QVERIFY2(m_qbsStdout.contains("is gcc: false") && m_qbsStdout.contains("is msvc: false"),
                 m_qbsStdout.constData());
        QSKIP("Neither GCC nor MSVC");
    }
    QbsRunParameters params(QStringList{"--command-echo-mode", "command-line"});
    params.expectFailure = true;
    runQbs(params);
    if (isGcc) {
        QVERIFY2(m_qbsStdout.contains("-msse2") && m_qbsStdout.contains("-mavx")
                 && m_qbsStdout.contains("-mno-avx512f"), m_qbsStdout.constData());
    } else {
        QVERIFY2(m_qbsStdout.contains("/arch:AVX"), m_qbsStdout.constData());
        QVERIFY2(!m_qbsStdout.contains("/arch:AVX2"), m_qbsStdout.constData());
        QVERIFY2(m_qbsStdout.contains("/arch:SSE2") == isX86, m_qbsStdout.constData());
    }
}

void TestBlackbox::cxxModules_data()
{
    QTest::addColumn<QString>("projectDir");
    QTest::newRow("single-module") << "single-mod";
    QTest::newRow("dot-in-name") << "dot-in-name";
    QTest::newRow("export-import") << "export-import";
    QTest::newRow("import-std") << "import-std";
    QTest::newRow("import-std-compat") << "import-std-compat";
    QTest::newRow("dependent-modules") << "dep-mods";
    QTest::newRow("declaration-implementation") << "decl-impl";
    QTest::newRow("library-module") << "lib-mod";
    QTest::newRow("partitions") << "partitions";
    QTest::newRow("partitions-recursive") << "part-rec";
    QTest::newRow("partitions-dependency-on-module") << "part-depmod";
    QTest::newRow("partitions-library") << "part-lib";
}

void TestBlackbox::cxxModules()
{
    QFETCH(QString, projectDir);
    QDir::setCurrent(testDataDir + "/cxx-modules/" + projectDir);

    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(QbsRunParameters{"resolve"}), 0);
    if (m_qbsStdout.contains("Unsupported toolchainType"))
        QSKIP("Modules are not supported for this toolchain");

    QCOMPARE(runQbs(QbsRunParameters{"build"}), 0);
}

void TestBlackbox::cxxModulesChangesTracking()
{
    const auto checkContains = [this](const QStringList &files) {
        for (const auto &file : files) {
            if (!m_qbsStdout.contains(QByteArrayLiteral("compiling ") + file.toUtf8()))
                return false;
        }
        return true;
    };
    const auto checkNotContains = [this](const QStringList &files) {
        for (const auto &file : files) {
            if (m_qbsStdout.contains(QByteArrayLiteral("compiling ") + file.toUtf8()))
                return false;
        }
        return true;
    };
    QDir::setCurrent(testDataDir + "/cxx-modules/dep-mods");
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(QbsRunParameters{"resolve"}), 0);
    if (m_qbsStdout.contains("Unsupported toolchainType"))
        QSKIP("Modules are not supported for this toolchain");
    QbsRunParameters buildParams{"build"};
    QCOMPARE(runQbs(buildParams), 0);
    QVERIFY2(checkContains({"a.cppm", "b.cppm", "c.cppm", "main.cpp"}), m_qbsStdout.constData());

    WAIT_FOR_NEW_TIMESTAMP();
    touch("main.cpp");
    QCOMPARE(runQbs(buildParams), 0);
    QVERIFY2(checkContains({"main.cpp"}), m_qbsStdout.constData());
    QVERIFY2(checkNotContains({"a.cppm", "b.cppm", "c.cppm"}), m_qbsStdout.constData());

    WAIT_FOR_NEW_TIMESTAMP();
    touch("c.cppm");
    QCOMPARE(runQbs(buildParams), 0);
    QVERIFY2(checkContains({"c.cppm", "main.cpp"}), m_qbsStdout.constData());
    QVERIFY2(checkNotContains({"a.cppm", "b.cppm"}), m_qbsStdout.constData());

    WAIT_FOR_NEW_TIMESTAMP();
    touch("b.cppm");
    QCOMPARE(runQbs(buildParams), 0);
    QVERIFY2(checkContains({"b.cppm", "c.cppm", "main.cpp"}), m_qbsStdout.constData());
    QVERIFY2(checkNotContains({"a.cppm"}), m_qbsStdout.constData());

    WAIT_FOR_NEW_TIMESTAMP();
    touch("a.cppm");
    QCOMPARE(runQbs(buildParams), 0);
    QVERIFY2(checkContains({"a.cppm", "b.cppm", "c.cppm", "main.cpp"}), m_qbsStdout.constData());
}

void TestBlackbox::dateProperty()
{
    QDir::setCurrent(testDataDir + "/date-property");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("The stored date was 1999-12-31"), m_qbsStdout.constData());
}

void TestBlackbox::renameDependency()
{
    QDir::setCurrent(testDataDir + "/renameDependency");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDataDir + "/renameDependency/work");
    QCOMPARE(runQbs(), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    QFile::remove("lib.h");
    QFile::remove("lib.cpp");
    ccp("../after", ".");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
}

void TestBlackbox::separateDebugInfo()
{
    QDir::setCurrent(testDataDir + "/separate-debug-info");
    QCOMPARE(runQbs(QbsRunParameters(QStringList("qbs.debugInformation:true"))), 0);
    const bool isWindows = m_qbsStdout.contains("is windows: yes");
    const bool isNotWindows = m_qbsStdout.contains("is windows: no");
    QVERIFY(isWindows != isNotWindows);
    const bool isMacos = m_qbsStdout.contains("is macos: yes");
    const bool isNotMacos = m_qbsStdout.contains("is macos: no");
    QVERIFY(isMacos != isNotMacos);
    const bool isDarwin = m_qbsStdout.contains("is darwin: yes");
    const bool isNotDarwin = m_qbsStdout.contains("is darwin: no");
    QVERIFY(isDarwin != isNotDarwin);
    const bool isGcc = m_qbsStdout.contains("is gcc: yes");
    const bool isNotGcc = m_qbsStdout.contains("is gcc: no");
    QVERIFY(isGcc != isNotGcc);
    const bool isMsvc = m_qbsStdout.contains("is msvc: yes");
    const bool isNotMsvc = m_qbsStdout.contains("is msvc: no");
    QVERIFY(isMsvc != isNotMsvc);
    const bool isEmscripten = m_qbsStdout.contains("is emscripten: yes");
    const bool isNotEmscripten = m_qbsStdout.contains("is emscripten: no");
    QVERIFY(isEmscripten != isNotEmscripten);

    if (isDarwin) {
        QVERIFY(directoryExists(relativeProductBuildDir("app1") + "/app1.app.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("app1")
            + "/app1.app.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("app1")
            + "/app1.app.dSYM/Contents/Resources/DWARF/app1"));
        QCOMPARE(QDir(relativeProductBuildDir("app1")
            + "/app1.app.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(!QFile::exists(relativeProductBuildDir("app2") + "/app2.app.dSYM"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("app3") + "/app3.app.dSYM"));
        if (isMacos) {
            QVERIFY(regularFileExists(relativeProductBuildDir("app3")
                + "/app3.app/Contents/MacOS/app3.dwarf"));
        } else {
            QVERIFY(regularFileExists(relativeProductBuildDir("app3")
                + "/app3.app/app3.dwarf"));
        }
        QVERIFY(directoryExists(relativeProductBuildDir("app4") + "/app4.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("app4")
            + "/app4.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("app4")
            + "/app4.dSYM/Contents/Resources/DWARF/app4"));
        QCOMPARE(QDir(relativeProductBuildDir("app4")
            + "/app4.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(regularFileExists(relativeProductBuildDir("app5") + "/app5.dwarf"));
        QVERIFY(directoryExists(relativeProductBuildDir("foo1") + "/foo1.framework.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("foo1")
            + "/foo1.framework.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("foo1")
            + "/foo1.framework.dSYM/Contents/Resources/DWARF/foo1"));
        QCOMPARE(QDir(relativeProductBuildDir("foo1")
            + "/foo1.framework.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(!QFile::exists(relativeProductBuildDir("foo2") + "/foo2.framework.dSYM"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("foo3") + "/foo3.framework.dSYM"));
        if (isMacos) {
            QVERIFY(regularFileExists(relativeProductBuildDir("foo3")
                + "/foo3.framework/Versions/A/foo3.dwarf"));
        } else {
            QVERIFY(regularFileExists(relativeProductBuildDir("foo3")
                + "/foo3.framework/foo3.dwarf"));
        }
        QVERIFY(directoryExists(relativeProductBuildDir("foo4") + "/libfoo4.dylib.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("foo4")
            + "/libfoo4.dylib.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("foo4")
            + "/libfoo4.dylib.dSYM/Contents/Resources/DWARF/libfoo4.dylib"));
        QCOMPARE(QDir(relativeProductBuildDir("foo4")
            + "/libfoo4.dylib.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(regularFileExists(relativeProductBuildDir("foo5") + "/libfoo5.dylib.dwarf"));
        QVERIFY(directoryExists(relativeProductBuildDir("bar1") + "/bar1.bundle.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("bar1")
            + "/bar1.bundle.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("bar1")
            + "/bar1.bundle.dSYM/Contents/Resources/DWARF/bar1"));
        QCOMPARE(QDir(relativeProductBuildDir("bar1")
            + "/bar1.bundle.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(!QFile::exists(relativeProductBuildDir("bar2") + "/bar2.bundle.dSYM"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("bar3") + "/bar3.bundle.dSYM"));
        if (isMacos) {
            QVERIFY(regularFileExists(relativeProductBuildDir("bar3")
                + "/bar3.bundle/Contents/MacOS/bar3.dwarf"));
        } else {
            QVERIFY(regularFileExists(relativeProductBuildDir("bar3")
                + "/bar3.bundle/bar3.dwarf"));
        }
        QVERIFY(directoryExists(relativeProductBuildDir("bar4") + "/bar4.bundle.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("bar4")
            + "/bar4.bundle.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("bar4")
            + "/bar4.bundle.dSYM/Contents/Resources/DWARF/bar4.bundle"));
        QCOMPARE(QDir(relativeProductBuildDir("bar4")
            + "/bar4.bundle.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(regularFileExists(relativeProductBuildDir("bar5") + "/bar5.bundle.dwarf"));
    } else if (isGcc && isNotEmscripten) {
        const QString exeSuffix = isWindows ? ".exe" : "";
        const QString dllPrefix = isWindows ? "" : "lib";
        const QString dllSuffix = isWindows ? ".dll" : ".so";
        QVERIFY(QFile::exists(relativeProductBuildDir("app1") + "/app1" + exeSuffix + ".debug"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("app2") + "/app2" + exeSuffix + ".debug"));
        QVERIFY(QFile::exists(relativeProductBuildDir("foo1")
                              + '/' + dllPrefix + "foo1" + dllSuffix + ".debug"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("foo2")
                               + '/' + "foo2" + dllSuffix + ".debug"));
        QVERIFY(QFile::exists(relativeProductBuildDir("bar1")
                              + '/' + dllPrefix +  "bar1" + dllSuffix + ".debug"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("bar2")
                               + '/' + dllPrefix + "bar2" + dllSuffix + ".debug"));
    } else if (isEmscripten) {
        QVERIFY(QFile::exists(relativeProductBuildDir("app1") + "/app1.wasm.debug.wasm"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("app2") + "/app2.wasm.debug.wasm"));
    } else if (isMsvc) {
        QVERIFY(QFile::exists(relativeProductBuildDir("app1") + "/app1.pdb"));
        QVERIFY(QFile::exists(relativeProductBuildDir("foo1") + "/foo1.pdb"));
        QVERIFY(QFile::exists(relativeProductBuildDir("bar1") + "/bar1.pdb"));
        // MSVC's linker even creates a pdb file if /Z7 is passed to the compiler.
    } else {
        QSKIP("Unsupported toolchain. Skipping.");
    }
}

void TestBlackbox::trackAddFile()
{
    QList<QByteArray> output;
    QDir::setCurrent(testDataDir + "/trackAddFile");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDataDir + "/trackAddFile/work");
    QCOMPARE(runQbs({"resolve"}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    const QString objectSuffix = parsedObjectSuffix(m_qbsStdout);
    const QbsRunParameters runParams("run", QStringList{"-qp", "someapp"});
    QCOMPARE(runQbs(runParams), 0);

    output = m_qbsStdout.split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");
    QString unchangedObjectFile = relativeBuildDir() + "/someapp/narf.cpp" + objectSuffix;
    QDateTime unchangedObjectFileTime1 = QFileInfo(unchangedObjectFile).lastModified();

    WAIT_FOR_NEW_TIMESTAMP();
    ccp("../after", ".");
    touch("trackAddFile.qbs");
    touch("main.cpp");
    QCOMPARE(runQbs(runParams), 0);

    output = m_qbsStdout.split('\n');
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
    QVERIFY2(
        m_qbsStdout.contains("[trackExternalProductChanges] installing "), m_qbsStdout.constData());

    const bool isGcc = m_qbsStdout.contains("is gcc: true");
    const bool isNotGcc = m_qbsStdout.contains("is gcc: false");

    QbsRunParameters params;
    params.environment.insert("QBS_TEST_PULL_IN_FILE_VIA_ENV", "1");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));
    QVERIFY2(!m_qbsStdout.contains("Installing"), m_qbsStdout.constData());
    params.command = "resolve";
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY2(!m_qbsStdout.contains("compiling jsFileChange.cpp"), m_qbsStdout.constData());
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));
    QVERIFY2(
        m_qbsStdout.contains("[trackExternalProductChanges] installing "), m_qbsStdout.constData());

    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));
    QVERIFY2(
        m_qbsStdout.contains("[trackExternalProductChanges] installing "), m_qbsStdout.constData());

    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("fileList.js", "return []", "return ['jsFileChange.cpp']");
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));
    QVERIFY2(
        m_qbsStdout.contains("[trackExternalProductChanges] installing "), m_qbsStdout.constData());

    rmDirR(relativeBuildDir());
    REPLACE_IN_FILE("fileList.js", "['jsFileChange.cpp']", "[]");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));
    QVERIFY2(
        m_qbsStdout.contains("[trackExternalProductChanges] installing "), m_qbsStdout.constData());

    QFile cppFile("fileExists.cpp");
    QVERIFY(cppFile.open(QIODevice::WriteOnly));
    cppFile.write("void fileExists() { }\n");
    cppFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling fileExists.cpp"));
    QVERIFY2(
        m_qbsStdout.contains("[trackExternalProductChanges] installing "), m_qbsStdout.constData());

    if (isNotGcc)
        QSKIP("The remainder of this test requires a GCC-like toolchain");
    QVERIFY(isGcc);

    rmDirR(relativeBuildDir());
    params.environment = QbsRunParameters::defaultEnvironment();
    params.environment.insert("INCLUDE_PATH_TEST", "1");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("hiddenheaderqbs.h"), m_qbsStderr.constData());
    params.command = "resolve";
    params.environment.insert("CPLUS_INCLUDE_PATH",
                              QDir::toNativeSeparators(QDir::currentPath() + "/hidden"));
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::trackGroupConditionChange()
{
    QbsRunParameters params;
    params.expectFailure = true;
    QDir::setCurrent(testDataDir + "/group-condition-change");
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("jibbetnich"));

    params.command = "resolve";
    params.arguments = QStringList("project.kaputt:false");
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::trackRemoveFile()
{
    QDir::setCurrent(testDataDir + "/trackAddFile");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    ccp("after", "work");
    QDir::setCurrent(testDataDir + "/trackAddFile/work");
    QCOMPARE(runQbs({"resolve"}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    const QbsRunParameters runParams("run", QStringList{"-qp", "someapp"});
    QCOMPARE(runQbs(runParams), 0);
    const QString objectSuffix = parsedObjectSuffix(m_qbsStdout);
    QList<QByteArray> output = m_qbsStdout.split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "ZORT!");
    QString unchangedObjectFile = relativeBuildDir() + "/someapp/narf.cpp" + objectSuffix;
    QDateTime unchangedObjectFileTime1 = QFileInfo(unchangedObjectFile).lastModified();

    WAIT_FOR_NEW_TIMESTAMP();
    QFile::remove("trackAddFile.qbs");
    QFile::remove("main.cpp");
    QFile::copy("../before/trackAddFile.qbs", "trackAddFile.qbs");
    QFile::copy("../before/main.cpp", "main.cpp");
    QVERIFY(QFile::remove("zort.h"));
    QVERIFY(QFile::remove("zort.cpp"));
    QCOMPARE(runQbs(QbsRunParameters(QStringLiteral("resolve"))), 0);

    touch("main.cpp");
    touch("trackAddFile.qbs");
    QCOMPARE(runQbs(runParams), 0);
    output = m_qbsStdout.split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");

    // the object file of the untouched source should not have changed
    QDateTime unchangedObjectFileTime2 = QFileInfo(unchangedObjectFile).lastModified();
    QCOMPARE(unchangedObjectFileTime1, unchangedObjectFileTime2);

    // the object file for the removed cpp file should have vanished too
    QVERIFY(!regularFileExists(relativeBuildDir() + "/someapp/zort.cpp" + objectSuffix));
}

void TestBlackbox::trackAddFileTag()
{
    QList<QByteArray> output;
    QDir::setCurrent(testDataDir + "/trackFileTags");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDataDir + "/trackFileTags/work");
    QCOMPARE(runQbs({"resolve"}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    const QbsRunParameters runParams("run", QStringList{"-qp", "someapp"});
    QCOMPARE(runQbs(runParams), 0);
    output = m_qbsStdout.split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's no foo here");

    WAIT_FOR_NEW_TIMESTAMP();
    ccp("../after", ".");
    touch("main.cpp");
    touch("trackFileTags.qbs");
    waitForFileUnlock();
    QCOMPARE(runQbs(runParams), 0);
    output = m_qbsStdout.split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's 15 foo here");
}

void TestBlackbox::trackRemoveFileTag()
{
    QDir::setCurrent(testDataDir + "/trackFileTags");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("after", "work");
    QDir::setCurrent(testDataDir + "/trackFileTags/work");
    QCOMPARE(runQbs({"resolve"}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    const QString objectSuffix = parsedObjectSuffix(m_qbsStdout);
    const QbsRunParameters runParams("run", QStringList{"-qp", "someapp"});
    QCOMPARE(runQbs(runParams), 0);

    // check if the artifacts are here that will become stale in the 2nd step
    QVERIFY(regularFileExists(
        relativeProductBuildDir("someapp") + '/' + inputDirHash(".") + "/main_foo.cpp"
        + objectSuffix));
    QVERIFY(regularFileExists(relativeProductBuildDir("someapp") + "/main_foo.cpp"));
    QVERIFY(regularFileExists(relativeProductBuildDir("someapp") + "/main.foo"));
    QList<QByteArray> output = m_qbsStdout.split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's 15 foo here");

    WAIT_FOR_NEW_TIMESTAMP();
    ccp("../before", ".");
    touch("main.cpp");
    touch("trackFileTags.qbs");
    QCOMPARE(runQbs(runParams), 0);
    output = m_qbsStdout.split('\n');
    QVERIFY(!output.isEmpty());
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's no foo here");

    // check if stale artifacts have been removed
    QCOMPARE(
        regularFileExists(
            relativeProductBuildDir("someapp") + '/' + inputDirHash(".") + "/main_foo.cpp"
            + objectSuffix),
        false);
    QCOMPARE(regularFileExists(relativeProductBuildDir("someapp") + "/main_foo.cpp"), false);
    QCOMPARE(regularFileExists(relativeProductBuildDir("someapp") + "/main.foo"), false);
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

    WAIT_FOR_NEW_TIMESTAMP();
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

    WAIT_FOR_NEW_TIMESTAMP();
    QFile::remove("zoo.cpp");
    QFile::remove("product3.qbs");
    copyFileAndUpdateTimestamp("../before/trackProducts.qbs", "trackProducts.qbs");
    QCOMPARE(runQbs(params), 0);
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
    QCOMPARE(runQbs(QbsRunParameters("install")), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/pioniere.txt").exists());
    WAIT_FOR_NEW_TIMESTAMP();
    QFile::rename(QDir::currentPath() + "/pioniere.txt", QDir::currentPath() + "/fdj.txt");
    QCOMPARE(runQbs(QbsRunParameters(QStringLiteral("install"),
                                     QStringList("--clean-install-root"))), 0);
    QVERIFY(!QFileInfo(defaultInstallRoot + "/pioniere.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/fdj.txt").exists());
}

void TestBlackbox::recursiveRenaming()
{
    QDir::setCurrent(testDataDir + "/recursive_renaming");
    QCOMPARE(runQbs(QbsRunParameters("install")), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/wasser.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/subdir/blubb.txt").exists());
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(QFile::rename(QDir::currentPath() + "/dir/wasser.txt", QDir::currentPath() + "/dir/wein.txt"));
    QCOMPARE(runQbs(QbsRunParameters(QStringLiteral("install"),
                                     QStringList("--clean-install-root"))), 0);
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
    QFile outputFile(defaultInstallRoot + "/output.txt");
    QVERIFY2(outputFile.open(QIODevice::ReadOnly), qPrintable(outputFile.errorString()));
    QCOMPARE(outputFile.readAll(), QByteArray("file1.txtfile2.txt"));
    outputFile.close();
    WAIT_FOR_NEW_TIMESTAMP();
    QFile newFile("dir/subdir/file3.txt");
    QVERIFY2(newFile.open(QIODevice::WriteOnly), qPrintable(newFile.errorString()));
    newFile.close();
    QCOMPARE(runQbs(QbsRunParameters("install")), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/file3.txt").exists());
    QVERIFY2(outputFile.open(QIODevice::ReadOnly), qPrintable(outputFile.errorString()));
    QCOMPARE(outputFile.readAll(), QByteArray("file1.txtfile2.txtfile3.txt"));
    outputFile.close();
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY2(newFile.remove(), qPrintable(newFile.errorString()));
    QVERIFY2(!newFile.exists(), qPrintable(newFile.fileName()));
    QCOMPARE(runQbs(QbsRunParameters("install", QStringList{ "--clean-install-root"})), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/file1.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/file2.txt").exists());
    QVERIFY(!QFileInfo(defaultInstallRoot + "/dir/file3.txt").exists());
    QVERIFY2(outputFile.open(QIODevice::ReadOnly), qPrintable(outputFile.errorString()));
    QCOMPARE(outputFile.readAll(), QByteArray("file1.txtfile2.txt"));
}

void TestBlackbox::referenceErrorInExport()
{
    QDir::setCurrent(testDataDir + "/referenceErrorInExport");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(
        m_qbsStderr.contains("referenceErrorInExport.qbs:2:5 Error while handling product "
                             "'referenceErrorInExport':"),
        m_qbsStderr.constData());
    QVERIFY2(
        m_qbsStderr.contains("referenceErrorInExport.qbs:5:27 While evaluating here"),
        m_qbsStderr.constData());
    QVERIFY2(
        m_qbsStderr.contains("referenceErrorInExport.qbs:15:31 includePaths is not defined"),
        m_qbsStderr.constData());
}

void TestBlackbox::removeDuplicateLibraries_data()
{
    QTest::addColumn<bool>("removeDuplicates");
    QTest::newRow("remove duplicates") << true;
    QTest::newRow("don't remove duplicates") << false;
}

void TestBlackbox::removeDuplicateLibraries()
{
    QDir::setCurrent(testDataDir + "/remove-duplicate-libs");
    QFETCH(bool, removeDuplicates);
    const QbsRunParameters resolveParams("resolve", {"-f", "remove-duplicate-libs.qbs",
            "project.removeDuplicates:" + QString(removeDuplicates? "true" : "false")});
    QCOMPARE(runQbs(resolveParams), 0);
    const bool isBfd = m_qbsStdout.contains("is bfd linker: true");
    const bool isNotBfd = m_qbsStdout.contains("is bfd linker: false");
    QVERIFY2(isBfd != isNotBfd, m_qbsStdout.constData());
    QbsRunParameters buildParams("build");
    buildParams.expectFailure = removeDuplicates && isBfd;
    QCOMPARE(runQbs(buildParams) == 0, !buildParams.expectFailure);
}

void TestBlackbox::reproducibleBuild()
{
    QFETCH(bool, reproducible);

    QDir::setCurrent(testDataDir + "/reproducible-build");
    QbsRunParameters params("resolve");
    params.arguments << QString("modules.cpp.enableReproducibleBuilds:")
                        + (reproducible ? "true" : "false");
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(params), 0);
    const bool isGcc = m_qbsStdout.contains("is gcc: true");
    const bool isNotGcc = m_qbsStdout.contains("is gcc: false");
    const QString objectSuffix = parsedObjectSuffix(m_qbsStdout);
    if (isNotGcc)
        QSKIP("reproducible builds only supported for gcc");
    QVERIFY(isGcc);

    QCOMPARE(runQbs(), 0);
    QFile object(
        relativeProductBuildDir("the product") + '/' + inputDirHash(".") + '/' + "file1.cpp"
        + objectSuffix);
    QVERIFY2(object.open(QIODevice::ReadOnly), qPrintable(object.fileName()));
    const QByteArray oldContents = object.readAll();
    object.close();
    QCOMPARE(runQbs(QbsRunParameters("clean")), 0);
    QVERIFY(!object.exists());
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    if (reproducible) {
        QVERIFY(object.open(QIODevice::ReadOnly));
        const QByteArray newContents = object.readAll();
        QVERIFY(oldContents == newContents);
        object.close();
    }
    QCOMPARE(runQbs(QbsRunParameters("clean")), 0);
}

void TestBlackbox::reproducibleBuild_data()
{
    QTest::addColumn<bool>("reproducible");
    QTest::newRow("non-reproducible build") << false;
    QTest::newRow("reproducible build") << true;
}

void TestBlackbox::responseFiles()
{
    QDir::setCurrent(testDataDir + "/response-files");
    QCOMPARE(runQbs({"resolve"}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QbsRunParameters params;
    params.command = "install";
    params.arguments << "--install-root" << "installed";
    QCOMPARE(runQbs(params), 0);
    QFile file("installed/response-file-content.txt");
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QList<QByteArray> expected = QList<QByteArray>()
            << "foo" << qbs::Internal::shellQuote(QStringLiteral("with space")).toUtf8()
            << "bar" << "";
    QList<QByteArray> lines = file.readAll().split('\n');
    for (auto &line : lines)
        line = line.trimmed();
    QCOMPARE(lines, expected);
}

void TestBlackbox::retaggedOutputArtifact()
{
    QDir::setCurrent(testDataDir + "/retagged-output-artifact");
    QbsRunParameters resolveParams("resolve");
    resolveParams.arguments = QStringList("products.p.useTag1:true");
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(), 0);
    const QString a2 = relativeProductBuildDir("p") + "/a2.txt";
    const QString a3 = relativeProductBuildDir("p") + "/a3.txt";
    QVERIFY2(QFile::exists(a2), qPrintable(a2));
    QVERIFY2(!QFile::exists(a3), qPrintable(a3));
    resolveParams.arguments = QStringList("products.p.useTag1:false");
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!QFile::exists(a2), qPrintable(a2));
    QVERIFY2(QFile::exists(a3), qPrintable(a3));
    resolveParams.arguments = QStringList("products.p.useTag1:true");
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(QFile::exists(a2), qPrintable(a2));
    QVERIFY2(!QFile::exists(a3), qPrintable(a3));
}

void TestBlackbox::rpathlinkDeduplication()
{
    QDir::setCurrent(testDataDir + "/rpathlink-deduplication");
    QbsRunParameters resolveParams{"resolve"};
    QCOMPARE(runQbs(resolveParams), 0);
    const bool useRPathLink = m_qbsStdout.contains("useRPathLink: true");
    const bool dontUseRPathLink = m_qbsStdout.contains("useRPathLink: false");
    QVERIFY2(useRPathLink || dontUseRPathLink, m_qbsStdout);
    if (dontUseRPathLink)
        QSKIP("Only applies to toolchains that support rPathLink");
    const QString output = QString::fromLocal8Bit(m_qbsStdout);
    const QRegularExpression pattern(QRegularExpression::anchoredPattern(".*===(.*)===.*"),
                                     QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = pattern.match(output);
    QVERIFY2(match.hasMatch(), qPrintable(output));
    QCOMPARE(pattern.captureCount(), 1);
    const QString linkFlag = match.captured(1);

    QbsRunParameters buildParams;
    buildParams.arguments = QStringList({"--command-echo-mode", "command-line"});
    QCOMPARE(runQbs(buildParams), 0);
    // private DynamicLibraryA is a dependency for 2 other libs but should only appear once
    const auto libDir = QFileInfo(relativeProductBuildDir("DynamicLibraryA")).absoluteFilePath();
    QCOMPARE(m_qbsStdout.count((linkFlag + libDir).toUtf8()), 1);
}

void TestBlackbox::ruleConditions_data()
{
    QTest::addColumn<bool>("enableGroup");
    QTest::addColumn<bool>("enableRule");
    QTest::addColumn<bool>("enableTagger");
    QTest::addColumn<bool>("fileShouldExist");

    QTest::newRow("all off") << false << false << false << false;
    QTest::newRow("group off, rule off, tagger on") << false << false << true << false;
    QTest::newRow("group off, rule on, tagger off") << false << true << false << false;
    QTest::newRow("group off, rule on, tagger on") << false << true << true << false;
    QTest::newRow("group on, rule off, tagger off") << true << false << false << false;
    QTest::newRow("group on, rule off, tagger on") << true << false << true << false;
    QTest::newRow("group on, rule on, tagger off") << true << true << false << false;
    QTest::newRow("all on") << true << true << true << true;
}

void TestBlackbox::ruleConditions()
{
    QFETCH(bool, enableGroup);
    QFETCH(bool, enableRule);
    QFETCH(bool, enableTagger);
    QFETCH(bool, fileShouldExist);

    QDir::setCurrent(testDataDir + "/ruleConditions");
    rmDirR(relativeBuildDir());

    static const auto b2s = [](bool b) { return QString(b ? "true" : "false"); };
    const QStringList args{
        "modules.narfzort.enableGroup:" + b2s(enableGroup),
        "modules.narfzort.enableRule:" + b2s(enableRule),
        "modules.narfzort.enableTagger:" + b2s(enableTagger)};

    QCOMPARE(runQbs(args), 0);
    QVERIFY(QFileInfo(relativeExecutableFilePath("ruleConditions", m_qbsStdout)).exists());
    QCOMPARE(
        QFileInfo(relativeProductBuildDir("ruleConditions") + "/ruleConditions.foo.narf.zort")
            .exists(),
        fileShouldExist);
}

void TestBlackbox::ruleConnectionWithExcludedInputs()
{
    QDir::setCurrent(testDataDir + "/rule-connection-with-excluded-inputs");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("inputs.x: 2") && m_qbsStdout.contains("inputs.y: 0"),
             m_qbsStdout.constData());
}

void TestBlackbox::ruleCycle()
{
    QDir::setCurrent(testDataDir + "/ruleCycle");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("Cycle detected in rule dependencies"));
}

void TestBlackbox::ruleWithNoInputs()
{
    QDir::setCurrent(testDataDir + "/rule-with-no-inputs");
    QVERIFY2(runQbs() == 0, m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("running the rule"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("creating output"), m_qbsStdout.constData());
    QVERIFY2(runQbs() == 0, m_qbsStderr.constData());
    QVERIFY2(!m_qbsStdout.contains("running the rule"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("creating output"), m_qbsStdout.constData());
    QbsRunParameters params("resolve", QStringList() << "products.theProduct.version:1");
    QVERIFY2(runQbs(params) == 0, m_qbsStderr.constData());
    params.command = "build";
    QVERIFY2(runQbs(params) == 0, m_qbsStderr.constData());
    QVERIFY2(!m_qbsStdout.contains("running the rule"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("creating output"), m_qbsStdout.constData());
    params.command = "resolve";
    params.arguments = QStringList() << "products.theProduct.version:2";
    QVERIFY2(runQbs(params) == 0, m_qbsStderr.constData());
    params.command = "build";
    QVERIFY2(runQbs(params) == 0, m_qbsStderr.constData());
    QVERIFY2(!m_qbsStdout.contains("running the rule"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("creating output"), m_qbsStdout.constData());
    params.command = "resolve";
    params.arguments = QStringList() << "products.theProduct.version:2"
                                     << "products.theProduct.dummy:true";
    QVERIFY2(runQbs(params) == 0, m_qbsStderr.constData());
    params.command = "build";
    QVERIFY2(runQbs(params) == 0, m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("running the rule"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("creating output"), m_qbsStdout.constData());
}

void TestBlackbox::ruleWithNonRequiredInputs()
{
    QDir::setCurrent(testDataDir + "/rule-with-non-required-inputs");
    QbsRunParameters params("build", {"products.p.enableTagger:false"});
    QCOMPARE(runQbs(params), 0);
    QFile outFile(relativeProductBuildDir("p") + "/output.txt");
    QVERIFY2(outFile.open(QIODevice::ReadOnly), qPrintable(outFile.errorString()));
    QByteArray output = outFile.readAll();
    QCOMPARE(output, QByteArray("()"));
    outFile.close();
    params.command = "resolve";
    params.arguments = QStringList({"products.p.enableTagger:true"});
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(outFile.open(QIODevice::ReadOnly), qPrintable(outFile.errorString()));
    output = outFile.readAll();
    QCOMPARE(output, QByteArray("(a.inp,b.inp,c.inp,)"));
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("generating"), m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    touch("a.inp");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("generating"), m_qbsStdout.constData());
}

void TestBlackbox::runMultiplexed()
{
    QDir::setCurrent(testDataDir + "/run-multiplexed");
    QCOMPARE(runQbs({"resolve"}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");

    QbsRunParameters params("run");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    params.arguments = QStringList{"-p", "app"};
    QVERIFY(runQbs(params) != 0);
    params.expectFailure = false;
    params.arguments.last() = "app {\"buildVariant\":\"debug\"}";
    QCOMPARE(runQbs(params), 0);
    params.arguments.last() = "app {\"buildVariant\":\"release\"}";
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::sanitizer_data()
{
    QTest::addColumn<QString>("sanitizer");
    QTest::newRow("none") << QString();
    QTest::newRow("address") << QStringLiteral("address");
    QTest::newRow("undefined") << QStringLiteral("undefined");
    QTest::newRow("thread") << QStringLiteral("thread");
}

void TestBlackbox::sanitizer()
{
    QFETCH(QString, sanitizer);
    QDir::setCurrent(testDataDir + "/sanitizer");
    rmDirR(relativeBuildDir());
    QbsRunParameters params("build", {"--command-echo-mode", "command-line"});
    if (!sanitizer.isEmpty()) {
        params.arguments.append(
                {QStringLiteral("products.sanitizer.sanitizer:\"") + sanitizer + "\""});
    }
    QCOMPARE(runQbs(params), 0);
    if (m_qbsStdout.contains(QByteArrayLiteral("Compiler does not support sanitizer")))
        QSKIP("Compiler does not support the specified sanitizer");
    if (!sanitizer.isEmpty()) {
        QVERIFY2(m_qbsStdout.contains(QByteArrayLiteral("fsanitize=") + sanitizer.toLatin1()),
                 qPrintable(m_qbsStdout));
    } else {
        QVERIFY2(!m_qbsStdout.contains(QByteArrayLiteral("fsanitize=")), qPrintable(m_qbsStdout));
    }
}

void TestBlackbox::scannerItem_data()
{
    QTest::addColumn<bool>("inProduct");
    QTest::addColumn<bool>("inModule");
    QTest::addColumn<bool>("enableGroup");
    QTest::addColumn<bool>("successExpected");

    QTest::newRow("all off") << false << false << false << false;
    QTest::newRow("product scanner") << true << false << false << true;
    QTest::newRow("module scanner, group off") << false << true << false << false;
    QTest::newRow("module scanner, group on") << false << true << true << true;
}

void TestBlackbox::scannerItem()
{
    QFETCH(bool, inProduct);
    QFETCH(bool, inModule);
    QFETCH(bool, enableGroup);
    QFETCH(bool, successExpected);

    static const auto b2s = [](bool b) { return QString(b ? "true" : "false"); };

    QDir::setCurrent(testDataDir + "/scanner-item");
    rmDirR(relativeBuildDir());

    const QbsRunParameters params(
        {"-f",
         "scanner-item.qbs",
         "products.scanner-item.productScanner:" + b2s(inProduct),
         "products.scanner-item.moduleScanner:" + b2s(inModule),
         "products.scanner-item.enableGroup:" + b2s(enableGroup)});
    QCOMPARE(runQbs(params), 0);

    QVERIFY2(m_qbsStdout.contains("handling file1.in"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("handling file2.in"), m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    touch("subdir1/file.inc");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStdout.contains("handling file1.in"), successExpected);
    QVERIFY2(!m_qbsStdout.contains("handling file2.in"), m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    touch("subdir2/file.inc");
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(!m_qbsStdout.contains("handling file1.in"), m_qbsStdout.constData());
    QCOMPARE(m_qbsStdout.contains("handling file2.in"), successExpected);
}

void TestBlackbox::scanResultInOtherProduct()
{
    QDir::setCurrent(testDataDir + "/scan-result-in-other-product");
    QCOMPARE(runQbs(QStringList("-vv")), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("generating text file"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStderr.contains("The file dependency might get lost during change tracking"),
             m_qbsStderr.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("other/other.qbs", "blubb", "blubb2");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("generating text file"), m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    touch("lib/lib.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("generating text file"), m_qbsStdout.constData());
}

void TestBlackbox::scanResultInNonDependency()
{
    QDir::setCurrent(testDataDir + "/scan-result-in-non-dependency");
    QCOMPARE(runQbs(QStringList("-vv")), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("generating text file"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStderr.contains("The file dependency might get lost during change tracking"),
             m_qbsStderr.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("other/other.qbs", "blubb", "blubb2");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("generating text file"), m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    touch("lib/lib.h");
    QCOMPARE(runQbs(), 0);
    QEXPECT_FAIL("", "QBS-1532", Continue);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("generating text file"), m_qbsStdout.constData());
}

void TestBlackbox::setupBuildEnvironment()
{
    QDir::setCurrent(testDataDir + "/setup-build-environment");
    QCOMPARE(runQbs(), 0);
    QFile f(relativeProductBuildDir("first_product") + QLatin1String("/m.output"));
    QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.errorString()));
    QCOMPARE(f.readAll().trimmed(), QByteArray("1"));
    f.close();
    f.setFileName(relativeProductBuildDir("second_product") + QLatin1String("/m.output"));
    QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.errorString()));
    QCOMPARE(f.readAll().trimmed(), QByteArray());
}

void TestBlackbox::setupRunEnvironment()
{
    QDir::setCurrent(testDataDir + "/setup-run-environment");
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    const QString appFilePath = QDir::currentPath() + "/dryrun/"
                                + relativeExecutableFilePath("app", m_qbsStdout);
    QbsRunParameters failParams("run", QStringList({"--setup-run-env-config",
                                                    "ignore-lib-dependencies"}));
    failParams.expectFailure = true;
    failParams.expectCrash = m_qbsStdout.contains("is windows");
    QVERIFY(runQbs(QbsRunParameters(failParams)) != 0);
    QVERIFY2(failParams.expectCrash || m_qbsStderr.contains("lib"), m_qbsStderr.constData());
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QbsRunParameters dryRunParams("run", QStringList("--dry-run"));
    dryRunParams.buildDirectory = "dryrun";
    QCOMPARE(runQbs(dryRunParams), 0);
    QVERIFY2(m_qbsStdout.contains("Would start target")
             && m_qbsStdout.contains(QDir::toNativeSeparators(appFilePath).toLocal8Bit()),
             m_qbsStdout.constData());
}

void TestBlackbox::staticLibDeps()
{
    QFETCH(bool, withExport);
    QFETCH(bool, importPrivateLibraries);
    QFETCH(bool, successExpected);

    QDir::setCurrent(testDataDir + "/static-lib-deps");
    rmDirR(relativeBuildDir());

    QStringList args{
        QStringLiteral("project.useExport:%1").arg(withExport ? "true" : "false"),
        QStringLiteral("modules.cpp.importPrivateLibraries:%1")
            .arg(importPrivateLibraries ? "true" : "false")};
    QbsRunParameters params(args);
    params.expectFailure = !successExpected;
    QCOMPARE(runQbs(params) == 0, successExpected);
}

void TestBlackbox::staticLibDeps_data()
{
    QTest::addColumn<bool>("withExport");
    QTest::addColumn<bool>("importPrivateLibraries");
    QTest::addColumn<bool>("successExpected");

    QTest::newRow("no Export, with import") << false << true << true;
    QTest::newRow("no Export, no import") << false << false << false;
    QTest::newRow("with Export, with import") << true << true << true;
    QTest::newRow("with Export, no import") << true << false << true;
}

void TestBlackbox::smartRelinking()
{
    QDir::setCurrent(testDataDir + "/smart-relinking");
    rmDirR(relativeBuildDir());
    QFETCH(bool, strictChecking);
    QbsRunParameters params(QStringList() << (QString("modules.cpp.exportedSymbolsCheckMode:%1")
            .arg(strictChecking ? "strict" : "ignore-undefined")));
    QCOMPARE(runQbs(params), 0);
    if (m_qbsStdout.contains("project disabled"))
        QSKIP("Test does not apply on this target");
    QVERIFY2(m_qbsStdout.contains("linking lib"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("linking app"), m_qbsStdout.constData());

    // Irrelevant change.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("lib.cpp");
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("linking lib"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("linking app"), m_qbsStdout.constData());

    // Add new private symbol.
    WAIT_FOR_NEW_TIMESTAMP();
    params.command = "resolve";
    params.arguments << "products.lib.defines:PRIV2";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("linking lib"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("linking app"), m_qbsStdout.constData());

    // Remove private symbol.
    WAIT_FOR_NEW_TIMESTAMP();
    params.arguments.removeLast();
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("linking lib"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("linking app"), m_qbsStdout.constData());

    // Add new public symbol.
    WAIT_FOR_NEW_TIMESTAMP();
    params.arguments << "products.lib.defines:PUB2";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("linking lib"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("linking app"), m_qbsStdout.constData());

    // Remove public symbol.
    WAIT_FOR_NEW_TIMESTAMP();
    params.arguments.removeLast();
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("linking lib"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("linking app"), m_qbsStdout.constData());

    // Add new undefined symbol.
    WAIT_FOR_NEW_TIMESTAMP();
    params.arguments << "products.lib.defines:PRINTF";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("linking lib"), m_qbsStdout.constData());
    QVERIFY2(strictChecking == m_qbsStdout.contains("linking app"), m_qbsStdout.constData());

    // Remove undefined symbol.
    WAIT_FOR_NEW_TIMESTAMP();
    params.arguments.removeLast();
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("linking lib"), m_qbsStdout.constData());
    QVERIFY2(strictChecking == m_qbsStdout.contains("linking app"), m_qbsStdout.constData());
}

void TestBlackbox::smartRelinking_data()
{
    QTest::addColumn<bool>("strictChecking");
    QTest::newRow("strict checking") << true;
    QTest::newRow("ignore undefined") << false;
}


static QString soName(const QString &readElfPath, const QString &libFilePath)
{
    QProcess readElf;
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C")); // force readelf to use US encoding
    readElf.setProcessEnvironment(env);
    readElf.start(readElfPath, QStringList() << "-a" << libFilePath);
    if (!readElf.waitForStarted() || !readElf.waitForFinished() || readElf.exitCode() != 0) {
        qDebug() << readElf.errorString() << readElf.readAllStandardError();
        return {};
    }
    const QByteArray output = readElf.readAllStandardOutput();
    const QByteArray magicString = "Library soname: [";
    const int magicStringIndex = output.indexOf(magicString);
    if (magicStringIndex == -1)
        return {};
    const int endIndex = output.indexOf(']', magicStringIndex);
    if (endIndex == -1)
        return {};
    const int nameIndex = magicStringIndex + magicString.size();
    const QByteArray theName = output.mid(nameIndex, endIndex - nameIndex);
    return QString::fromLatin1(theName);
}

void TestBlackbox::soVersion()
{
    const QString readElfPath = findExecutable(QStringList("readelf"));
    if (readElfPath.isEmpty() || readElfPath.endsWith("exe"))
        QSKIP("soversion test not applicable on this system");
    QDir::setCurrent(testDataDir + "/soversion");

    QFETCH(QString, soVersion);
    QFETCH(bool, useVersion);
    QFETCH(QString, expectedSoName);

    QbsRunParameters params;
    params.arguments << ("products.mylib.useVersion:" + QString((useVersion ? "true" : "false")));
    if (!soVersion.isNull())
        params.arguments << ("modules.cpp.soVersion:" + soVersion);
    const QString libFilePath = relativeProductBuildDir("mylib") + "/libmylib.so"
            + (useVersion ? ".1.2.3" : QString());
    rmDirR(relativeBuildDir());
    const bool success = runQbs(params) == 0;
    QVERIFY(success);
    QVERIFY2(regularFileExists(libFilePath), qPrintable(libFilePath));
    if (m_qbsStdout.contains("is emscripten: true"))
        QEXPECT_FAIL(nullptr, "Emscripten does not produce dynamic libraries of elf format", Abort);
    QCOMPARE(soName(readElfPath, libFilePath), expectedSoName);
}

void TestBlackbox::soVersion_data()
{
    QTest::addColumn<QString>("soVersion");
    QTest::addColumn<bool>("useVersion");
    QTest::addColumn<QString>("expectedSoName");

    QTest::newRow("default") << QString() << true << QString("libmylib.so.1");
    QTest::newRow("explicit soVersion") << QString("1.2") << true << QString("libmylib.so.1.2");
    QTest::newRow("empty soVersion") << QString("") << true << QString("libmylib.so.1.2.3");
    QTest::newRow("no version, explicit soVersion") << QString("5") << false
                                                    << QString("libmylib.so.5");
    QTest::newRow("no version, default soVersion") << QString() << false << QString("libmylib.so");
    QTest::newRow("no version, empty soVersion") << QString("") << false << QString("libmylib.so");
}

void TestBlackbox::sourceArtifactChanges()
{
    QDir::setCurrent(testDataDir + "/source-artifact-changes");
    bool useCustomFileTags = false;
    bool overrideFileTags = true;
    bool filesAreTargets = false;
    bool useCxx11 = false;
    static const auto b2s = [](bool b) { return QString(b ? "true" : "false"); };
    const auto resolveParams = [&useCustomFileTags, &overrideFileTags, &filesAreTargets, &useCxx11] {
        return QbsRunParameters("resolve", QStringList{
            "modules.module_with_files.overrideTags:" + b2s(overrideFileTags),
            "modules.module_with_files.filesAreTargets:" + b2s(filesAreTargets),
            "modules.module_with_files.fileTags:" + QString(useCustomFileTags ? "custom" : "cpp"),
            "modules.cpp.cxxLanguageVersion:" + QString(useCxx11 ? "c++11" : "c++98")
        });
    };
#define VERIFY_COMPILATION(expected) \
    do { \
        QVERIFY2(m_qbsStdout.contains("compiling main.cpp") == expected, m_qbsStdout.constData()); \
        QVERIFY2(QFile::exists(appFilePath) == expected, qPrintable(appFilePath)); \
        if (expected) \
            QVERIFY2(m_qbsStdout.contains("cpp artifacts: 1"), m_qbsStdout.constData()); \
        else \
            QVERIFY2(m_qbsStdout.contains("cpp artifacts: 0"), m_qbsStdout.constData()); \
    } while (false)

    // Initial build.
    QCOMPARE(runQbs(resolveParams()), 0);
    QVERIFY2(m_qbsStdout.contains("is gcc: "), m_qbsStdout.constData());
    const bool isGcc = m_qbsStdout.contains("is gcc: true");
    const QString appFilePath = QDir::currentPath() + '/'
                                + relativeExecutableFilePath("app", m_qbsStdout);
    QCOMPARE(runQbs(), 0);
    VERIFY_COMPILATION(true);

    // Overwrite the file tags. Now the source file is no longer tagged "cpp" and nothing
    // should get built.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("modules/module_with_files/main.cpp");
    useCustomFileTags = true;
    QCOMPARE(runQbs(resolveParams()), 0);
    QCOMPARE(runQbs(), 0);
    VERIFY_COMPILATION(false);

    // Now the custom file tag exists in addition to "cpp", and the app should get built again.
    overrideFileTags = false;
    QCOMPARE(runQbs(resolveParams()), 0);
    QCOMPARE(runQbs(), 0);
    VERIFY_COMPILATION(true);

    // Mark the cpp file as a module target. Now it will no longer be considered an input
    // by the compiler rule, and nothing should get built.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("modules/module_with_files/main.cpp");
    filesAreTargets = true;
    QCOMPARE(runQbs(resolveParams()), 0);
    QCOMPARE(runQbs(), 0);
    VERIFY_COMPILATION(false);

    // Now just revert the last change.
    filesAreTargets = false;
    QCOMPARE(runQbs(resolveParams()), 0);
    QCOMPARE(runQbs(), 0);
    VERIFY_COMPILATION(true);

    // Change a relevant cpp property. A rebuild is expected.
    useCxx11 = true;
    QCOMPARE(runQbs(resolveParams()), 0);
    QCOMPARE(runQbs(QStringList({"--command-echo-mode", "command-line"})), 0);
    if (isGcc) {
        QVERIFY2(m_qbsStdout.contains("-std=c++11") || m_qbsStdout.contains("-std=c++0x"),
                 m_qbsStdout.constData());
    }

#undef VERIFY_COMPILATION
}

void TestBlackbox::overrideProjectProperties()
{
    QDir::setCurrent(testDataDir + "/overrideProjectProperties");
    QCOMPARE(runQbs(QbsRunParameters(QStringList()
                                     << QStringLiteral("-f")
                                     << QStringLiteral("overrideProjectProperties.qbs")
                                     << QStringLiteral("project.nameSuffix:ForYou")
                                     << QStringLiteral("project.someBool:false")
                                     << QStringLiteral("project.someInt:156")
                                     << QStringLiteral("project.someStringList:one")
                                     << QStringLiteral("products.MyAppForYou.mainFile:main.cpp"))),
             0);
    QVERIFY(regularFileExists(relativeExecutableFilePath("MyAppForYou", m_qbsStdout)));
    QVERIFY(QFile::remove(relativeBuildGraphFilePath()));
    QbsRunParameters params;
    params.arguments << QStringLiteral("-f") << QStringLiteral("project_using_helper_lib.qbs");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);

    rmDirR(relativeBuildDir());
    params.arguments = QStringList() << QStringLiteral("-f")
            << QStringLiteral("project_using_helper_lib.qbs")
            << QStringLiteral("project.linkSuccessfully:true");
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::partiallyBuiltDependency_data()
{
    QTest::addColumn<QByteArray>("mode");
    QTest::addColumn<bool>("expectBuilding");

    QTest::newRow("default") << QByteArray("default") << true;
    QTest::newRow("minimal") << QByteArray("minimal") << false;
    QTest::newRow("full") << QByteArray("full") << true;
}

void TestBlackbox::partiallyBuiltDependency()
{
    QFETCH(QByteArray, mode);
    QFETCH(bool, expectBuilding);

    QDir::setCurrent(testDataDir + "/partially-built-dependency");
    rmDirR(relativeBuildDir());
    QbsRunParameters params({"-p", "p"});
    if (mode == "minimal")
        params.arguments << "project.minimalDependency:true";
    else if (mode == "full")
        params.arguments << "project.minimalDependency:false";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStdout.count("generating main.cpp"), 1);
    QCOMPARE(m_qbsStdout.count("copying main.cpp"), 1);
    QCOMPARE(m_qbsStdout.count("compiling main.cpp"), expectBuilding ? 2 : 1);
    QVERIFY2(m_qbsStdout.contains("linking") == expectBuilding, m_qbsStdout.constData());
}

void TestBlackbox::pathProbe_data()
{
    QTest::addColumn<QString>("projectFile");
    QTest::addColumn<bool>("successExpected");
    QTest::newRow("non-existent") << QString("non-existent.qbs") << false;
    QTest::newRow("non-existent-selector.qbs") << QString("non-existent-selector.qbs") << false;
    QTest::newRow("single-file") << QString("single-file.qbs") << true;
    QTest::newRow("single-file-selector") << QString("single-file-selector.qbs") << true;
    QTest::newRow("single-file-selector-array") << QString("single-file-selector-array.qbs") << true;
    QTest::newRow("single-file-mult-variants") << QString("single-file-mult-variants.qbs") << true;
    QTest::newRow("mult-files") << QString("mult-files.qbs") << true;
    QTest::newRow("mult-files-mult-variants") << QString("mult-files-mult-variants.qbs") << true;
    QTest::newRow("single-file-suffixes") << QString("single-file-suffixes.qbs") << true;
    QTest::newRow("mult-files-suffixes") << QString("mult-files-suffixes.qbs") << true;
    QTest::newRow("mult-files-common-suffixes") << QString("mult-files-common-suffixes.qbs") << true;
    QTest::newRow("mult-files-mult-suffixes") << QString("mult-files-mult-suffixes.qbs") << true;
    QTest::newRow("name-filter") << QString("name-filter.qbs") << true;
    QTest::newRow("candidate-filter") << QString("candidate-filter.qbs") << true;
    QTest::newRow("environment-paths") << QString("environment-paths.qbs") << true;
}

void TestBlackbox::pathProbe()
{
    QDir::setCurrent(testDataDir + "/path-probe");
    QFETCH(QString, projectFile);
    QFETCH(bool, successExpected);
    rmDirR(relativeBuildDir());

    QbsRunParameters buildParams("build", QStringList{"-f", projectFile});
    buildParams.expectFailure = !successExpected;
    buildParams.environment.insert("SEARCH_PATH", "usr/bin");
    QCOMPARE(runQbs(buildParams) == 0, successExpected);
    if (!successExpected)
        QVERIFY2(m_qbsStderr.contains("Probe failed to find files"), m_qbsStderr);
}

void TestBlackbox::pathListInProbe()
{
    QDir::setCurrent(testDataDir + "/path-list-in-probe");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::pchChangeTracking()
{
    QDir::setCurrent(testDataDir + "/pch-change-tracking");
    bool success = runQbs() == 0;
    if (!success && m_qbsStderr.contains("mingw32_gt_pch_use_address"))
        QSKIP("https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91440");
    QVERIFY(success);
    QVERIFY(m_qbsStdout.contains("precompiling pch.h (cpp)"));
    WAIT_FOR_NEW_TIMESTAMP();
    touch("header1.h");
    success = runQbs() == 0;
    if (!success && m_qbsStderr.contains("mingw32_gt_pch_use_address"))
        QSKIP("https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91440");
    QVERIFY(success);
    QVERIFY(m_qbsStdout.contains("precompiling pch.h (cpp)"));
    QVERIFY(m_qbsStdout.contains("compiling header2.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    WAIT_FOR_NEW_TIMESTAMP();
    touch("header2.h");
    success = runQbs() == 0;
    if (!success && m_qbsStderr.contains("mingw32_gt_pch_use_address"))
        QSKIP("https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91440");
    QVERIFY(success);
    QVERIFY2(!m_qbsStdout.contains("precompiling pch.h (cpp)"), m_qbsStdout.constData());
}

void TestBlackbox::perGroupDefineInExportItem()
{
    QDir::setCurrent(testDataDir + "/per-group-define-in-export-item");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::pkgConfigProbe()
{
    const QString exe = findExecutable(QStringList() << "pkg-config");
    if (exe.isEmpty())
        QSKIP("This test requires the pkg-config tool");

    QDir::setCurrent(testDataDir + "/pkg-config-probe");

    QFETCH(QString, packageBaseName);
    QFETCH(QStringList, found);
    QFETCH(QStringList, libs);
    QFETCH(QStringList, cflags);
    QFETCH(QStringList, version);

    rmDirR(relativeBuildDir());
    QbsRunParameters params(QStringList() << ("project.packageBaseName:" + packageBaseName));
    QCOMPARE(runQbs(params), 0);
    const QString stdOut = m_qbsStdout;
    QVERIFY2(stdOut.contains("theProduct1 found: " + found.at(0)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct2 found: " + found.at(1)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct1 libs: " + libs.at(0)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct2 libs: " + libs.at(1)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct1 cflags: " + cflags.at(0)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct2 cflags: " + cflags.at(1)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct1 version: " + version.at(0)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct2 version: " + version.at(1)), m_qbsStdout.constData());
}

void TestBlackbox::pkgConfigProbe_data()
{
    QTest::addColumn<QString>("packageBaseName");
    QTest::addColumn<QStringList>("found");
    QTest::addColumn<QStringList>("libs");
    QTest::addColumn<QStringList>("cflags");
    QTest::addColumn<QStringList>("version");

    QTest::newRow("existing package")
            << "dummy" << (QStringList() << "true" << "true")
            << (QStringList() << "[\"-Ldummydir1\",\"-ldummy1\"]"
                << "[\"-Ldummydir2\",\"-ldummy2\"]")
            << (QStringList() << "[]" << "[]") << (QStringList() << "0.0.1" << "0.0.2");

    // Note: The array values should be "undefined", but we lose that information when
    //       converting to QVariants in the ProjectResolver.
    QTest::newRow("non-existing package")
            << "blubb" << (QStringList() << "false" << "false") << (QStringList() << "[]" << "[]")
            << (QStringList() << "[]" << "[]") << (QStringList() << "undefined" << "undefined");
}

void TestBlackbox::pkgConfigProbeSysroot()
{
    const QString exe = findExecutable(QStringList() << "pkg-config");
    if (exe.isEmpty())
        QSKIP("This test requires the pkg-config tool");

    QDir::setCurrent(testDataDir + "/pkg-config-probe-sysroot");
    QCOMPARE(runQbs(QStringList("-v")), 0);
    QCOMPARE(m_qbsStderr.count("PkgConfigProbe: found packages"), 2);
    const QString outputTemplate = "theProduct%1 libs: [\"-L%2/usr/dummy\",\"-ldummy1\"]";
    QVERIFY2(m_qbsStdout.contains(outputTemplate
                                  .arg("1", QDir::currentPath() + "/sysroot1").toLocal8Bit()),
             m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains(outputTemplate
                                  .arg("2", QDir::currentPath() + "/sysroot2").toLocal8Bit()),
             m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains(outputTemplate
                                  .arg("3", QDir::currentPath() + "/sysroot1").toLocal8Bit()),
             m_qbsStdout.constData());
}

void TestBlackbox::pluginDependency()
{
    QDir::setCurrent(testDataDir + "/plugin-dependency");

    // Build the plugins and the helper2 lib.
    QCOMPARE(runQbs(QStringList{"--products", "plugin1,plugin2,plugin3,plugin4,helper2"}), 0);
    QVERIFY(m_qbsStdout.contains("plugin1"));
    QVERIFY(m_qbsStdout.contains("plugin2"));
    QVERIFY(m_qbsStdout.contains("plugin3"));
    QVERIFY(m_qbsStdout.contains("plugin4"));
    QVERIFY(m_qbsStdout.contains("helper2"));
    QVERIFY(!m_qbsStderr.contains("SOFT ASSERT"));

    // Build the app. Plugins 1 and 2 must not be linked. Plugin 3 must be linked.
    QCOMPARE(runQbs(QStringList{"--command-echo-mode", "command-line"}), 0);
    QByteArray output = m_qbsStdout + '\n' + m_qbsStderr;
    QVERIFY(!output.contains("plugin1"));
    QVERIFY(!output.contains("plugin2"));
    QVERIFY(!output.contains("helper2"));

    // Test change tracking for parameter in Parameters item.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("plugin-dependency.qbs", "false // marker 1", "true");
    QCOMPARE(runQbs(QStringList{"-p", "plugin2"}), 0);
    QVERIFY2(!m_qbsStdout.contains("linking"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QStringList{"--command-echo-mode", "command-line"}), 0);
    output = m_qbsStdout + '\n' + m_qbsStderr;
    QVERIFY2(!output.contains("plugin1"), output.constData());
    QVERIFY2(!output.contains("helper2"), output.constData());
    QVERIFY2(output.contains("plugin2"), output.constData());

    // Test change tracking for parameter in Depends item.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("plugin-dependency.qbs", "false /* marker 2 */", "true");
    QCOMPARE(runQbs(QStringList{"-p", "helper1", "--command-echo-mode", "command-line"}), 0);
    output = m_qbsStdout + '\n' + m_qbsStderr;
    QVERIFY2(output.contains("helper2"), output.constData());

    // Check that the build dependency still works.
    QCOMPARE(runQbs(QStringLiteral("clean")), 0);
    QCOMPARE(runQbs(QStringList{"--products", "myapp", "--command-echo-mode", "command-line"}), 0);
    QVERIFY(m_qbsStdout.contains("plugin1"));
    QVERIFY(m_qbsStdout.contains("plugin2"));
    QVERIFY(m_qbsStdout.contains("plugin3"));
    QVERIFY(m_qbsStdout.contains("plugin4"));
}

void TestBlackbox::precompiledAndPrefixHeaders()
{
    QDir::setCurrent(testDataDir + "/precompiled-and-prefix-headers");
    const bool success = runQbs() == 0;
    if (!success && m_qbsStderr.contains("mingw32_gt_pch_use_address"))
        QSKIP("https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91440");
    QVERIFY(success);
}

void TestBlackbox::precompiledHeaderAndRedefine()
{
    QDir::setCurrent(testDataDir + "/precompiled-headers-and-redefine");
    const bool success = runQbs() == 0;
    if (!success && m_qbsStderr.contains("mingw32_gt_pch_use_address"))
        QSKIP("https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91440");
    QVERIFY(success);
}

void TestBlackbox::preventFloatingPointValues()
{
    QDir::setCurrent(testDataDir + "/prevent-floating-point-values");
    QCOMPARE(runQbs(QStringList("products.p.version:1.50")), 0);
    QVERIFY2(m_qbsStdout.contains("version: 1.50"), m_qbsStdout.constData());
}

void TestBlackbox::probeChangeTracking()
{
    QDir::setCurrent(testDataDir + "/probe-change-tracking");

    // Product probe disabled, other probes enabled.
    QbsRunParameters params;
    params.command = "resolve";
    params.arguments = QStringList("products.theProduct.runProbe:false");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(m_qbsStdout.contains("running subProbe"));
    QVERIFY(!m_qbsStdout.contains("running productProbe"));

    // Product probe newly enabled.
    params.arguments = QStringList("products.theProduct.runProbe:true");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(!m_qbsStdout.contains("running subProbe"));
    QVERIFY(m_qbsStdout.contains("running productProbe: 12"));

    // Re-resolving with unchanged probe.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("probe-change-tracking.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(!m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(!m_qbsStdout.contains("running subProbe"));
    QVERIFY(!m_qbsStdout.contains("running productProbe"));

    // Re-resolving with changed configure scripts.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("probe-change-tracking.qbs", "console.info", " console.info");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(m_qbsStdout.contains("running subProbe"));
    QVERIFY(m_qbsStdout.contains("running productProbe: 12"));

    // Re-resolving with added property.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("probe-change-tracking.qbs", "condition: product.runProbe",
                    "condition: product.runProbe\nproperty string something: 'x'");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(!m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(!m_qbsStdout.contains("running subProbe"));
    QVERIFY(m_qbsStdout.contains("running productProbe: 12"));

    // Re-resolving with changed property.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("probe-change-tracking.qbs", "'x'", "'y'");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(!m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(!m_qbsStdout.contains("running subProbe"));
    QVERIFY(m_qbsStdout.contains("running productProbe: 12"));

    // Re-resolving with removed property.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("probe-change-tracking.qbs", "property string something: 'y'", "");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(!m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(!m_qbsStdout.contains("running subProbe"));
    QVERIFY(m_qbsStdout.contains("running productProbe: 12"));

    // Re-resolving with unchanged probe again.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("probe-change-tracking.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(!m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(!m_qbsStdout.contains("running subProbe"));
    QVERIFY(!m_qbsStdout.contains("running productProbe"));

    // Enforcing re-running via command-line option.
    params.arguments.prepend("--force-probe-execution");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(m_qbsStdout.contains("running tlpProbe"));
    QVERIFY(m_qbsStdout.contains("running subProbe"));
    QVERIFY(m_qbsStdout.contains("running productProbe: 12"));
}

void TestBlackbox::probeProperties()
{
    QDir::setCurrent(testDataDir + "/probeProperties");
    const QByteArray dir = QDir::cleanPath(testDataDir).toLocal8Bit() + "/probeProperties";
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("probe1.fileName=bin/tool"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe1.path=" + dir), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe1.filePath=" + dir + "/bin/tool"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe2.fileName=tool"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe2.path=" + dir + "/bin"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe2.filePath=" + dir + "/bin/tool"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe3.fileName=tool"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe3.path=" + dir + "/bin"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe3.filePath=" + dir + "/bin/tool"), m_qbsStdout.constData());
}

void TestBlackbox::probesAndShadowProducts()
{
    QDir::setCurrent(testDataDir + "/probes-and-shadow-products");
    QCOMPARE(runQbs(QStringList("--log-time")), 0);
    QVERIFY2(m_qbsStdout.contains("2 probes encountered, 1 configure scripts executed"),
             m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    touch("probes-and-shadow-products.qbs");
    QCOMPARE(runQbs(QStringList("--log-time")), 0);
    QVERIFY2(m_qbsStdout.contains("2 probes encountered, 0 configure scripts executed"),
             m_qbsStdout.constData());
}

void TestBlackbox::probeInExportedModule()
{
    QDir::setCurrent(testDataDir + "/probe-in-exported-module");
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << QStringLiteral("-f")
                                     << QStringLiteral("probe-in-exported-module.qbs"))), 0);
    QVERIFY2(m_qbsStdout.contains("found: true"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("prop: yes"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("listProp: myother,my"), m_qbsStdout.constData());
}

void TestBlackbox::probesAndArrayProperties()
{
    QDir::setCurrent(testDataDir + "/probes-and-array-properties");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("prop: [\"probe\"]"), m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("probes-and-array-properties.qbs", "//", "");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("prop: [\"product\",\"probe\"]"), m_qbsStdout.constData());
}

void TestBlackbox::productProperties()
{
    QDir::setCurrent(testDataDir + "/productproperties");
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << QStringLiteral("-f")
                                     << QStringLiteral("productproperties.qbs"))), 0);
    QVERIFY(regularFileExists(relativeExecutableFilePath("blubb_user", m_qbsStdout)));
}

void TestBlackbox::propertyAssignmentInFailedModule()
{
    QDir::setCurrent(testDataDir + "/property-assignment-in-failed-module");
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("modules.m.doFail:false"))), 0);
    QbsRunParameters failParams;
    failParams.expectFailure = true;
    QVERIFY(runQbs(failParams) != 0);
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("modules.m.doFail:true"))), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    failParams.expectFailure = false;
    QCOMPARE(runQbs(failParams), 0);
}

void TestBlackbox::propertyChanges()
{
    QDir::setCurrent(testDataDir + "/propertyChanges");
    const QString projectFile("propertyChanges.qbs");
    QbsRunParameters params(QStringList({"-f", "propertyChanges.qbs", "qbs.enableDebugCode:true"}));

    // Initial build.
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product 1.debug"));
    QVERIFY(m_qbsStdout.contains("generated.txt"));
    QVERIFY(m_qbsStdout.contains("making output from input"));
    QVERIFY(m_qbsStdout.contains("default value"));
    QVERIFY(m_qbsStdout.contains("making output from other output"));
    QFile generatedFile(relativeProductBuildDir("generated text file") + "/generated.txt");
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
    QVERIFY(!m_qbsStdout.contains("making output from input"));
    QVERIFY(!m_qbsStdout.contains("making output from other output"));

    // Incremental build with no changes, but updated project file timestamp.
    WAIT_FOR_NEW_TIMESTAMP();
    touch(projectFile);
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("making output from input"));
    QVERIFY(!m_qbsStdout.contains("making output from other output"));

    // Incremental build, input property changed for first product
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFile, "blubb1", "blubb01");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product 1.debug"));
    QVERIFY(!m_qbsStdout.contains("linking product 2"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("linking library"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("making output from input"));
    QVERIFY(!m_qbsStdout.contains("making output from other output"));

    // Incremental build, input property changed via project for second product.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFile, "blubb2", "blubb02");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("making output from input"));
    QVERIFY(!m_qbsStdout.contains("making output from other output"));

    // Incremental build, input property changed via command line for second product.
    params.command = "resolve";
    params.arguments << "project.projectDefines:blubb002";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    params.arguments.removeLast();
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("making output from input"));
    QVERIFY(!m_qbsStdout.contains("making output from other output"));

    // Incremental build, input property changed via environment for third product.
    params.environment.insert("QBS_BLACKBOX_DEFINE", "newvalue");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(!m_qbsStdout.contains("linking product 2"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    params.environment.remove("QBS_BLACKBOX_DEFINE");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(!m_qbsStdout.contains("linking product 2"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("making output from input"));
    QVERIFY(!m_qbsStdout.contains("making output from other output"));
    params.environment.insert("QBS_BLACKBOX_DEFINE", "newvalue");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(!m_qbsStdout.contains("linking product 2"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    params.environment.remove("QBS_BLACKBOX_DEFINE");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(!m_qbsStdout.contains("linking product 2"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("making output from input"));
    QVERIFY(!m_qbsStdout.contains("making output from other output"));

    // Incremental build, module property changed via command line.
    params.arguments << "qbs.enableDebugCode:false";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product 1.release"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    params.arguments.removeLast();
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product 1.debug"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("making output from input"));
    QVERIFY(!m_qbsStdout.contains("making output from other output"));

    // Incremental build, non-essential dependency removed.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFile, "Depends { name: 'library' }", "// Depends { name: 'library' }");
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(m_qbsStdout.contains("linking product 2"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("linking library"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("making output from input"));
    QVERIFY(!m_qbsStdout.contains("making output from other output"));

    // Incremental build, prepare script of a transformer changed.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFile, "contents 1", "contents 2");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("making output from input"));
    QVERIFY(!m_qbsStdout.contains("making output from other output"));
    QVERIFY(generatedFile.open(QIODevice::ReadOnly));
    QCOMPARE(generatedFile.readAll(), QByteArray("prefix 1contents 2suffix 1"));
    generatedFile.close();

    // Incremental build, product property used in JavaScript command changed.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFile, "prefix 1", "prefix 2");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("making output from input"));
    QVERIFY(!m_qbsStdout.contains("making output from other output"));
    QVERIFY(generatedFile.open(QIODevice::ReadOnly));
    QCOMPARE(generatedFile.readAll(), QByteArray("prefix 2contents 2suffix 1"));
    generatedFile.close();

    // Incremental build, project property used in JavaScript command changed.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFile, "suffix 1", "suffix 2");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("making output from input"));
    QVERIFY(!m_qbsStdout.contains("making output from other output"));
    QVERIFY(generatedFile.open(QIODevice::ReadOnly));
    QCOMPARE(generatedFile.readAll(), QByteArray("prefix 2contents 2suffix 2"));
    generatedFile.close();

    // Incremental build, module property used in JavaScript command changed.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFile, "default value", "new value");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(m_qbsStdout.contains("making output from input"));
    QVERIFY(m_qbsStdout.contains("making output from other output"));
    QVERIFY(m_qbsStdout.contains("new value"));

    // Incremental build, prepare script of a rule in a module changed.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("modules/TestModule/module.qbs", "// console.info('Change in source code')",
                    "console.info('Change in source code')");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(m_qbsStdout.contains("making output from input"));
    QVERIFY(m_qbsStdout.contains("making output from other output"));
}

void TestBlackbox::propertyEvaluationContext()
{
    const QString testDir = testDataDir + "/property-evaluation-context";
    QDir::setCurrent(testDir);
    QCOMPARE(runQbs(), 0);
    QCOMPARE(m_qbsStdout.count("base.productInBase evaluated in: myapp"), 1);
    QCOMPARE(m_qbsStdout.count("base.productInTop evaluated in: myapp"), 1);
    QCOMPARE(m_qbsStdout.count("top.productInExport evaluated in: mylib"), 1);
    QCOMPARE(m_qbsStdout.count("top.productInTop evaluated in: myapp"), 1);
}

void TestBlackbox::qtBug51237()
{
    const SettingsPtr s = settings();
    qbs::Internal::TemporaryProfile profile("qbs_autotests_qtBug51237", s.get());
    profile.p.setValue("mymodule.theProperty", QStringList());
    s->sync();

    QDir::setCurrent(testDataDir + "/QTBUG-51237");
    QbsRunParameters params;
    params.profile = profile.p.name();
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::dynamicMultiplexRule()
{
    const QString testDir = testDataDir + "/dynamicMultiplexRule";
    QDir::setCurrent(testDir);
    QCOMPARE(runQbs(), 0);
    const QString outputFilePath = relativeProductBuildDir("dynamicMultiplexRule") + "/stuff-from-3-inputs";
    QVERIFY(regularFileExists(outputFilePath));
    WAIT_FOR_NEW_TIMESTAMP();
    touch("two.txt");
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(outputFilePath));
}

void TestBlackbox::dynamicProject()
{
    const QString testDir = testDataDir + "/dynamic-project";
    QDir::setCurrent(testDir);
    QCOMPARE(runQbs(), 0);
    QCOMPARE(m_qbsStdout.count("compiling main.cpp"), 2);
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

    const QString appFile = relativeExecutableFilePath("genlexer", m_qbsStdout);
    const QString headerFile1 = relativeProductBuildDir("genlexer") + "/GeneratedFiles/numberscanner.h";
    const QString sourceFile1 = relativeProductBuildDir("genlexer") + "/GeneratedFiles/numberscanner.c";
    const QString sourceFile2 = relativeProductBuildDir("genlexer") + "/GeneratedFiles/lex.yy.c";

    // Check build #1: source and header file name are specified in numbers.l
    QVERIFY(regularFileExists(appFile));
    QVERIFY(regularFileExists(headerFile1));
    QVERIFY(regularFileExists(sourceFile1));
    QVERIFY(!QFile::exists(sourceFile2));

    QDateTime appFileTimeStamp1 = QFileInfo(appFile).lastModified();
    WAIT_FOR_NEW_TIMESTAMP();
    copyFileAndUpdateTimestamp("../after/numbers.l", "numbers.l");
    QCOMPARE(runQbs(), 0);

    // Check build #2: no file names are specified in numbers.l
    //                 flex will default to lex.yy.c without header file.
    QDateTime appFileTimeStamp2 = QFileInfo(appFile).lastModified();
    QVERIFY(appFileTimeStamp1 < appFileTimeStamp2);
    QVERIFY(!QFile::exists(headerFile1));
    QVERIFY(!QFile::exists(sourceFile1));
    QVERIFY(regularFileExists(sourceFile2));

    WAIT_FOR_NEW_TIMESTAMP();
    copyFileAndUpdateTimestamp("../before/numbers.l", "numbers.l");
    QCOMPARE(runQbs(), 0);

    // Check build #3: source and header file name are specified in numbers.l
    QDateTime appFileTimeStamp3 = QFileInfo(appFile).lastModified();
    QVERIFY(appFileTimeStamp2 < appFileTimeStamp3);
    QVERIFY(regularFileExists(appFile));
    QVERIFY(regularFileExists(headerFile1));
    QVERIFY(regularFileExists(sourceFile1));
    QVERIFY(!QFile::exists(sourceFile2));
}

void TestBlackbox::emptyProfile()
{
    QDir::setCurrent(testDataDir + "/empty-profile");

    const SettingsPtr s = settings();
    const qbs::Profile buildProfile(profileName(), s.get());
    bool isMsvc = false;
    auto toolchainType = buildProfile.value(QStringLiteral("qbs.toolchainType")).toString();
    QbsRunParameters params;
    params.profile = "none";

    if (toolchainType.isEmpty()) {
        const auto toolchain = buildProfile.value(QStringLiteral("qbs.toolchain")).toStringList();
        if (!toolchain.isEmpty())
            toolchainType = toolchain.first();
    }
    if (!toolchainType.isEmpty()) {
        params.arguments = QStringList{QStringLiteral("qbs.toolchainType:") + toolchainType};
        isMsvc = toolchainType == "msvc" || toolchainType == "clang-cl";
    }

    if (!isMsvc) {
        const auto tcPath =
                QDir::toNativeSeparators(
                        buildProfile.value(QStringLiteral("cpp.toolchainInstallPath")).toString());
        auto paths = params.environment.value(QStringLiteral("PATH"))
                .split(HostOsInfo::pathListSeparator(), Qt::SkipEmptyParts);
        if (!tcPath.isEmpty() && !paths.contains(tcPath)) {
            paths.prepend(tcPath);
            params.environment.insert(
                    QStringLiteral("PATH"), paths.join(HostOsInfo::pathListSeparator()));
        }
    }
    QCOMPARE(runQbs(params), 0);
}

using ASet = QSet<QString>;
namespace {
QString JS()
{
    static const auto suffix = QStringLiteral(".js");
    return suffix;
}
QString WASM()
{
    static const auto suffix = QStringLiteral(".wasm");
    return suffix;
}
QString WASMJS()
{
    static const auto suffix = QStringLiteral(".wasm.js");
    return suffix;
}
QString HTML()
{
    static const auto suffix = QStringLiteral(".html");
    return suffix;
}
} // namespace
void TestBlackbox::emscriptenArtifacts()
{
    QDir::setCurrent(testDataDir + "/emscripten-artifacts");
    QFETCH(QString, executableSuffix);
    QFETCH(QString, wasmOption);
    QFETCH(int, buildresult);
    QFETCH(ASet, suffixes);

    const QStringList params = {
        QStringLiteral("modules.cpp.executableSuffix:%1").arg(executableSuffix),
        QStringLiteral("modules.cpp.driverLinkerFlags:%1").arg(wasmOption)};

    ASet possibleSuffixes = {JS(), WASM(), WASMJS(), HTML()};

    QbsRunParameters qbsParams("resolve", params);
    int result = runQbs(qbsParams);
    QCOMPARE(result, 0);
    if (m_qbsStdout.contains("is emscripten: false"))
        QSKIP("Skipping emscripten test");
    QVERIFY(m_qbsStdout.contains("is emscripten: true"));
    result = runQbs(QbsRunParameters("build", params));
    QCOMPARE(result, buildresult);
    if (result == 0) {
        const auto app = QStringLiteral("app");
        const auto appWithoutSuffix = relativeProductBuildDir(app) + QLatin1Char('/') + app;

        ASet usedSuffixes;
        for (const auto &suffix : suffixes) {
            usedSuffixes.insert(suffix);
            QVERIFY(regularFileExists(appWithoutSuffix + suffix));
        }

        possibleSuffixes.subtract(usedSuffixes);
        for (const auto &unusedSuffix : possibleSuffixes) {
            QVERIFY(!regularFileExists(appWithoutSuffix + unusedSuffix));
        }
    }
    QCOMPARE(runQbs(QbsRunParameters("clean")), 0);
}

void TestBlackbox::emscriptenArtifacts_data()
{
    QTest::addColumn<QString>("executableSuffix");
    QTest::addColumn<QString>("wasmOption");
    QTest::addColumn<int>("buildresult");
    QTest::addColumn<ASet>("suffixes");

    QTest::newRow(".js WASM=0") << ".js"
                                << "-sWASM=0" << 0 << ASet{JS()};
    QTest::newRow(".js WASM=1") << ".js"
                                << "-sWASM=1" << 0 << ASet{JS(), WASM()};

    QTest::newRow(".js WASM=2") << ".js"
                                << "-sWASM=2" << 0 << ASet{JS(), WASM(), WASMJS()};

    QTest::newRow(".js WASM=779") << ".js"
                                  << "-sWASM=779" << 0 << ASet{JS(), WASM()};

    QTest::newRow(".wasm WASM=0") << ".wasm"
                                  << "-sWASM=0" << 1 << ASet{};

    QTest::newRow(".wasm WASM=1") << ".wasm"
                                  << "-sWASM=1" << 0 << ASet{WASM()};

    QTest::newRow(".wasm WASM=2") << ".wasm"
                                  << "-sWASM=2" << 0 << ASet{WASM(), WASMJS()};

    QTest::newRow(".wasm WASM=848") << ".wasm"
                                    << "-sWASM=848" << 0 << ASet{WASM()};

    QTest::newRow(".html WASM=0") << ".html"
                                  << "-sWASM=0" << 0 << ASet{JS(), HTML()};

    QTest::newRow(".html WASM=1") << ".html"
                                  << "-sWASM=1" << 0 << ASet{JS(), HTML(), WASM()};

    QTest::newRow(".html WASM=2") << ".html"
                                  << "-sWASM=2" << 0 << ASet{JS(), HTML(), WASM(), WASMJS()};

    QTest::newRow(".html WASM=233") << ".html"
                                    << "-sWASM=233" << 0 << ASet{JS(), HTML(), WASM()};
}

void TestBlackbox::erroneousFiles_data()
{
    QTest::addColumn<QString>("errorMessage");
    QTest::newRow("nonexistentWorkingDir")
            << "The working directory '.does.not.exist' for process '.*ls.*' is invalid.";
    QTest::newRow("outputArtifacts-missing-filePath")
            << "Error in Rule\\.outputArtifacts\\[0\\]\n\r?"
               "Property filePath must be a non-empty string\\.";
    QTest::newRow("outputArtifacts-missing-fileTags")
            << "Error in Rule\\.outputArtifacts\\[0\\]\n\r?"
               "Property fileTags for artifact 'outputArtifacts-missing-fileTags\\.txt' "
               "must be a non-empty string list\\.";
    QTest::newRow("texttemplate-unknown-placeholder")
            << "Placeholder 'what' is not defined in textemplate.dict for 'boom.txt.in'";
    QTest::newRow("tag-mismatch")
            << "tag-mismatch.qbs:8:18.*Artifact '.*dummy1' has undeclared file tags "
               "\\[\"y\",\"z\"\\].";
}

void TestBlackbox::erroneousFiles()
{
    QFETCH(QString, errorMessage);
    QDir::setCurrent(testDataDir + "/erroneous/" + QTest::currentDataTag());
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QString err = QString::fromLocal8Bit(m_qbsStderr);
    if (!err.contains(QRegularExpression(errorMessage))) {
        qDebug().noquote() << "Output:  " << err;
        qDebug().noquote() << "Expected: " << errorMessage;
        QFAIL("Unexpected error message.");
    }
}

void TestBlackbox::errorInfo()
{
    QDir::setCurrent(testDataDir + "/error-info");
    QCOMPARE(runQbs(), 0);

    QbsRunParameters resolveParams;
    QbsRunParameters buildParams;
    buildParams.expectFailure = true;

    resolveParams.command = "resolve";
    resolveParams.arguments = QStringList() << "project.fail1:true";
    QCOMPARE(runQbs(resolveParams), 0);
    buildParams.arguments = resolveParams.arguments;
    QVERIFY(runQbs(buildParams) != 0);
    QVERIFY2(m_qbsStderr.contains("error-info.qbs:24"), m_qbsStderr);

    resolveParams.arguments = QStringList() << "project.fail2:true";
    QCOMPARE(runQbs(resolveParams), 0);
    buildParams.arguments = resolveParams.arguments;
    QVERIFY(runQbs(buildParams) != 0);
    QVERIFY2(m_qbsStderr.contains("error-info.qbs:36"), m_qbsStderr);

    resolveParams.arguments = QStringList() << "project.fail3:true";
    QCOMPARE(runQbs(resolveParams), 0);
    buildParams.arguments = resolveParams.arguments;
    QVERIFY(runQbs(buildParams) != 0);
    QVERIFY2(m_qbsStderr.contains("error-info.qbs:51"), m_qbsStderr);

    resolveParams.arguments = QStringList() << "project.fail4:true";
    QCOMPARE(runQbs(resolveParams), 0);
    buildParams.arguments = resolveParams.arguments;
    QVERIFY(runQbs(buildParams) != 0);
    QVERIFY2(m_qbsStderr.contains("error-info.qbs:66"), m_qbsStderr);

    resolveParams.arguments = QStringList() << "project.fail5:true";
    QCOMPARE(runQbs(resolveParams), 0);
    buildParams.arguments = resolveParams.arguments;
    QVERIFY(runQbs(buildParams) != 0);
    QVERIFY2(m_qbsStderr.contains("helper.js:4"), m_qbsStderr);

    resolveParams.arguments = QStringList() << "project.fail6:true";
    QCOMPARE(runQbs(resolveParams), 0);
    buildParams.arguments = resolveParams.arguments;
    QVERIFY(runQbs(buildParams) != 0);
    QVERIFY2(m_qbsStderr.contains("helper.js:8"), m_qbsStderr);

    resolveParams.arguments = QStringList() << "project.fail7:true";
    QCOMPARE(runQbs(resolveParams), 0);
    buildParams.arguments = resolveParams.arguments;
    QVERIFY(runQbs(buildParams) != 0);
    QVERIFY2(m_qbsStderr.contains("JavaScriptCommand.sourceCode"), m_qbsStderr);
    QVERIFY2(m_qbsStderr.contains("error-info.qbs:57"), m_qbsStderr);
}

void TestBlackbox::escapedLinkerFlags()
{
    QDir::setCurrent(testDataDir + "/escaped-linker-flags");
    QbsRunParameters params("resolve", QStringList("products.app.escapeLinkerFlags:false"));
    QCOMPARE(runQbs(params), 0);
    const bool isGcc = m_qbsStdout.contains("is gcc: true");
    const bool isNotGcc = m_qbsStdout.contains("is gcc: false");
    if (isNotGcc)
        QSKIP("escaped linker flags test only applies on plain unix with gcc and GNU ld");
    QVERIFY(isGcc);

    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    params.command = "resolve";
    params.arguments = QStringList() << "products.app.escapeLinkerFlags:true";
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Encountered escaped linker flag"), m_qbsStderr.constData());
}

void TestBlackbox::exportedDependencyInDisabledProduct()
{
    QDir::setCurrent(testDataDir + "/exported-dependency-in-disabled-product");
    QFETCH(QString, depCondition);
    QFETCH(bool, compileExpected);
    rmDirR(relativeBuildDir());
    const QString propertyArg = "products.dep.conditionString:" + depCondition;
    QCOMPARE(runQbs(QStringList(propertyArg)), 0);
    QCOMPARE(m_qbsStdout.contains("compiling"), compileExpected);
}

void TestBlackbox::exportedDependencyInDisabledProduct_data()
{
    QTest::addColumn<QString>("depCondition");
    QTest::addColumn<bool>("compileExpected");
    QTest::newRow("dependency enabled") << "true" << true;
    QTest::newRow("dependency directly disabled") << "false" << false;
    QTest::newRow("dependency disabled via non-present module") << "nosuchmodule.present" << false;
    QTest::newRow("dependency disabled via failed module") << "broken.present" << false;
}

void TestBlackbox::exportedPropertyInDisabledProduct()
{
    QDir::setCurrent(testDataDir + "/exported-property-in-disabled-product");
    QFETCH(QString, depCondition);
    QFETCH(bool, successExpected);
    const QString propertyArg = "products.dep.conditionString:" + depCondition;
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList(propertyArg))), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QbsRunParameters buildParams;
    buildParams.expectFailure = !successExpected;
    QCOMPARE(runQbs(buildParams) == 0, successExpected);
}

void TestBlackbox::exportedPropertyInDisabledProduct_data()
{
    QTest::addColumn<QString>("depCondition");
    QTest::addColumn<bool>("successExpected");
    QTest::newRow("dependency enabled") << "true" << false;
    QTest::newRow("dependency directly disabled") << "false" << true;
    QTest::newRow("dependency disabled via non-present module") << "nosuchmodule.present" << true;
    QTest::newRow("dependency disabled via failed module") << "broken.present" << true;
}

void TestBlackbox::systemRunPaths()
{
    const QString lddFilePath = findExecutable(QStringList() << "ldd");
    if (lddFilePath.isEmpty())
        QSKIP("ldd not found");

    QDir::setCurrent(testDataDir + "/system-run-paths");
    QFETCH(bool, setRunPaths);
    rmDirR(relativeBuildDir());
    QbsRunParameters params("resolve");
    params.arguments << QString("project.setRunPaths:") + (setRunPaths ? "true" : "false");
    QCOMPARE(runQbs(params), 0);
    const bool isUnix = m_qbsStdout.contains("is unix: true");
    const bool isNotUnix = m_qbsStdout.contains("is unix: false");
    if (isNotUnix)
        QSKIP("only applies on Unix");
    QVERIFY(isUnix);
    const QString exe = relativeExecutableFilePath("app", m_qbsStdout);

    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QProcess ldd;
    ldd.start(lddFilePath, QStringList() << exe);
    QVERIFY2(ldd.waitForStarted(), qPrintable(ldd.errorString()));
    QVERIFY2(ldd.waitForFinished(), qPrintable(ldd.errorString()));
    QVERIFY2(ldd.exitCode() == 0, ldd.readAllStandardError().constData());
    const QByteArray output = ldd.readAllStandardOutput();
    const QList<QByteArray> outputLines = output.split('\n');
    QByteArray libLine;
    for (const auto &line : outputLines) {
        if (line.contains("theLib")) {
            libLine = line;
            break;
        }
    }
    QVERIFY2(!libLine.isEmpty(), output.constData());
    QVERIFY2(setRunPaths == libLine.contains("not found"), libLine.constData());
}

void TestBlackbox::systemRunPaths_data()
{
    QTest::addColumn<bool>("setRunPaths");
    QTest::newRow("do not set system run paths") << false;
    QTest::newRow("do set system run paths") << true;
}

void TestBlackbox::exportRule()
{
    QDir::setCurrent(testDataDir + "/export-rule");
    QbsRunParameters params(QStringList{"modules.blubber.enableTagger:false"});
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    params.command = "resolve";
    params.arguments = QStringList{"modules.blubber.enableTagger:true"};
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("creating C++ source file"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("compiling myapp.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::exportToOutsideSearchPath()
{
    QDir::setCurrent(testDataDir + "/export-to-outside-searchpath");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Dependency 'aModule' not found for product 'theProduct'."),
             m_qbsStderr.constData());
}

void TestBlackbox::exportsCMake()
{
    QFETCH(QStringList, arguments);

    QDir::setCurrent(testDataDir + "/exports-cmake");
    rmDirR(relativeBuildDir());
    QbsRunParameters findCMakeParams("resolve", {"-f", "find-cmake.qbs"});
    QCOMPARE(runQbs(findCMakeParams), 0);
    const QString output = QString::fromLocal8Bit(m_qbsStdout);
    const QRegularExpression pattern(
        QRegularExpression::anchoredPattern(".*---(.*)---.*"),
        QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = pattern.match(output);
    QVERIFY2(match.hasMatch(), qPrintable(output));
    QCOMPARE(pattern.captureCount(), 1);
    const QString jsonString = match.captured(1);
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonString.toUtf8());
    const QJsonObject jsonData = jsonDoc.object();

    rmDirR(relativeBuildDir());
    const QStringList exporterArgs{"-f", "exports-cmake.qbs"};
    QbsRunParameters exporterRunParams("build", exporterArgs);
    exporterRunParams.arguments << arguments;
    QCOMPARE(runQbs(exporterRunParams), 0);

    if (!jsonData.value(u"cmakeFound").toBool()) {
        QSKIP("cmake is not installed");
        return;
    }

    if (jsonData.value(u"crossCompiling").toBool()) {
        QSKIP("test is not applicable with cross-compile toolchains");
        return;
    }

    const auto cmakeFilePath = jsonData.value(u"cmakeFilePath").toString();
    QVERIFY(!cmakeFilePath.isEmpty());

    const auto generator = jsonData.value(u"generator").toString();
    if (generator.isEmpty()) {
        QSKIP("cannot detect cmake generator");
        return;
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const auto buildEnv = jsonData.value(u"buildEnv").toObject();
    for (auto it = buildEnv.begin(), end = buildEnv.end(); it != end; ++it) {
        if (it.key() == "PATH") {
            auto paths = it.value().toString().split(HostOsInfo::pathListSeparator());
            paths.append(env.value(it.key()).split(HostOsInfo::pathListSeparator()));
            env.insert(it.key(), paths.join(HostOsInfo::pathListSeparator()));
        } else {
            env.insert(it.key(), it.value().toString());
        }
    }

    const auto installPrefix = jsonData.value(u"installPrefix").toString();
    const auto cmakePrefixPath = QFileInfo(relativeBuildDir()).absoluteFilePath() + "/install-root/"
                                 + installPrefix + "/lib/cmake";
    const auto sourceDirectory = testDataDir + "/exports-cmake/cmake";
    QProcess configure;
    configure.setProcessEnvironment(env);
    configure.setWorkingDirectory(sourceDirectory);
    configure.start(
        cmakeFilePath, {".", "-DCMAKE_PREFIX_PATH=" + cmakePrefixPath, "-G" + generator});
    QVERIFY(waitForProcessSuccess(configure, 120000));

    QProcess build;
    build.setProcessEnvironment(env);
    build.setWorkingDirectory(sourceDirectory);
    build.start(cmakeFilePath, QStringList{"--build", "."});
    QVERIFY(waitForProcessSuccess(build));
}

void TestBlackbox::exportsCMake_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::newRow("dynamic lib") << QStringList("project.isStatic: false");
    QTest::newRow("static lib") << QStringList("project.isStatic: true");
    QTest::newRow("framework") << QStringList("project.isBundle: true");
}

void TestBlackbox::exportsPkgconfig()
{
    QDir::setCurrent(testDataDir + "/exports-pkgconfig");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("creating TheFirstLib.pc"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("creating TheSecondLib.pc"), m_qbsStdout.constData());
    QFile sourcePcFile(HostOsInfo::isWindowsHost() ? "TheFirstLib_windows.pc" : "TheFirstLib.pc");
    QString generatedPcFilePath = relativeProductBuildDir("TheFirstLib") + "/TheFirstLib.pc";
    QFile generatedPcFile(generatedPcFilePath);
    QVERIFY2(sourcePcFile.open(QIODevice::ReadOnly), qPrintable(sourcePcFile.errorString()));
    QVERIFY2(generatedPcFile.open(QIODevice::ReadOnly), qPrintable(generatedPcFile.errorString()));
    QCOMPARE(generatedPcFile.readAll().replace("\r", ""), sourcePcFile.readAll().replace("\r", ""));
    sourcePcFile.close();
    generatedPcFile.close();
    TEXT_FILE_COMPARE(relativeProductBuildDir("TheSecondLib") + "/TheSecondLib.pc",
                      "TheSecondLib.pc");
    WAIT_FOR_NEW_TIMESTAMP();
    touch("firstlib.cpp");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("linking"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("creating TheFirstLib.pc"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("creating TheSecondLib.pc"), m_qbsStdout.constData());
}

void TestBlackbox::exportsQbs()
{
    QDir::setCurrent(testDataDir + "/exports-qbs");

    QCOMPARE(runQbs({"resolve", {"-f", "exports-qbs.qbs"}}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    // First we build exportable products and use them (as products) inside
    // the original project.
    QCOMPARE(runQbs(QStringList{"-f", "exports-qbs.qbs", "--command-echo-mode", "command-line"}),
             0);
    QVERIFY2(m_qbsStdout.contains("somelocaldir"), m_qbsStdout.constData());

    // Now we build an external product against the modules that were just installed.
    // We try debug and release mode; one module exists for each of them.
    QbsRunParameters paramsExternalBuild(QStringList{"-f", "consumer.qbs",
                                                     "--command-echo-mode", "command-line",
                                                     "modules.qbs.buildVariant:debug",});
    paramsExternalBuild.buildDirectory = QDir::currentPath() + "/external-consumer-debug";
    QCOMPARE(runQbs(paramsExternalBuild), 0);
    QVERIFY2(!m_qbsStdout.contains("somelocaldir"), m_qbsStdout.constData());

    paramsExternalBuild.arguments = QStringList{"-f", "consumer.qbs",
            "modules.qbs.buildVariant:release"};
    paramsExternalBuild.buildDirectory = QDir::currentPath() + "/external-consumer-release";
    QCOMPARE(runQbs(paramsExternalBuild), 0);

    // Trying to build with an unsupported build variant must fail.
    paramsExternalBuild.arguments = QStringList{"-f", "consumer.qbs",
            "modules.qbs.buildVariant:profiling"};
    paramsExternalBuild.buildDirectory = QDir::currentPath() + "/external-consumer-profile";
    paramsExternalBuild.expectFailure = true;
    QVERIFY(runQbs(paramsExternalBuild) != 0);
    QVERIFY2(m_qbsStderr.contains("Dependency 'MyLib' not found for product 'consumer'"),
             m_qbsStderr.constData());

    // Removing the condition from the generated module leaves us with two conflicting
    // candidates.
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList{ "-f", "exports-qbs.qbs",
            "modules.Exporter.qbs.additionalContent:''"})), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(runQbs(paramsExternalBuild) != 0);
    QVERIFY2(m_qbsStderr.contains("There is more than one equally prioritized candidate "
                                  "for module 'MyLib'."), m_qbsStderr.constData());

    // Change tracking for accesses to product.exports (negative).
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList{"-f", "exports-qbs.qbs"})), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("creating MyTool.qbs"), m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    touch("exports-qbs.qbs");
    QCOMPARE(runQbs(QStringList({"-p", "MyTool"})), 0);
    QVERIFY2(!m_qbsStdout.contains("creating MyTool.qbs"), m_qbsStdout.constData());

    // Rebuilding the target binary should not cause recreating the module file.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("mylib.cpp");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.count("linking") >= 2, m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("creating MyLib"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("creating MyTool.qbs"), m_qbsStdout.constData());

    // Changing a setting that influences the name of a target artifact should cause
    // recreating the module file.
    const QbsRunParameters resolveParams("resolve", QStringList{"-f", "exports-qbs.qbs",
            "modules.cpp.dynamicLibrarySuffix:.blubb"});
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.count("linking") >= 2, m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.count("creating MyLib") == 2, m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("creating MyTool.qbs"), m_qbsStdout.constData());

    // Change tracking for accesses to product.exports (positive).
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("tool.qbs", "exportingProduct.toolTags", "[]");
    QCOMPARE(runQbs(QStringList({"-p", "MyTool"})), 0);
    QVERIFY2(m_qbsStdout.contains("creating MyTool.qbs"), m_qbsStdout.constData());
}

void TestBlackbox::externalLibs()
{
    QDir::setCurrent(testDataDir + "/external-libs");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::fileDependencies()
{
    QDir::setCurrent(testDataDir + "/fileDependencies");
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling narf.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling zort.cpp"));
    const QString productFileName = relativeExecutableFilePath("myapp", m_qbsStdout);
    QVERIFY2(regularFileExists(productFileName), qPrintable(productFileName));

    // Incremental build without changes.
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("compiling"));
    QVERIFY(!m_qbsStdout.contains("linking"));

    // Incremental build with changed file dependency.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("awesomelib/awesome.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling narf.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling zort.cpp"));

    // Incremental build with changed 2nd level file dependency.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("awesomelib/magnificent.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling narf.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling zort.cpp"));

    // Change the product in between to force the list of dependencies to get rescued.
    REPLACE_IN_FILE("fileDependencies.qbs", "//", "");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY(!m_qbsStdout.contains("compiling narf.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling zort.cpp"));
    WAIT_FOR_NEW_TIMESTAMP();
    touch("awesomelib/magnificent.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("[myapp] compiling narf.cpp"), m_qbsStdout.constData());
    QVERIFY(!m_qbsStdout.contains("compiling zort.cpp"));
}

void TestBlackbox::fileTagsFilterMerging()
{
    QDir::setCurrent(testDataDir + "/filetagsfilter-merging");
    QCOMPARE(runQbs(QStringList{"-f", "filetagsfilter-merging.qbs"}), 0);
    const QString installedApp
        = defaultInstallRoot + "/myapp/binDir/"
          + QFileInfo(relativeExecutableFilePath("myapp", m_qbsStdout)).fileName();
    QVERIFY2(QFile::exists(installedApp), qPrintable(installedApp));
    const QString otherOutput = relativeProductBuildDir("myapp") + "/myapp.txt";
    QVERIFY2(QFile::exists(otherOutput), qPrintable(otherOutput));
}

void TestBlackbox::flatbuf()
{
    QFETCH(QString, projectFile);

    QDir::setCurrent(testDataDir + "/flatbuf");

    rmDirR(relativeBuildDir());
    if (!prepareAndRunConan())
        QSKIP("conan is not prepared, check messages above");

    QbsRunParameters resolveParams(
        "resolve", QStringList{"-f", projectFile, "moduleProviders.conan.installDirectory:build"});
    QCOMPARE(runQbs(resolveParams), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    const bool withFlatbuffers = m_qbsStdout.contains("has flatbuffers: true");
    const bool withoutFlatbuffers = m_qbsStdout.contains("has flatbuffers: false");
    QVERIFY2(withFlatbuffers || withoutFlatbuffers, m_qbsStdout.constData());
    if (withoutFlatbuffers)
        QSKIP("flatbuf module not present");
    QbsRunParameters runParams("run");
    QCOMPARE(runQbs(runParams), 0);
}

void TestBlackbox::flatbuf_data()
{
    QTest::addColumn<QString>("projectFile");

    // QTest::newRow("c") << QString("flat_c.qbs");
    QTest::newRow("cpp") << QString("flat_cpp.qbs");
    QTest::newRow("relative import") << QString("flat_relative_import.qbs");
    QTest::newRow("absolute import") << QString("flat_absolute_import.qbs");
    QTest::newRow("filename suffix") << QString("flat_filename_suffix.qbs");
    QTest::newRow("filename extension") << QString("flat_filename_extension.qbs");
    QTest::newRow("keep prefix") << QString("flat_keep_prefix.qbs");
}

void TestBlackbox::freedesktop()
{
    if (!HostOsInfo::isAnyUnixHost())
        QSKIP("only applies on Unix");
    if (HostOsInfo::isMacosHost())
        QSKIP("Does not apply on macOS");
    QDir::setCurrent(testDataDir + "/freedesktop");
    QCOMPARE(runQbs(), 0);
    if (m_qbsStdout.contains("is emscripten: true"))
        QSKIP("Irrelevant for emscripten");
    QVERIFY(m_qbsStdout.contains("is emscripten: false"));

    // Check desktop file
    QString desktopFilePath =
        defaultInstallRoot + "/usr/local/share/applications/myapp.desktop";
    QVERIFY(QFile::exists(desktopFilePath));
    QFile desktopFile(desktopFilePath);
    QVERIFY2(desktopFile.open(QIODevice::ReadOnly), qPrintable(desktopFile.errorString()));
    QByteArrayList lines = desktopFile.readAll().split('\n');
    // Automatically filled line:
    QVERIFY(lines.contains("Exec=main"));
    // Name specified in `freedesktop.name` property
    QVERIFY(lines.contains("Name=My App"));
    // Overridden line:
    QVERIFY(lines.contains("Icon=myapp.png"));
    // Untouched line:
    QVERIFY(lines.contains("Terminal=false"));
    // Untouched localized line:
    QVERIFY(lines.contains("GenericName[fr]=Éditeur d'images"));
    // Untouched localized line:
    QVERIFY(lines.contains("Comment[fr]=Créer des images et éditer des photographies"));

    // Check AppStream file
    QVERIFY(QFile::exists(defaultInstallRoot +
                          "/usr/local/share/metainfo/myapp.appdata.xml"));

    // Check hicolor icon files
    QVERIFY(
        QFile::exists(defaultInstallRoot + "/usr/local/share/icons/hicolor/48x48/apps/myapp.png"));
    QVERIFY(QFile::exists(
        defaultInstallRoot + "/usr/local/share/icons/hicolor/48x48@2/apps/myapp.png"));
    QVERIFY(
        QFile::exists(defaultInstallRoot + "/usr/local/share/icons/hicolor/64x64/apps/myapp.png"));
    QVERIFY(QFile::exists(
        defaultInstallRoot + "/usr/local/share/icons/hicolor/64x64@2/apps/myapp.png"));
    QVERIFY(QFile::exists(
        defaultInstallRoot
        + "/usr/local/share/icons/hicolor/64x64/mimetypes/application-format.png"));
    QVERIFY(QFile::exists(
        defaultInstallRoot + "/usr/local/share/icons/hicolor/scalable/apps/myapp.png"));
}

void TestBlackbox::installedTransformerOutput()
{
    QDir::setCurrent(testDataDir + "/installed-transformer-output");
    QCOMPARE(runQbs(), 0);
    const QString installedFilePath = defaultInstallRoot + "/textfiles/HelloWorld.txt";
    QVERIFY2(QFile::exists(installedFilePath), qPrintable(installedFilePath));
}

void TestBlackbox::installLocations_data()
{
    QTest::addColumn<QString>("binDir");
    QTest::addColumn<QString>("dllDir");
    QTest::addColumn<QString>("libDir");
    QTest::addColumn<QString>("pluginDir");
    QTest::addColumn<QString>("dsymDir");
    QTest::addColumn<bool>("useModule");
    QTest::addColumn<bool>("useInstallPaths");
    QTest::newRow("explicit values, direct")
        << QString("bindir") << QString("dlldir") << QString("libdir") << QString("pluginDir")
        << QString("dsymDir") << false << false;
    QTest::newRow("explicit values, using config.install module")
        << QString("bindir") << QString("dlldir") << QString("libdir") << QString("pluginDir")
        << QString("dsymDir") << true << false;
    QTest::newRow("explicit values, using installpaths module")
        << QString("bindir") << QString("dlldir") << QString("libdir") << QString("pluginDir")
        << QString("dsymDir") << false << true;
    QTest::newRow("default values")
        << QString() << QString() << QString() << QString() << QString() << false << false;
}

void TestBlackbox::installLocations()
{
    QDir::setCurrent(testDataDir + "/install-locations");
    QFETCH(QString, binDir);
    QFETCH(QString, dllDir);
    QFETCH(QString, libDir);
    QFETCH(QString, pluginDir);
    QFETCH(QString, dsymDir);
    QFETCH(bool, useModule);
    QFETCH(bool, useInstallPaths);
    QbsRunParameters params("resolve");
    if (!binDir.isEmpty()) {
        const auto prop = useModule
                              ? (useInstallPaths ? "modules.installpaths.bin:"
                                                 : "modules.config.install.binariesDirectory:")
                              : "products.theapp.installDir:";
        params.arguments.push_back(prop + binDir);
        if (useModule && !useInstallPaths) {
            params.arguments.push_back("modules.config.install.applicationsDirectory:" + binDir);
        }
    }
    if (!dllDir.isEmpty()) {
        const auto prop = useModule
                              ? (useInstallPaths
                                     ? "modules.installpaths.lib:"
                                     : "modules.config.install.dynamicLibrariesDirectory:")
                              : "products.thelib.installDir:";
        params.arguments.push_back(prop + dllDir);
        if (!useModule)
            params.arguments.push_back("products.theloadablemodule.installDir:" + dllDir);
        if (useModule && !useInstallPaths) {
            params.arguments.push_back("modules.config.install.frameworksDirectory:" + dllDir);
        }
    }
    if (!libDir.isEmpty()) {
        const auto prop = useModule ? "modules.config.install.importLibrariesDirectory:"
                                    : "products.thelib.importLibInstallDir:";
        params.arguments.push_back(prop + libDir);
    }
    if (!pluginDir.isEmpty()) {
        const auto prop = useModule
                              ? (useInstallPaths ? "modules.installpaths.plugins:"
                                                 : "modules.config.install.pluginsDirectory:")
                              : "products.theplugin.installDir:";
        params.arguments.push_back(prop + pluginDir);
        if (useModule && !useInstallPaths) {
            params.arguments.push_back(
                "modules.config.install.loadableModulesDirectory:" + pluginDir);
        }
    }
    if (!dsymDir.isEmpty()) {
        if (useModule) {
            params.arguments.push_back(
                "modules.config.install.debugInformationDirectory:" + dsymDir);
        } else {
            params.arguments.push_back("products.theapp.debugInformationInstallDir:" + dsymDir);
            params.arguments.push_back("products.thelib.debugInformationInstallDir:" + dsymDir);
            params.arguments.push_back("products.theloadablemodule.debugInformationInstallDir:" + dsymDir);
            params.arguments.push_back("products.theplugin.debugInformationInstallDir:" + dsymDir);
        }
    }
    QCOMPARE(runQbs(params), 0);
    const bool isWindows = m_qbsStdout.contains("is windows");
    const bool isDarwin = m_qbsStdout.contains("is darwin");
    const bool isMac = m_qbsStdout.contains("is mac");
    const bool isUnix = m_qbsStdout.contains("is unix");
    const bool isMingw = m_qbsStdout.contains("is mingw");
    const bool isEmscripten = m_qbsStdout.contains("is emscripten");
    QVERIFY(isWindows || isDarwin || isUnix);
    QCOMPARE(runQbs(QbsRunParameters(QStringList("--clean-install-root"))), 0);

    struct BinaryInfo
    {
        QString fileName;
        QString installDir;
        QString subDir;

        QString absolutePath(const QString &prefix) const
        {
            return QDir::cleanPath(prefix + '/' + installDir + '/' + subDir + '/' + fileName);
        }
    };

    const BinaryInfo dll = {
        isWindows  ? "thelib.dll"
        : isDarwin ? "thelib"
                   : "libthelib.so",
        dllDir.isEmpty() ? (isDarwin ? "/Library/Frameworks" : (isWindows ? "/bin" : "/lib"))
                         : (isWindows && useModule && useInstallPaths ? binDir : dllDir),
        isDarwin ? "thelib.framework" : ""};
    const BinaryInfo dllDsym = {
        isWindows  ? (!isMingw ? "thelib.pdb" : "thelib.dll.debug")
        : isDarwin ? "thelib.framework.dSYM"
                   : "libthelib.so.debug",
        dsymDir.isEmpty() ? dll.installDir : dsymDir,
        {}};
    const BinaryInfo plugin = {
        isWindows  ? "theplugin.dll"
        : isDarwin ? "theplugin"
                   : "libtheplugin.so",
        pluginDir.isEmpty()
            ? (isDarwin ? "/Library/Install-Locations/PlugIns" : "/lib/install-locations/plugins/")
            : pluginDir,
        isDarwin ? (isMac ? "theplugin.bundle/Contents/MacOS" : "theplugin.bundle") : ""};
    const BinaryInfo pluginDsym = {
        isWindows  ? (!isMingw ? "theplugin.pdb" : "theplugin.dll.debug")
        : isDarwin ? "theplugin.bundle.dSYM"
                   : "libtheplugin.so.debug",
        dsymDir.isEmpty() ? plugin.installDir : dsymDir,
        {}};
    const BinaryInfo app = {
        isWindows      ? "theapp.exe"
        : isEmscripten ? "theapp.js"
                       : "theapp",
        binDir.isEmpty() ? isDarwin ? "/Applications" : "/bin" : binDir,
        isDarwin ? isMac ? "theapp.app/Contents/MacOS" : "theapp.app" : ""};
    const BinaryInfo appDsym = {
        isWindows      ? (!isMingw ? "theapp.pdb" : "theapp.exe.debug")
        : isDarwin     ? "theapp.app.dSYM"
        : isEmscripten ? "theapp.wasm.debug.wasm"
                       : "theapp.debug",
        dsymDir.isEmpty() ? app.installDir : dsymDir,
        {}};
    const BinaryInfo loadableModule = {
        isWindows  ? "theloadablemodule.dll"
        : isDarwin ? "theloadablemodule"
                   : "libtheloadablemodule.so",
        dllDir.isEmpty() ? (isDarwin ? "/Library/Frameworks" : (isWindows ? "/bin" : "/lib"))
                         : (isWindows && useModule && useInstallPaths ? binDir : dllDir),
        isDarwin ? (isMac ? "theloadablemodule.bundle/Contents/MacOS" : "theloadablemodule.bundle") : ""};
    const BinaryInfo loadableModuleDsym = {
        isWindows  ? (!isMingw ? "theloadablemodule.pdb" : "theloadablemodule.dll.debug")
        : isDarwin ? "theloadablemodule.bundle.dSYM"
                   : "libtheloadablemodule.so.debug",
        dsymDir.isEmpty() ? loadableModule.installDir : dsymDir,
        {}};

    const QString installRoot = QDir::currentPath() + "/default/install-root";
    const QString installPrefix = (isWindows || isEmscripten) ? QString() : "/usr/local";
    const QString fullInstallPrefix = installRoot + '/' + installPrefix + '/';
    const QString appFilePath = app.absolutePath(fullInstallPrefix);
    QVERIFY2(QFile::exists(appFilePath), qPrintable(appFilePath));
    const QString dllFilePath = dll.absolutePath(fullInstallPrefix);
    QVERIFY2(QFile::exists(dllFilePath), qPrintable(dllFilePath));
    if (isWindows && !isEmscripten) {
        const BinaryInfo lib = {"thelib.lib", libDir.isEmpty() ? "/lib" : libDir, ""};
        const QString libFilePath = lib.absolutePath(fullInstallPrefix);
        QVERIFY2(QFile::exists(libFilePath), qPrintable(libFilePath));
    }
    const QString pluginFilePath = plugin.absolutePath(fullInstallPrefix);
    QVERIFY2(QFile::exists(pluginFilePath), qPrintable(pluginFilePath));
    const QString loadableModuleFilePath = loadableModule.absolutePath(fullInstallPrefix);
    QVERIFY2(QFile::exists(loadableModuleFilePath), qPrintable(loadableModuleFilePath));

    const QString appDsymFilePath = appDsym.absolutePath(fullInstallPrefix);
    QVERIFY2(QFileInfo(appDsymFilePath).exists(), qPrintable(appDsymFilePath));

    if (!isEmscripten) { // no separate debug info for emscripten libs
        const QString dllDsymFilePath = dllDsym.absolutePath(fullInstallPrefix);
        QVERIFY2(QFileInfo(dllDsymFilePath).exists(), qPrintable(dllDsymFilePath));
        const QString loadableModuleDsymFilePath = loadableModuleDsym.absolutePath(fullInstallPrefix);
        QVERIFY2(QFileInfo(loadableModuleDsymFilePath).exists(), qPrintable(loadableModuleDsymFilePath));
        const QString pluginDsymFilePath = pluginDsym.absolutePath(fullInstallPrefix);
        QVERIFY2(QFile::exists(pluginDsymFilePath), qPrintable(pluginDsymFilePath));
    }
}

void TestBlackbox::inputsFromDependencies()
{
    QDir::setCurrent(testDataDir + "/inputs-from-dependencies");
    QCOMPARE(runQbs(), 0);
    const QList<QByteArray> output = m_qbsStdout.trimmed().split('\n');
    QVERIFY2(output.contains((QDir::currentPath() + "/file1.txt").toUtf8()),
             m_qbsStdout.constData());
    QVERIFY2(output.contains((QDir::currentPath() + "/file2.txt").toUtf8()),
             m_qbsStdout.constData());
    QVERIFY2(output.contains((QDir::currentPath() + "/file3.txt").toUtf8()),
             m_qbsStdout.constData());
    QVERIFY2(!output.contains((QDir::currentPath() + "/file4.txt").toUtf8()),
             m_qbsStdout.constData());
}

void TestBlackbox::installPackage()
{
    if (HostOsInfo::hostOs() == HostOsInfo::HostOsWindows)
        QSKIP("Beware of the msys tar");
    QString binary = findArchiver("tar");
    if (binary.isEmpty())
        QSKIP("tar not found");
    MacosTarHealer tarHealer;
    QDir::setCurrent(testDataDir + "/installpackage");
    QCOMPARE(runQbs(), 0);
    const QString tarFilePath = relativeProductBuildDir("tar-package") + "/tar-package.tar.gz";
    QVERIFY2(regularFileExists(tarFilePath), qPrintable(tarFilePath));
    QProcess tarList;
    tarList.start(binary, QStringList() << "tf" << tarFilePath);
    QVERIFY2(tarList.waitForStarted(), qPrintable(tarList.errorString()));
    QVERIFY2(tarList.waitForFinished(), qPrintable(tarList.errorString()));
    const QList<QByteArray> outputLines = tarList.readAllStandardOutput().split('\n');
    QList<QByteArray> cleanOutputLines;
    for (const QByteArray &line : outputLines) {
        const QByteArray trimmedLine = line.trimmed();
        if (!trimmedLine.isEmpty())
            cleanOutputLines.push_back(trimmedLine);
    }
    QCOMPARE(cleanOutputLines.size(), 3);
    for (const QByteArray &line : std::as_const(cleanOutputLines)) {
        QVERIFY2(line.contains("public_tool") || line.contains("mylib") || line.contains("lib.h"),
                 line.constData());
    }
}

void TestBlackbox::installRootFromProjectFile()
{
    QDir::setCurrent(testDataDir + "/install-root-from-project-file");
    const QString installRoot = QDir::currentPath() + '/' + relativeBuildDir()
            + "/my-install-root/";
    QCOMPARE(runQbs(QbsRunParameters(QStringList("products.p.installRoot:" + installRoot))), 0);
    const QString installedFile = installRoot + "/install-prefix/install-dir/file.txt";
    QVERIFY2(QFile::exists(installedFile), qPrintable(installedFile));
}

void TestBlackbox::libraryType_data()
{
    QTest::addColumn<QString>("libraryType");
    QTest::newRow("dynamic") << QStringLiteral("dynamic");
    QTest::newRow("static") << QStringLiteral("static");
}

void TestBlackbox::libraryType()
{
    QFETCH(QString, libraryType);
    QDir::setCurrent(testDataDir + "/library-type");

    rmDirR(relativeBuildDir());

    QStringList args{QStringLiteral("modules.config.build.libraryType:") + libraryType};

    QCOMPARE(runQbs(QbsRunParameters("build", args)), 0);

    const bool isWindows = m_qbsStdout.contains("is windows: yes");
    const bool isDarwin = m_qbsStdout.contains("is darwin: yes");
    const bool isGcc = m_qbsStdout.contains("is gcc: yes");
    QVERIFY(isWindows || isDarwin || isGcc);

    const QString installRoot = QDir::currentPath() + '/' + relativeBuildDir() + "/install-root";

    QString filePath;
    if (libraryType == QLatin1String("dynamic")) {
        if (isWindows) {
            filePath = installRoot + QStringLiteral("/bin/mylib.dll");
        } else if (isDarwin) {
            filePath = installRoot + QStringLiteral("/lib/libmylib.dylib");
        } else if (isGcc) {
            filePath = installRoot + QStringLiteral("/lib/libmylib.so");
        }
    } else {
        if (isWindows && !isGcc) {
            filePath = installRoot + QStringLiteral("/lib/mylib.lib");
        } else {
            filePath = installRoot + QStringLiteral("/lib/libmylib.a");
        }
    }
    if (filePath.isEmpty())
        QSKIP("Cannot determine file name");

    QVERIFY2(QFileInfo::exists(filePath), qPrintable(filePath));
}

void TestBlackbox::pluginType_data()
{
    QTest::addColumn<QString>("pluginType");
    QTest::newRow("dynamic plugin") << QStringLiteral("pluginType:dynamic");
    QTest::newRow("static plugin") << QStringLiteral("pluginType:static");
    QTest::newRow("static library") << QStringLiteral("libraryType:static");
}

void TestBlackbox::pluginType()
{
    QFETCH(QString, pluginType);
    QDir::setCurrent(testDataDir + "/plugin-type");

    rmDirR(relativeBuildDir());

    QStringList args{QStringLiteral("modules.config.build.") + pluginType};

    QCOMPARE(runQbs(QbsRunParameters("build", args)), 0);

    const bool isWindows = m_qbsStdout.contains("is windows: yes");
    const bool isDarwin = m_qbsStdout.contains("is darwin: yes");
    const bool isGcc = m_qbsStdout.contains("is gcc: yes");
    QVERIFY(isWindows || isDarwin || isGcc);

    const QString installRoot = QDir::currentPath() + '/' + relativeBuildDir() + "/install-root";
    const QString pluginsDir = QStringLiteral("/lib/plugin_type/plugins");

    QString filePath;
    if (pluginType == QLatin1String("pluginType:dynamic")) {
        if (isWindows) {
            filePath = installRoot + pluginsDir + QStringLiteral("/myplugin.dll");
        } else if (isDarwin) {
            filePath = installRoot + pluginsDir + QStringLiteral("/myplugin.bundle");
        } else if (isGcc) {
            filePath = installRoot + pluginsDir + QStringLiteral("/libmyplugin.so");
        }
    } else {
        if (isWindows && !isGcc) {
            filePath = installRoot + pluginsDir + QStringLiteral("/myplugin.lib");
        } else {
            filePath = installRoot + pluginsDir + QStringLiteral("/libmyplugin.a");
        }
    }
    if (filePath.isEmpty())
        QSKIP("Cannot determine file name");

    QVERIFY2(QFileInfo::exists(filePath), qPrintable(filePath));
}

void TestBlackbox::installable()
{
    QDir::setCurrent(testDataDir + "/installable");
    QCOMPARE(runQbs(), 0);
    QFile installList(relativeProductBuildDir("install-list") + "/installed-files.txt");
    QVERIFY2(installList.open(QIODevice::ReadOnly), qPrintable(installList.errorString()));
    QCOMPARE(installList.readAll().count('\n'), 2);
}

void TestBlackbox::installableAsAuxiliaryInput()
{
    QDir::setCurrent(testDataDir + "/installable-as-auxiliary-input");
    QCOMPARE(runQbs({"resolve"}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QVERIFY2(m_qbsStdout.contains("f-impl"), m_qbsStdout.constData());
}

void TestBlackbox::installTree()
{
    QDir::setCurrent(testDataDir + "/install-tree");
    QbsRunParameters params;
    params.command = "install";
    QCOMPARE(runQbs(params), 0);
    const QString installRoot = relativeBuildDir() + "/install-root/";
    QVERIFY(QFile::exists(installRoot + "content/foo.txt"));
    QVERIFY(QFile::exists(installRoot + "content/subdir1/bar.txt"));
    QVERIFY(QFile::exists(installRoot + "content/subdir2/baz.txt"));
}

void TestBlackbox::invalidArtifactPath_data()
{
    QTest::addColumn<QString>("baseDir");
    QTest::addColumn<bool>("isValid");

    QTest::newRow("inside, normal case") << "subdir" << true;
    QTest::newRow("inside, build dir 1") << "project.buildDirectory" << true;
    QTest::newRow("inside, build dir 2") << "subdir/.." << true;
    QTest::newRow("outside, absolute") << "/tmp" << false;
    QTest::newRow("outside, relative 1") << "../../" << false;
    QTest::newRow("outside, relative 2") << "subdir/../../.." << false;
}

void TestBlackbox::invalidArtifactPath()
{
    QFETCH(QString, baseDir);
    QFETCH(bool, isValid);

    rmDirR(relativeBuildDir());
    QDir::setCurrent(testDataDir + "/invalid-artifact-path");
    QbsRunParameters params(QStringList("project.artifactDir:" + baseDir));
    params.expectFailure = !isValid;
    QCOMPARE(runQbs(params) == 0, isValid);
    if (!isValid)
        QVERIFY2(m_qbsStderr.contains("outside of build directory"), m_qbsStderr.constData());
}

void TestBlackbox::invalidCommandProperty_data()
{
    QTest::addColumn<QString>("errorType");

    QTest::newRow("assigning QObject") << QString("qobject");
    QTest::newRow("assigning input artifact") << QString("input");
    QTest::newRow("assigning other artifact") << QString("artifact");
}

void TestBlackbox::invalidCommandProperty()
{
    QDir::setCurrent(testDataDir + "/invalid-command-property");
    QFETCH(QString, errorType);
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("products.p.errorType:" + errorType))),
             0);
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("unsuitable"), m_qbsStderr.constData());
}

void TestBlackbox::invalidLibraryNames()
{
    QDir::setCurrent(testDataDir + "/invalid-library-names");
    QFETCH(QString, index);
    QFETCH(bool, success);
    QFETCH(QStringList, diagnostics);
    QbsRunParameters params(QStringList("project.valueIndex:" + index));
    params.expectFailure = !success;
    QCOMPARE(runQbs(params) == 0, success);
    for (const QString &diag : std::as_const(diagnostics))
        QVERIFY2(m_qbsStderr.contains(diag.toLocal8Bit()), m_qbsStderr.constData());
}

void TestBlackbox::invalidLibraryNames_data()
{
    QTest::addColumn<QString>("index");
    QTest::addColumn<bool>("success");
    QTest::addColumn<QStringList>("diagnostics");

    QTest::newRow("null") << "0" << false << QStringList("is null");
    QTest::newRow("undefined") << "1" << false << QStringList("is undefined");
    QTest::newRow("number") << "2" << false << QStringList("does not have string type");
    QTest::newRow("array") << "3" << false << QStringList("does not have string type");
    QTest::newRow("empty string") << "4" << true << (QStringList()
                                  << "WARNING: Removing empty string from value of property "
                                     "'cpp.dynamicLibraries' in product 'invalid-library-names'."
                                  << "WARNING: Removing empty string from value of property "
                                     "'cpp.staticLibraries' in product 'invalid-library-names'.");
}

void TestBlackbox::invalidExtensionInstantiation()
{
    rmDirR(relativeBuildDir());
    QDir::setCurrent(testDataDir + "/invalid-extension-instantiation");
    QbsRunParameters params;
    params.expectFailure = true;
    params.arguments << (QString("products.theProduct.extension:") + QTest::currentDataTag());
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("invalid-extension-instantiation.qbs:17")
             && m_qbsStderr.contains('\'' + QByteArray(QTest::currentDataTag())
                                     + "' cannot be instantiated"),
             m_qbsStderr.constData());
}

void TestBlackbox::invalidExtensionInstantiation_data()
{
    QTest::addColumn<bool>("dummy");

    QTest::newRow("Environment");
    QTest::newRow("File");
    QTest::newRow("FileInfo");
    QTest::newRow("Utilities");
}

void TestBlackbox::invalidInstallDir()
{
    QDir::setCurrent(testDataDir + "/invalid-install-dir");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("outside of install root"), m_qbsStderr.constData());
}

void TestBlackbox::cli()
{
    int status;
    findCli(&status);
    QCOMPARE(status, 0);

    const SettingsPtr s = settings();
    qbs::Profile p("qbs_autotests-cli", s.get());
    if (!p.exists())
        QSKIP("No suitable Common Language Infrastructure test profile");

    QDir::setCurrent(testDataDir + "/cli");
    QbsRunParameters params(QStringList() << "-f" << "dotnettest.qbs");
    params.profile = p.name();

    status = runQbs(params);
    if (p.value("cli.toolchainInstallPath").toString().isEmpty()
            && status != 0 && m_qbsStderr.contains("toolchainInstallPath"))
        QSKIP("cli.toolchainInstallPath not set and automatic detection failed");

    QCOMPARE(status, 0);
    rmDirR(relativeBuildDir());

    QbsRunParameters params2(QStringList() << "-f" << "fshello.qbs");
    params2.profile = p.name();
    QCOMPARE(runQbs(params2), 0);
    rmDirR(relativeBuildDir());
}

void TestBlackbox::combinedSources()
{
    QDir::setCurrent(testDataDir + "/combined-sources");
    QbsRunParameters params(QStringList("modules.cpp.combineCxxSources:false"));
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling combinable.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling uncombinable.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling amalgamated_theapp.cpp"));
    params.arguments = QStringList("modules.cpp.combineCxxSources:true");
    params.command = "resolve";
    QCOMPARE(runQbs(params), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    touch("combinable.cpp");
    touch("main.cpp");
    touch("uncombinable.cpp");
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling combinable.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling uncombinable.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling amalgamated_theapp.cpp"));
}

void TestBlackbox::commandFile()
{
    QDir::setCurrent(testDataDir + "/command-file");
    QbsRunParameters params(QStringList() << "-p" << "theLib");
    QCOMPARE(runQbs(params), 0);
    params.arguments = QStringList() << "-p" << "theApp";
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::compilerDefinesByLanguage()
{
    QDir::setCurrent(testDataDir + "/compilerDefinesByLanguage");
    QbsRunParameters params(QStringList { "-f", "compilerDefinesByLanguage.qbs" });
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::jsExtensionsFile()
{
    QDir::setCurrent(testDataDir + "/jsextensions-file");
    QFile fileToMove("tomove.txt");
    QVERIFY2(fileToMove.open(QIODevice::WriteOnly), qPrintable(fileToMove.errorString()));
    fileToMove.close();
    fileToMove.setPermissions(fileToMove.permissions() & ~(QFile::ReadUser | QFile::ReadOwner
                                                           | QFile::ReadGroup | QFile::ReadOther));
    QbsRunParameters params(QStringList() << "-f" << "file.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!QFileInfo("original.txt").exists());
    QFile copy("copy.txt");
    QVERIFY(copy.exists());
    QVERIFY(copy.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = copy.readAll().trimmed().split('\n');
    QCOMPARE(lines.size(), 2);
    QCOMPARE(lines.at(0).trimmed().constData(), "false");
    QCOMPARE(lines.at(1).trimmed().constData(), "true");
}

void TestBlackbox::jsExtensionsFileInfo()
{
    QDir::setCurrent(testDataDir + "/jsextensions-fileinfo");
    QbsRunParameters params(QStringList() << "-f" << "fileinfo.qbs");
    QCOMPARE(runQbs(params), 0);
    QFile output("output.txt");
    QVERIFY(output.exists());
    QVERIFY(output.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = output.readAll().trimmed().split('\n');
    QCOMPARE(lines.size(), 28);
    int i = 0;
    QCOMPARE(lines.at(i++).trimmed().constData(), "blubb");
    QCOMPARE(lines.at(i++).trimmed().constData(), qUtf8Printable(
                 QFileInfo(QDir::currentPath()).canonicalFilePath()));
    QCOMPARE(lines.at(i++).trimmed().constData(), "/usr/bin");
    QCOMPARE(lines.at(i++).trimmed().constData(), "blubb.tar");
    QCOMPARE(lines.at(i++).trimmed().constData(), "blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/tmp/blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "c:/tmp/blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "true");
    QCOMPARE(lines.at(i++).trimmed().constData(), HostOsInfo::isWindowsHost() ? "true" : "false");
    QCOMPARE(lines.at(i++).trimmed().constData(), "false");
    QCOMPARE(lines.at(i++).trimmed().constData(), "true");
    QCOMPARE(lines.at(i++).trimmed().constData(), "false");
    QCOMPARE(lines.at(i++).trimmed().constData(), "false");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/tmp/blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/tmp/blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/tmp");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/tmp");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/");
    QCOMPARE(lines.at(i++).trimmed().constData(), HostOsInfo::isWindowsHost() ? "d:/" : "d:");
    QCOMPARE(lines.at(i++).trimmed().constData(), "d:");
    QCOMPARE(lines.at(i++).trimmed().constData(), "d:/");
    QCOMPARE(lines.at(i++).trimmed().constData(), "blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "tmp/blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "../blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "\\tmp\\blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "c:\\tmp\\blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), qUtf8Printable(HostOsInfo::pathListSeparator()));
    QCOMPARE(lines.at(i++).trimmed().constData(), qUtf8Printable(HostOsInfo::pathSeparator()));
}

void TestBlackbox::jsExtensionsHost()
{
    QDir::setCurrent(testDataDir + "//jsextensions-host");
    QbsRunParameters params(QStringList { "-f", "host.qbs" });
    QCOMPARE(runQbs(params), 0);
    QFile output("output.txt");
    QVERIFY(output.exists());
    QVERIFY(output.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = output.readAll().trimmed().split('\n');
    QCOMPARE(lines.size(), 10);
    int i = 0;
    QCOMPARE(lines.at(i++).trimmed().constData(), "architecture: " +
             HostOsInfo::hostOSArchitecture());
    QStringList list;
    for (const auto &s : HostOsInfo::canonicalOSIdentifiers(HostOsInfo::hostOSIdentifier()))
        list.push_back(s);
    QCOMPARE(lines.at(i++).trimmed().constData(), "os: " + list.join(','));
    QCOMPARE(lines.at(i++).trimmed().constData(), "platform: " + HostOsInfo::hostOSIdentifier());
    QCOMPARE(lines.at(i++).trimmed().constData(), "osVersion: " +
             HostOsInfo::hostOsVersion().toString());
    QCOMPARE(lines.at(i++).trimmed().constData(), "osBuildVersion: " +
             (HostOsInfo::hostOsBuildVersion().isNull() ? "undefined" :
                                                         HostOsInfo::hostOsBuildVersion()));
    QCOMPARE(lines.at(i++).trimmed().constData(), "osVersionParts: " +
             HostOsInfo::hostOsVersion().toString(','));
    QCOMPARE(lines.at(i++).trimmed().constData(), "osVersionMajor: " + QString::number(
             HostOsInfo::hostOsVersion().majorVersion()));
    QCOMPARE(lines.at(i++).trimmed().constData(), "osVersionMinor: " + QString::number(
             HostOsInfo::hostOsVersion().minorVersion()));
    QCOMPARE(lines.at(i++).trimmed().constData(), "osVersionPatch: " + QString::number(
             HostOsInfo::hostOsVersion().patchLevel()));
    QString nullDevice = HostOsInfo::isWindowsHost() ? QStringLiteral("NUL") :
                                                       QStringLiteral("/dev/null");
    QCOMPARE(lines.at(i++).trimmed().constData(), "nullDevice: " + nullDevice);
}

void TestBlackbox::jsExtensionsProcess()
{
    QDir::setCurrent(testDataDir + "/jsextensions-process");
    QCOMPARE(runQbs({"resolve", {"-f", "process.qbs"}}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QbsRunParameters params(QStringList() << "-f" << "process.qbs");
    QCOMPARE(runQbs(params), 0);
    QFile output("output.txt");
    QVERIFY(output.exists());
    QVERIFY(output.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = output.readAll().trimmed().split('\n');
    QCOMPARE(lines.size(), 15);
    QCOMPARE(lines.at(0).trimmed().constData(), "0");
    QVERIFY(lines.at(1).startsWith("qbs "));
    QCOMPARE(lines.at(2).trimmed().constData(), "true");
    QCOMPARE(lines.at(3).trimmed().constData(), "true");
    QCOMPARE(lines.at(4).trimmed().constData(), "0");
    QVERIFY(lines.at(5).startsWith("qbs "));
    QCOMPARE(lines.at(6).trimmed().constData(), "Unknown error");
    QCOMPARE(lines.at(7).trimmed().constData(), "false");
    QVERIFY(lines.at(8).trimmed() != "Unknown error");
    QCOMPARE(lines.at(9).trimmed().constData(), "Unknown error");
    QCOMPARE(lines.at(10).trimmed().constData(), "-1");
    QVERIFY(lines.at(11).trimmed() != "Unknown error");
    QVERIFY(lines.at(12).startsWith("Error running "));
    QCOMPARE(lines.at(13).trimmed().constData(), "should be");
    QCOMPARE(lines.at(14).trimmed().constData(), "123");
}

void TestBlackbox::jsExtensionsPropertyList()
{
    if (!HostOsInfo::isMacosHost())
        QSKIP("temporarily only applies on macOS");

    QDir::setCurrent(testDataDir + "/jsextensions-propertylist");
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
    //QCOMPARE(file1Contents, file2.readAll()); // keys don't have guaranteed order
    QJsonParseError err1, err2;
    QCOMPARE(QJsonDocument::fromJson(file1Contents, &err1),
             QJsonDocument::fromJson(file2.readAll(), &err2));
    QVERIFY(err1.error == QJsonParseError::NoError && err2.error == QJsonParseError::NoError);
    QFile file4("test.openstep.plist");
    QVERIFY(file4.exists());
    QFile file5("test3.json");
    QVERIFY(file5.exists());
    QVERIFY(file5.open(QIODevice::ReadOnly));
    QVERIFY(file1Contents != file5.readAll());
}

void TestBlackbox::jsExtensionsTemporaryDir()
{
    QDir::setCurrent(testDataDir + "/jsextensions-temporarydir");
    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::jsExtensionsTextFile()
{
    QDir::setCurrent(testDataDir + "/jsextensions-textfile");
    QbsRunParameters params(QStringList() << "-f" << "textfile.qbs");
    QCOMPARE(runQbs(params), 0);
    QFile file1("file1.txt");
    QVERIFY(file1.exists());
    QVERIFY(file1.open(QIODevice::ReadOnly));
    QCOMPARE(file1.size(), qint64(0));
    QFile file2("file2.txt");
    QVERIFY(file2.exists());
    QVERIFY(file2.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = file2.readAll().trimmed().split('\n');
    QCOMPARE(lines.size(), 6);
    QCOMPARE(lines.at(0).trimmed().constData(), "false");
    QCOMPARE(lines.at(1).trimmed().constData(), "First line.");
    QCOMPARE(lines.at(2).trimmed().constData(), "Second line.");
    QCOMPARE(lines.at(3).trimmed().constData(), "Third line.");
    QCOMPARE(lines.at(4).trimmed().constData(), qPrintable(QDir::currentPath() + "/file1.txt"));
    QCOMPARE(lines.at(5).trimmed().constData(), "true");
}

void TestBlackbox::jsExtensionsBinaryFile()
{
    QDir::setCurrent(testDataDir + "/jsextensions-binaryfile");
    QbsRunParameters params(QStringList() << "-f" << "binaryfile.qbs");
    QCOMPARE(runQbs(params), 0);
    QFile source("source.dat");
    QVERIFY(source.exists());
    QVERIFY(source.open(QIODevice::ReadOnly));
    QCOMPARE(source.size(), qint64(0));
    QFile destination("destination.dat");
    QVERIFY(destination.exists());
    QVERIFY(destination.open(QIODevice::ReadOnly));
    const QByteArray data = destination.readAll();
    QCOMPARE(data.size(), 8);
    QCOMPARE(data.at(0), char(0x00));
    QCOMPARE(data.at(1), char(0x01));
    QCOMPARE(data.at(2), char(0x02));
    QCOMPARE(data.at(3), char(0x03));
    QCOMPARE(data.at(4), char(0x04));
    QCOMPARE(data.at(5), char(0x05));
    QCOMPARE(data.at(6), char(0x06));
    QCOMPARE(data.at(7), char(0xFF));
    QFile destination2("destination2.dat");
    QVERIFY(destination2.exists());
    QVERIFY(destination2.open(QIODevice::ReadOnly));
    QCOMPARE(destination2.readAll(), data);
}

void TestBlackbox::lastModuleCandidateBroken()
{
    QDir::setCurrent(testDataDir + "/last-module-candidate-broken");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Dependency 'Foo' not found for product "
                                  "'last-module-candidate-broken'"), m_qbsStderr);
}

void TestBlackbox::ld()
{
    QDir::setCurrent(testDataDir + "/ld");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::symbolLinkMode()
{
    if (!HostOsInfo::isAnyUnixHost())
        QSKIP("only applies on Unix");

    QDir::setCurrent(testDataDir + "/symbolLinkMode");

    QCOMPARE(runQbs({"resolve"}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QbsRunParameters params;
    params.command = "run";
    const QStringList commonArgs{"-p", "driver", "--setup-run-env-config",
                                 "ignore-lib-dependencies", "qbs.installPrefix:''"};

    rmDirR(relativeBuildDir());
    params.arguments = QStringList() << commonArgs << "project.shouldInstallLibrary:true";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("somefunction existed and it returned 42"),
             m_qbsStdout.constData());

    rmDirR(relativeBuildDir());
    params.arguments = QStringList() << commonArgs << "project.shouldInstallLibrary:false";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("somefunction did not exist"), m_qbsStdout.constData());

    rmDirR(relativeBuildDir());
    params.arguments = QStringList() << commonArgs << "project.lazy:false";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("Lib was loaded!\nmeow\n"), m_qbsStdout.constData());

    if (HostOsInfo::isMacosHost()) {
        rmDirR(relativeBuildDir());
        params.arguments = QStringList() << commonArgs << "project.lazy:true";
        QCOMPARE(runQbs(params), 0);
        QVERIFY2(m_qbsStdout.contains("meow\n") && m_qbsStdout.contains("Lib was loaded!\n"),
                 m_qbsStdout.constData());
    }
}

void TestBlackbox::linkerMode()
{
    if (!HostOsInfo::isAnyUnixHost())
        QSKIP("only applies on Unix");

    QDir::setCurrent(testDataDir + "/linkerMode");
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("qbs.installPrefix:''"))), 0);
    if (m_qbsStdout.contains("is emscripten: true"))
        QSKIP("not applicable for emscripten");
    QVERIFY(m_qbsStdout.contains("is emscripten: false"));
    QCOMPARE(runQbs(), 0);

    auto testCondition = [&](const QString &lang,
            const std::function<bool(const QByteArray &)> &condition) {
        if ((lang == "Objective-C" || lang == "Objective-C++")
                && HostOsInfo::hostOs() != HostOsInfo::HostOsMacos)
            return;
        const QString binary = defaultInstallRoot + "/LinkedProduct-" + lang;
        QProcess deptool;
        if (HostOsInfo::hostOs() == HostOsInfo::HostOsMacos)
            deptool.start("otool", QStringList() << "-L" << binary);
        else
            deptool.start("readelf", QStringList() << "-a" << binary);
        QVERIFY(deptool.waitForStarted());
        QVERIFY(deptool.waitForFinished());
        QByteArray deptoolOutput = deptool.readAllStandardOutput();
        if (HostOsInfo::hostOs() != HostOsInfo::HostOsMacos) {
            QList<QByteArray> lines = deptoolOutput.split('\n');
            int sz = lines.size();
            for (int i = 0; i < sz; ++i) {
                if (!lines.at(i).contains("NEEDED")) {
                    lines.removeAt(i--);
                    sz--;
                }
            }

            deptoolOutput = lines.join('\n');
        }
        QCOMPARE(deptool.exitCode(), 0);
        QVERIFY2(condition(deptoolOutput), deptoolOutput.constData());
    };

    const QStringList nocpplangs = QStringList() << "Assembly" << "C" << "Objective-C";
    for (const QString &lang : nocpplangs)
        testCondition(lang, [](const QByteArray &lddOutput) { return !lddOutput.contains("c++"); });

    const QStringList cpplangs = QStringList() << "C++" << "Objective-C++";
    for (const QString &lang : cpplangs)
        testCondition(lang, [](const QByteArray &lddOutput) { return lddOutput.contains("c++"); });

    const QStringList objclangs = QStringList() << "Objective-C" << "Objective-C++";
    for (const QString &lang : objclangs)
        testCondition(lang, [](const QByteArray &lddOutput) { return lddOutput.contains("objc"); });
}

void TestBlackbox::linkerVariant_data()
{
    QTest::addColumn<QString>("theType");
    QTest::newRow("default") << QString();
    QTest::newRow("bfd") << QString("bfd");
    QTest::newRow("gold") << QString("gold");
    QTest::newRow("mold") << QString("mold");
}

void TestBlackbox::linkerVariant()
{
    QDir::setCurrent(testDataDir + "/linker-variant");
    QFETCH(QString, theType);
    QStringList resolveArgs("--force-probe-execution");
    if (!theType.isEmpty())
        resolveArgs << ("products.p.linkerVariant:" + theType);
    QCOMPARE(runQbs(QbsRunParameters("resolve", resolveArgs)), 0);
    const bool isGcc = m_qbsStdout.contains("is GCC: true");
    const bool isNotGcc = m_qbsStdout.contains("is GCC: false");
    QVERIFY2(isGcc != isNotGcc, m_qbsStdout.constData());
    QbsRunParameters buildParams("build", QStringList{"--command-echo-mode", "command-line"});
    buildParams.expectFailure = true;
    runQbs(buildParams);
    if (isGcc && !theType.isEmpty())
        QCOMPARE(m_qbsStdout.count("-fuse-ld=" + theType.toLocal8Bit()), 1);
    else
        QVERIFY2(!m_qbsStdout.contains("-fuse-ld"), m_qbsStdout.constData());
}

void TestBlackbox::lexyacc()
{
    if (!lexYaccExist())
        QSKIP("lex or yacc not present");
    QDir::setCurrent(testDataDir + "/lexyacc/one-grammar");
    QCOMPARE(runQbs({"resolve"}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    const QString parserBinary = relativeExecutableFilePath("one-grammar", m_qbsStdout);
    QCOMPARE(runQbs(), 0);
    QProcess p;
    const QByteArray magicString = "add to PATH: ";
    const int magicStringIndex = m_qbsStdout.indexOf(magicString);
    if (magicStringIndex != -1) {
        const int newLineIndex = m_qbsStdout.indexOf('\n', magicStringIndex);
        QVERIFY(newLineIndex != -1);
        const int dirIndex = magicStringIndex + magicString.length();
        const QString dir = QString::fromLocal8Bit(m_qbsStdout.mid(dirIndex,
                                                                   newLineIndex - dirIndex));
        QProcessEnvironment env;
        env.insert("PATH", dir);
        p.setProcessEnvironment(env);
    }
    p.start(parserBinary, QStringList());
    QVERIFY2(p.waitForStarted(), qPrintable(p.errorString()));
    p.write("a && b || c && !d");
    p.closeWriteChannel();
    QVERIFY2(p.waitForFinished(), qPrintable(p.errorString()));
    QVERIFY2(p.exitCode() == 0, p.readAllStandardError().constData());
    const QByteArray parserOutput = p.readAllStandardOutput();
    QVERIFY2(parserOutput.contains("OR AND a b AND c NOT d"), parserOutput.constData());

    QDir::setCurrent(testDataDir + "/lexyacc/two-grammars");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);

    params.expectFailure = false;
    params.command = "resolve";
    params.arguments << (QStringList() << "modules.lex_yacc.uniqueSymbolPrefix:true");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStderr.contains("whatever"), m_qbsStderr.constData());
    params.arguments << "modules.lex_yacc.enableCompilerWarnings:true";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    const QByteArray outputToCheck = m_qbsStdout + m_qbsStderr;
    QVERIFY2(outputToCheck.contains("whatever"), outputToCheck.constData());
}

void TestBlackbox::lexyaccOutputs()
{
    if (!lexYaccExist())
        QSKIP("lex or yacc not present");

    QFETCH(QString, lexOutputFilePath);
    QFETCH(QString, yaccOutputFilePath);
    QbsRunParameters params;
    if (!lexOutputFilePath.isEmpty())
        params.arguments << "modules.lex_yacc.lexOutputFilePath:" + lexOutputFilePath;
    if (!yaccOutputFilePath.isEmpty())
        params.arguments << "modules.lex_yacc.yaccOutputFilePath:" + yaccOutputFilePath;

#define VERIFY_COMPILATION(file) \
    if (!file.isEmpty()) { \
        QByteArray expected = "compiling " + file.toUtf8(); \
        if (!m_qbsStdout.contains(expected)) { \
            qDebug() << "Expected output:" << expected; \
            qDebug() << "Actual output:" << m_qbsStdout; \
            QFAIL("Expected stdout content missing."); \
        } \
    }

    const auto version = bisonVersion();
    if (version >= qbs::Version(2, 6)) {
        // prefix only supported starting from bison 2.6
        QVERIFY(QDir::setCurrent(testDataDir + "/lexyacc/lex_prefix"));
        rmDirR(relativeBuildDir());
        QCOMPARE(runQbs(params), 0);
        VERIFY_COMPILATION(yaccOutputFilePath);
    }

    QVERIFY(QDir::setCurrent(testDataDir + "/lexyacc/lex_outfile"));
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(params), 0);
    VERIFY_COMPILATION(yaccOutputFilePath);

    if (version >= qbs::Version(2, 4)) {
        // output syntax was changed in bison 2.4
        QVERIFY(QDir::setCurrent(testDataDir + "/lexyacc/yacc_output"));
        rmDirR(relativeBuildDir());
        QCOMPARE(runQbs(params), 0);
        VERIFY_COMPILATION(lexOutputFilePath);
    }

#undef VERIFY_COMPILATION
}

void TestBlackbox::lexyaccOutputs_data()
{
    QTest::addColumn<QString>("lexOutputFilePath");
    QTest::addColumn<QString>("yaccOutputFilePath");
    QTest::newRow("none") << QString() << QString();
    QTest::newRow("lexOutputFilePath")
            << QString{"lex_luthor.cpp"} << QString();
    QTest::newRow("yaccOutputFilePath")
            << QString() << QString{"shaven_yak.cpp"};
}

void TestBlackbox::linkerLibraryDuplicates()
{
    QDir::setCurrent(testDataDir + "/linker-library-duplicates");
    rmDirR(relativeBuildDir());
    QFETCH(QString, removeDuplicateLibraries);
    QStringList runParams;
    if (!removeDuplicateLibraries.isEmpty())
        runParams.append(removeDuplicateLibraries);

    QCOMPARE(runQbs(QbsRunParameters("resolve", runParams)), 0);
    const bool isGcc = m_qbsStdout.contains("is gcc: true");
    const bool isNotGcc = m_qbsStdout.contains("is gcc: false");
    if (isNotGcc)
        QSKIP("linkerLibraryDuplicates test only applies to GCC toolchain");
    QVERIFY(isGcc);

    QCOMPARE(runQbs(QStringList { "--command-echo-mode", "command-line" }), 0);
    const QByteArrayList output = m_qbsStdout.split('\n');
    QByteArray linkLine;
    for (const QByteArray &line : output) {
        if (line.contains("main.cpp.o"))
            linkLine = line;
    }
    QVERIFY(!linkLine.isEmpty());

    /* Now check the the libraries appear just once. In order to avoid dealing
     * with the different file extensions used in different platforms, we check
     * only for the library base name. But we must also take into account that
     * the build directories of each library will contain the library base name,
     * so we now exclude them. */
    QByteArrayList elementsWithoutPath;
    for (const QByteArray &element: linkLine.split(' ')) {
        if (element.indexOf('/') < 0)
            elementsWithoutPath.append(element);
    }
    QByteArray pathLessLinkLine = elementsWithoutPath.join(' ');

    typedef QMap<QByteArray,int> ObjectCount;
    QFETCH(ObjectCount, expectedObjectCount);
    for (auto i = expectedObjectCount.begin();
         i != expectedObjectCount.end();
         i++) {
        QCOMPARE(pathLessLinkLine.count(i.key()), i.value());
    }
}

void TestBlackbox::linkerLibraryDuplicates_data()
{
    typedef QMap<QByteArray,int> ObjectCount;

    QTest::addColumn<QString>("removeDuplicateLibraries");
    QTest::addColumn<ObjectCount>("expectedObjectCount");

    QTest::newRow("default") <<
        QString() <<
        ObjectCount {
            { "lib1", 1 },
            { "lib2", 1 },
            { "lib3", 1 },
        };

    QTest::newRow("enabled") <<
        "modules.cpp.removeDuplicateLibraries:true" <<
        ObjectCount {
            { "lib1", 1 },
            { "lib2", 1 },
            { "lib3", 1 },
        };

    QTest::newRow("disabled") <<
        "modules.cpp.removeDuplicateLibraries:false" <<
        ObjectCount {
            { "lib1", 3 },
            { "lib2", 2 },
            { "lib3", 1 },
        };
}

void TestBlackbox::linkerScripts()
{
    const QString sourceDir = QDir::cleanPath(testDataDir + "/linkerscripts");
    QbsRunParameters runParams("resolve", {"qbs.installRoot:" + QDir::currentPath()});
    runParams.buildDirectory = sourceDir + "/build";
    runParams.workingDir = sourceDir;

    QCOMPARE(runQbs(runParams), 0);
    const bool isGcc = m_qbsStdout.contains("is Linux gcc: true");
    const bool isNotGcc = m_qbsStdout.contains("is Linux gcc: false");
    if (isNotGcc)
        QSKIP("linker script test only applies to Linux");
    QVERIFY(isGcc);

    runParams.command = "build";
    QCOMPARE(runQbs(runParams), 0);
    const QString output = QString::fromLocal8Bit(m_qbsStderr);
    const QRegularExpression pattern(QRegularExpression::anchoredPattern(".*---(.*)---.*"),
                                     QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = pattern.match(output);
    QVERIFY2(match.hasMatch(), qPrintable(output));
    QCOMPARE(pattern.captureCount(), 1);
    const QString nmPath = match.captured(1);
    if (!QFile::exists(nmPath))
        QSKIP("Cannot check for symbol presence: No nm found.");

    const auto verifySymbols = [nmPath](const QByteArrayList& symbols) -> bool {
        QProcess nm;
        nm.start(nmPath, QStringList(QDir::currentPath() + "/liblinkerscripts.so"));
        if (!nm.waitForStarted()) {
            qDebug() << "Wait for process started failed.";
            return false;
        }
        if (!nm.waitForFinished()) {
            qDebug() << "Wait for process finished failed.";
            return false;
        }
        if (nm.exitCode() != 0) {
            qDebug() << "nm returned exit code " << nm.exitCode();
            return false;
        }
        const QByteArray nmOutput = nm.readAllStandardOutput();
        for (const QByteArray& symbol : symbols) {
            if (!nmOutput.contains(symbol)) {
                qDebug() << "Expected symbol" << symbol
                         << "not found in" << nmOutput.constData();
                return false;
            }
        }
        return true;
    };

    QVERIFY(verifySymbols({"TEST_SYMBOL1",
                           "TEST_SYMBOL2",
                           "TEST_SYMBOL_FROM_INCLUDE",
                           "TEST_SYMBOL_FROM_DIRECTORY",
                           "TEST_SYMBOL_FROM_RECURSIVE"}));
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(sourceDir + "/linkerscript_to_include",
                    "TEST_SYMBOL_FROM_INCLUDE = 1;",
                    "TEST_SYMBOL_FROM_INCLUDE_MODIFIED = 1;\n");
    QCOMPARE(runQbs(runParams), 0);
    QVERIFY2(m_qbsStdout.contains("linking liblinkerscripts.so"),
             "No linking after modifying included file");
    QVERIFY(verifySymbols({"TEST_SYMBOL1",
                           "TEST_SYMBOL2",
                           "TEST_SYMBOL_FROM_INCLUDE_MODIFIED",
                           "TEST_SYMBOL_FROM_DIRECTORY",
                           "TEST_SYMBOL_FROM_RECURSIVE"}));
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(sourceDir + "/scripts/linkerscript_in_directory",
                    "TEST_SYMBOL_FROM_DIRECTORY = 1;\n",
                    "TEST_SYMBOL_FROM_DIRECTORY_MODIFIED = 1;\n");
    QCOMPARE(runQbs(runParams), 0);
    QVERIFY2(m_qbsStdout.contains("linking liblinkerscripts.so"),
             "No linking after modifying file in directory");
    QVERIFY(verifySymbols({"TEST_SYMBOL1",
                           "TEST_SYMBOL2",
                           "TEST_SYMBOL_FROM_INCLUDE_MODIFIED",
                           "TEST_SYMBOL_FROM_DIRECTORY_MODIFIED",
                           "TEST_SYMBOL_FROM_RECURSIVE"}));
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(sourceDir + "/linkerscript_recursive",
                    "TEST_SYMBOL_FROM_RECURSIVE = 1;\n",
                    "TEST_SYMBOL_FROM_RECURSIVE_MODIFIED = 1;\n");
    QCOMPARE(runQbs(runParams), 0);
    QVERIFY2(m_qbsStdout.contains("linking liblinkerscripts.so"),
             "No linking after modifying recursive file");
    QVERIFY(verifySymbols({"TEST_SYMBOL1",
                           "TEST_SYMBOL2",
                           "TEST_SYMBOL_FROM_INCLUDE_MODIFIED",
                           "TEST_SYMBOL_FROM_DIRECTORY_MODIFIED",
                           "TEST_SYMBOL_FROM_RECURSIVE_MODIFIED"}));
}

void TestBlackbox::linkerModuleDefinition()
{
    QDir::setCurrent(testDataDir + "/linker-module-definition");
    QCOMPARE(runQbs({"build"}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QCOMPARE(runQbs({"run"}), 0);
    const auto verifyOutput = [this](const QByteArrayList &symbols) {
        for (const QByteArray &symbol : symbols) {
            if (!m_qbsStdout.contains(symbol)) {
                qDebug() << "Expected symbol" << symbol
                         << "not found in" << m_qbsStdout;
                return false;
            }
        }
        return true;
    };
    QVERIFY(verifyOutput({"foo", "bar"}));
}

void TestBlackbox::listProducts()
{
    QDir::setCurrent(testDataDir + "/list-products");
    QCOMPARE(runQbs(QbsRunParameters("list-products")), 0);
    m_qbsStdout.replace("\r\n", "\n");
    QVERIFY2(m_qbsStdout.contains(
                 "a\n"
                 "b {\"architecture\":\"mips\",\"buildVariant\":\"debug\"}\n"
                 "b {\"architecture\":\"mips\",\"buildVariant\":\"release\"}\n"
                 "b {\"architecture\":\"vax\",\"buildVariant\":\"debug\"}\n"
                 "b {\"architecture\":\"vax\",\"buildVariant\":\"release\"}\n"
                 "c\n"), m_qbsStdout.constData());
}

void TestBlackbox::listPropertiesWithOuter()
{
    QDir::setCurrent(testDataDir + "/list-properties-with-outer");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("listProp: [\"product\",\"higher\",\"group\"]"),
             m_qbsStdout.constData());
}

void TestBlackbox::listPropertyOrder()
{
    QDir::setCurrent(testDataDir + "/list-property-order");
    const QbsRunParameters params(QStringList() << "-q");
    QCOMPARE(runQbs(params), 0);
    const QByteArray firstOutput = m_qbsStderr;
    QVERIFY(firstOutput.contains("listProp = [\"product\",\"higher3\",\"higher2\",\"higher1\",\"lower\"]"));
    for (int i = 0; i < 25; ++i) {
        rmDirR(relativeBuildDir());
        QCOMPARE(runQbs(params), 0);
        if (m_qbsStderr != firstOutput)
            break;
    }
    QCOMPARE(m_qbsStderr.constData(), firstOutput.constData());
}

void TestBlackbox::require()
{
    QDir::setCurrent(testDataDir + "/require");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::rescueTransformerData()
{
    QDir::setCurrent(testDataDir + "/rescue-transformer-data");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp") && m_qbsStdout.contains("m.p: undefined"),
             m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    touch("main.cpp");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp") && !m_qbsStdout.contains("m.p: "),
             m_qbsStdout.constData());
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("modules.m.p:true"))), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp") && m_qbsStdout.contains("m.p: true"),
             m_qbsStdout.constData());
}

void TestBlackbox::multipleChanges()
{
    QDir::setCurrent(testDataDir + "/multiple-changes");
    QCOMPARE(runQbs(), 0);
    QFile newFile("test.blubb");
    QVERIFY(newFile.open(QIODevice::WriteOnly));
    newFile.close();
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList() << "project.prop:true")), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("prop: true"));
}

void TestBlackbox::multipleConfigurations()
{
    QDir::setCurrent(testDataDir + "/multiple-configurations");
    QbsRunParameters params(QStringList{"config:x", "config:y", "config:z"});
    params.profile.clear();
    struct DefaultProfileSwitcher
    {
        DefaultProfileSwitcher()
        {
            const SettingsPtr s = settings();
            oldDefaultProfile = s->defaultProfile();
            s->setValue("defaultProfile", profileName());
            s->sync();
        }
        ~DefaultProfileSwitcher()
        {
            const SettingsPtr s = settings();
            s->setValue("defaultProfile", oldDefaultProfile);
            s->sync();
        }
        QVariant oldDefaultProfile;
    };
    DefaultProfileSwitcher dps;
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStdout.count("compiling lib.cpp"), 3);
    QCOMPARE(m_qbsStdout.count("compiling file.cpp"), 3);
    QCOMPARE(m_qbsStdout.count("compiling main.cpp"), 3);
}

void TestBlackbox::multiplexedTool()
{
    QDir::setCurrent(testDataDir + "/multiplexed-tool");
    QCOMPARE(runQbs(QStringLiteral("resolve")), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QCOMPARE(runQbs(), 0);
    QCOMPARE(m_qbsStdout.count("creating tool.out"), 4);
}

void TestBlackbox::nestedGroups()
{
    QDir::setCurrent(testDataDir + "/nested-groups");
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(relativeExecutableFilePath("nested-groups", m_qbsStdout)));
}

void TestBlackbox::nestedProperties()
{
    QDir::setCurrent(testDataDir + "/nested-properties");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("value in higherlevel"), m_qbsStdout.constData());
}

void TestBlackbox::newOutputArtifact()
{
    QDir::setCurrent(testDataDir + "/new-output-artifact");
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(relativeBuildDir() + "/install-root/output_98.out"));
    const QString the100thArtifact = relativeBuildDir() + "/install-root/output_99.out";
    QVERIFY(!regularFileExists(the100thArtifact));
    QbsRunParameters params("resolve", QStringList() << "products.theProduct.artifactCount:100");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(the100thArtifact));
}

void TestBlackbox::noExportedSymbols_data()
{
    QTest::addColumn<bool>("link");
    QTest::addColumn<bool>("dryRun");
    QTest::newRow("link") << true << false;
    QTest::newRow("link (dry run)") << true << true;
    QTest::newRow("do not link") << false << false;
}

void TestBlackbox::noExportedSymbols()
{
    QDir::setCurrent(testDataDir + "/no-exported-symbols");
    QFETCH(bool, link);
    QFETCH(bool, dryRun);
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList{"--force-probe-execution",
            QString("products.the_app.link:") + (link ? "true" : "false")})), 0);
    const bool isMsvc = m_qbsStdout.contains("compiler is MSVC");
    const bool isNotMsvc = m_qbsStdout.contains("compiler is not MSVC");
    QVERIFY2(isMsvc || isNotMsvc, m_qbsStdout.constData());
    if (isNotMsvc)
        QSKIP("Test applies with MSVC only");
    QbsRunParameters buildParams;
    if (dryRun)
        buildParams.arguments << "--dry-run";
    buildParams.expectFailure = link && !dryRun;
    QCOMPARE(runQbs(buildParams) == 0, !buildParams.expectFailure);
    QVERIFY2(m_qbsStderr.contains("This typically happens when a DLL does not export "
                                  "any symbols.") == buildParams.expectFailure,
             m_qbsStderr.constData());
}

void TestBlackbox::noProfile()
{
    QDir::setCurrent(testDataDir + "/no-profile");
    QbsRunParameters params;
    params.profile = "none";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("profile: none"), m_qbsStdout.constData());
}

void TestBlackbox::noSuchProfile()
{
    QDir::setCurrent(testDataDir + "/no-such-profile");
    QbsRunParameters params(QStringList("products.theProduct.p:1"));
    params.profile = "jibbetnich";
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Profile 'jibbetnich' does not exist"), m_qbsStderr.constData());
}

void TestBlackbox::nonBrokenFilesInBrokenProduct()
{
    QDir::setCurrent(testDataDir + "/non-broken-files-in-broken-product");
    QbsRunParameters params(QStringList() << "-k");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStdout.contains("fine.cpp"));
    QVERIFY(runQbs(params) != 0);
    QVERIFY(!m_qbsStdout.contains("fine.cpp")); // The non-broken file must not be recompiled.
}

void TestBlackbox::nonDefaultProduct()
{
    QDir::setCurrent(testDataDir + "/non-default-product");

    QCOMPARE(runQbs(), 0);
    const QString defaultAppExe = relativeExecutableFilePath("default app", m_qbsStdout);
    const QString nonDefaultAppExe = relativeExecutableFilePath("non-default app", m_qbsStdout);
    QVERIFY2(QFile::exists(defaultAppExe), qPrintable(defaultAppExe));
    QVERIFY2(!QFile::exists(nonDefaultAppExe), qPrintable(nonDefaultAppExe));

    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "--all-products")), 0);
    QVERIFY2(QFile::exists(nonDefaultAppExe), qPrintable(nonDefaultAppExe));
}

void TestBlackbox::notAlwaysUpdated()
{
    QDir::setCurrent(testDataDir + "/not-always-updated");
    QCOMPARE(runQbs(), 0);
    QCOMPARE(runQbs(), 0);
}

static void switchProfileContents(qbs::Profile &p, qbs::Settings *s, bool on)
{
    const QString scalarKey = "leaf.scalarProp";
    const QString listKey = "leaf.listProp";
    if (on) {
        p.setValue(scalarKey, "profile");
        p.setValue(listKey, QStringList() << "profile");
    } else {
        p.remove(scalarKey);
        p.remove(listKey);
    }
    s->sync();
}

static void switchFileContents(QFile &f, bool on)
{
    f.seek(0);
    QByteArray contents = f.readAll();
    f.resize(0);
    if (on)
        contents.replace("// leaf.", "leaf.");
    else
        contents.replace("leaf.", "// leaf.");
    f.write(contents);
    f.flush();
}

void TestBlackbox::propertyPrecedence()
{
    QDir::setCurrent(testDataDir + "/property-precedence");
    const SettingsPtr s = settings();
    qbs::Internal::TemporaryProfile profile("qbs_autotests_propPrecedence", s.get());
    profile.p.setValue("qbs.architecture", "x86"); // Profiles must not be empty...
    s->sync();
    const QStringList args = QStringList() << "-f" << "property-precedence.qbs";
    QbsRunParameters params(args);
    params.profile = profile.p.name();
    QbsRunParameters resolveParams = params;
    resolveParams.command = "resolve";

    // Case 1: [cmdline=0,prod=0,export=0,nonleaf=0,profile=0]
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());

    QVERIFY2(m_qbsStdout.contains("scalar prop: leaf\n")
             && m_qbsStdout.contains("list prop: [\"leaf\"]\n"),
             m_qbsStdout.constData());
    params.arguments.clear();

    // Case 2: [cmdline=0,prod=0,export=0,nonleaf=0,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());

    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: profile\n")
             && m_qbsStdout.contains("list prop: [\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 3: [cmdline=0,prod=0,export=0,nonleaf=1,profile=0]
    QFile nonleafFile("modules/nonleaf/nonleaf.qbs");
    QVERIFY2(nonleafFile.open(QIODevice::ReadWrite), qPrintable(nonleafFile.errorString()));
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, true);
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: nonleaf\n")
             && m_qbsStdout.contains("list prop: [\"nonleaf\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 4: [cmdline=0,prod=0,export=0,nonleaf=1,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: nonleaf\n")
             && m_qbsStdout.contains("list prop: [\"nonleaf\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 5: [cmdline=0,prod=0,export=1,nonleaf=0,profile=0]
    QFile depFile("dep.qbs");
    QVERIFY2(depFile.open(QIODevice::ReadWrite), qPrintable(depFile.errorString()));
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, true);
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: export\n")
             && m_qbsStdout.contains("list prop: [\"export\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 6: [cmdline=0,prod=0,export=1,nonleaf=0,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: export\n")
             && m_qbsStdout.contains("list prop: [\"export\",\"profile\"]\n"),
             m_qbsStdout.constData());


    // Case 7: [cmdline=0,prod=0,export=1,nonleaf=1,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, true);
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData()); QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: export\n")
             && m_qbsStdout.contains("list prop: [\"export\",\"nonleaf\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 8: [cmdline=0,prod=0,export=1,nonleaf=1,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: export\n")
             && m_qbsStdout.contains("list prop: [\"export\",\"nonleaf\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 9: [cmdline=0,prod=1,export=0,nonleaf=0,profile=0]
    QFile productFile("property-precedence.qbs");
    QVERIFY2(productFile.open(QIODevice::ReadWrite), qPrintable(productFile.errorString()));
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, false);
    switchFileContents(productFile, true);
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 10: [cmdline=0,prod=1,export=0,nonleaf=0,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 11: [cmdline=0,prod=1,export=0,nonleaf=1,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, true);
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"nonleaf\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 12: [cmdline=0,prod=1,export=0,nonleaf=1,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"nonleaf\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 13: [cmdline=0,prod=1,export=1,nonleaf=0,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, true);
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"export\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 14: [cmdline=0,prod=1,export=1,nonleaf=0,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"export\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 15: [cmdline=0,prod=1,export=1,nonleaf=1,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, true);
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"export\",\"nonleaf\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 16: [cmdline=0,prod=1,export=1,nonleaf=1,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"export\",\"nonleaf\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Command line properties wipe everything, including lists.
    // Case 17: [cmdline=1,prod=0,export=0,nonleaf=0,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, false);
    switchFileContents(productFile, false);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 18: [cmdline=1,prod=0,export=0,nonleaf=0,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 19: [cmdline=1,prod=0,export=0,nonleaf=1,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 20: [cmdline=1,prod=0,export=0,nonleaf=1,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 21: [cmdline=1,prod=0,export=1,nonleaf=0,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 22: [cmdline=1,prod=0,export=1,nonleaf=0,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 23: [cmdline=1,prod=0,export=1,nonleaf=1,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 24: [cmdline=1,prod=0,export=1,nonleaf=1,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 25: [cmdline=1,prod=1,export=0,nonleaf=0,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, false);
    switchFileContents(productFile, true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 26: [cmdline=1,prod=1,export=0,nonleaf=0,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 27: [cmdline=1,prod=1,export=0,nonleaf=1,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 28: [cmdline=1,prod=1,export=0,nonleaf=1,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 29: [cmdline=1,prod=1,export=1,nonleaf=0,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 30: [cmdline=1,prod=1,export=1,nonleaf=0,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 31: [cmdline=1,prod=1,export=1,nonleaf=1,profile=0]
    switchProfileContents(profile.p, s.get(), false);
    switchFileContents(nonleafFile, true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 32: [cmdline=1,prod=1,export=1,nonleaf=1,profile=1]
    switchProfileContents(profile.p, s.get(), true);
    resolveParams.arguments << "modules.leaf.scalarProp:cmdline" << "modules.leaf.listProp:cmdline";
    QCOMPARE(runQbs(resolveParams), 0);
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());
}

void TestBlackbox::productDependenciesByType()
{
    QDir::setCurrent(testDataDir + "/product-dependencies-by-type");
    QCOMPARE(runQbs(), 0);
    QFile appListFile(relativeProductBuildDir("app list") + "/app-list.txt");
    QVERIFY2(appListFile.open(QIODevice::ReadOnly), qPrintable(appListFile.fileName()));
    const QList<QByteArray> appList = appListFile.readAll().trimmed().split('\n');
    QCOMPARE(appList.size(), 6);
    QStringList apps = QStringList()
                       << QDir::currentPath() + '/'
                              + relativeExecutableFilePath("app1", m_qbsStdout)
                       << QDir::currentPath() + '/'
                              + relativeExecutableFilePath("app2", m_qbsStdout)
                       << QDir::currentPath() + '/'
                              + relativeExecutableFilePath("app3", m_qbsStdout)
                       << QDir::currentPath() + '/'
                              + relativeExecutableFilePath("app4", m_qbsStdout)
                       << QDir::currentPath() + '/' + relativeProductBuildDir("other-product")
                              + "/output.txt"
                       << QDir::currentPath() + '/' + relativeProductBuildDir("yet-another-product")
                              + "/output.txt";
    for (const QByteArray &line : appList) {
        const QString cleanLine = QString::fromLocal8Bit(line.trimmed());
        QVERIFY2(apps.removeOne(cleanLine), qPrintable(cleanLine));
    }
    QVERIFY(apps.empty());
}

void TestBlackbox::productInExportedModule()
{
    QDir::setCurrent(testDataDir + "/product-in-exported-module");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("product: importing"), m_qbsStdout.constData());
}

void TestBlackbox::properQuoting()
{
    QDir::setCurrent(testDataDir + "/proper quoting");
    QCOMPARE(runQbs(), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QbsRunParameters params(QStringLiteral("run"), QStringList() << "-q" << "-p" << "Hello World");
    params.expectFailure = true; // Because the exit code is non-zero.
    QCOMPARE(runQbs(params), 156);
    const char * const expectedOutput = "whitespaceless\ncontains space\ncontains\ttab\n"
            "backslash\\\nHello World! The magic number is 156.";
    QCOMPARE(unifiedLineEndings(m_qbsStdout).constData(), expectedOutput);
}

void TestBlackbox::propertiesInExportItems()
{
    QDir::setCurrent(testDataDir + "/properties-in-export-items");
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(relativeExecutableFilePath("p1", m_qbsStdout)));
    QVERIFY(regularFileExists(relativeExecutableFilePath("p2", m_qbsStdout)));
    QVERIFY2(m_qbsStderr.isEmpty(), m_qbsStderr.constData());
}

void TestBlackbox::protobuf_data()
{
    QTest::addColumn<QString>("projectFile");
    QTest::addColumn<QStringList>("properties");
    QTest::addColumn<bool>("hasModules");
    QTest::addColumn<bool>("successExpected");
    QTest::newRow("cpp-pkgconfig")
        << QString("addressbook_cpp.qbs")
        << QStringList({"project.qbsModuleProviders:qbspkgconfig"}) << true << true;
    QTest::newRow("cpp-conan") << QString("addressbook_cpp.qbs")
                               << QStringList(
                                      {"project.qbsModuleProviders:conan",
                                       "qbs.buildVariant:release",
                                       "moduleProviders.conan.installDirectory:build"})
                               << true << true;
    QTest::newRow("objc") << QString("addressbook_objc.qbs") << QStringList() << false << true;
    QTest::newRow("nanopb") << QString("addressbook_nanopb.qbs") << QStringList() << false << true;
    QTest::newRow("import") << QString("import.qbs") << QStringList() << true << true;
    QTest::newRow("missing import dir") << QString("needs-import-dir.qbs")
                                        << QStringList() << true << false;
    QTest::newRow("provided import dir")
            << QString("needs-import-dir.qbs")
            << QStringList("products.app.theImportDir:subdir") << true << true;
    QTest::newRow("create proto library")
            << QString("create-proto-library.qbs") << QStringList() << true << true;
}

void TestBlackbox::protobuf()
{
    QDir::setCurrent(testDataDir + "/protobuf");
    QFETCH(QString, projectFile);
    QFETCH(QStringList, properties);
    QFETCH(bool, hasModules);
    QFETCH(bool, successExpected);
    rmDirR(relativeBuildDir());

    if (QTest::currentDataTag() == QLatin1String("cpp-conan")) {
        if (!prepareAndRunConan())
            QSKIP("conan is not prepared, check messages above");
    }

    QbsRunParameters resolveParams("resolve", QStringList{"-f", projectFile} << properties);
    QCOMPARE(runQbs(resolveParams), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    const bool withProtobuf = m_qbsStdout.contains("has protobuf: true");
    const bool withoutProtobuf = m_qbsStdout.contains("has protobuf: false");
    QVERIFY2(withProtobuf || withoutProtobuf, m_qbsStdout.constData());
    if (withoutProtobuf)
        QSKIP("protobuf module not present");
    const bool hasMods = m_qbsStdout.contains("has modules: true");
    const bool dontHaveMods = m_qbsStdout.contains("has modules: false");
    QVERIFY2(hasMods == !dontHaveMods, m_qbsStdout.constData());
    QCOMPARE(hasMods, hasModules);
    QbsRunParameters runParams("run");
    runParams.expectFailure = !successExpected;
    QCOMPARE(runQbs(runParams) == 0, successExpected);
}

void TestBlackbox::protobufLibraryInstall()
{
    QDir::setCurrent(testDataDir + "/protobuf-library-install");
    rmDirR(relativeBuildDir());
    QbsRunParameters resolveParams("resolve", QStringList{"qbs.installPrefix:/usr/local"});
    QCOMPARE(runQbs(resolveParams), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    const bool withProtobuf = m_qbsStdout.contains("has protobuf: true");
    const bool withoutProtobuf = m_qbsStdout.contains("has protobuf: false");
    QVERIFY2(withProtobuf || withoutProtobuf, m_qbsStdout.constData());
    if (withoutProtobuf)
        QSKIP("protobuf module not present");
    QbsRunParameters buildParams("build");
    buildParams.expectFailure = false;
    QCOMPARE(runQbs(buildParams), 0);

    const QString installRootInclude = relativeBuildDir() + "/install-root/usr/local/include";
    QVERIFY(QFileInfo::exists(installRootInclude + "/hello.pb.h") &&
            QFileInfo::exists(installRootInclude + "/hello/world.pb.h"));
}

void TestBlackbox::pseudoMultiplexing()
{
    // This is "pseudo-multiplexing" on all platforms that initialize qbs.architectures
    // to an array with one element. See QBS-1243.
    QDir::setCurrent(testDataDir + "/pseudo-multiplexing");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::qbsConfig()
{
    QbsRunParameters params("config");
#ifdef QBS_ENABLE_UNIT_TESTS
    QTemporaryDir tempSystemSettingsDir;
    params.environment.insert("QBS_AUTOTEST_SYSTEM_SETTINGS_DIR", tempSystemSettingsDir.path());
    QTemporaryDir tempUserSettingsDir;
    QVERIFY(tempSystemSettingsDir.isValid());
    QVERIFY(tempUserSettingsDir.isValid());
    const QStringList settingsDirArgs = QStringList{"--settings-dir", tempUserSettingsDir.path()};

    // Set values.
    params.arguments = settingsDirArgs + QStringList{"--system", "key.subkey.scalar", "s"};
    QCOMPARE(runQbs(params), 0);
    params.arguments = settingsDirArgs + QStringList{"--system", "key.subkey.list", "['sl']"};
    QCOMPARE(runQbs(params), 0);
    params.arguments = settingsDirArgs + QStringList{"--user", "key.subkey.scalar", "u"};
    QCOMPARE(runQbs(params), 0);
    params.arguments = settingsDirArgs + QStringList{"key.subkey.list", "[\"u1\",\"u2\"]"};
    QCOMPARE(runQbs(params), 0);

    // Check outputs.
    const auto valueExtractor = [this] {
        const QByteArray trimmed = m_qbsStdout.trimmed();
        return trimmed.mid(trimmed.lastIndexOf(':') + 2);
    };
    params.arguments = settingsDirArgs + QStringList{"--list", "key.subkey.scalar"};
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(valueExtractor(), QByteArray("\"u\""));
    params.arguments = settingsDirArgs + QStringList{"--list", "--user", "key.subkey.scalar"};
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(valueExtractor(), QByteArray("\"u\""));
    params.arguments = settingsDirArgs + QStringList{"--list", "--system", "key.subkey.scalar"};
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(valueExtractor(), QByteArray("\"s\""));
    params.arguments = settingsDirArgs + QStringList{"--list", "key.subkey.list"};
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(valueExtractor(), QByteArray("[\"u1\", \"u2\", \"sl\"]"));
    params.arguments = settingsDirArgs + QStringList{"--list", "--user", "key.subkey.list"};
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(valueExtractor(), QByteArray("[\"u1\", \"u2\"]"));
    params.arguments = settingsDirArgs + QStringList{"--list", "--system", "key.subkey.list"};
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(valueExtractor(), QByteArray("[\"sl\"]"));

    // Remove some values and re-check.
    params.arguments = settingsDirArgs + QStringList{"--unset", "key.subkey.scalar"};
    QCOMPARE(runQbs(params), 0);
    params.arguments = settingsDirArgs + QStringList{"--system", "--unset", "key.subkey.list"};
    QCOMPARE(runQbs(params), 0);
    params.arguments = settingsDirArgs + QStringList{"--list", "key.subkey.scalar"};
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(valueExtractor(), QByteArray("\"s\""));
    params.arguments = settingsDirArgs + QStringList{"--list", "key.subkey.list"};
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(valueExtractor(), QByteArray("[\"u1\", \"u2\"]"));

    // Check preferences.ignoreSystemSearchPaths
    params.arguments = settingsDirArgs
            + QStringList{"--system", "preferences.qbsSearchPaths", "['/usr/lib/qbs']"};
    QCOMPARE(runQbs(params), 0);
    params.arguments = settingsDirArgs
            + QStringList{"preferences.qbsSearchPaths", "['/home/user/qbs']"};
    QCOMPARE(runQbs(params), 0);
    qbs::Settings settings(tempUserSettingsDir.path(), tempSystemSettingsDir.path());
    const qbs::Preferences prefs(&settings, "SomeProfile");
    QVERIFY2(prefs.searchPaths().contains("/usr/lib/qbs")
             && prefs.searchPaths().contains("/home/user/qbs"),
             qPrintable(prefs.searchPaths().join(',')));
    settings.setValue("profiles.SomeProfile.preferences.ignoreSystemSearchPaths", true);
    QVERIFY2(!prefs.searchPaths().contains("/usr/lib/qbs")
             && prefs.searchPaths().contains("/home/user/qbs"),
             qPrintable(prefs.searchPaths().join(',')));

#else
    qDebug() << "ability to redirect the system settings dir not compiled in, skipping"
                "most qbs-config tests";
#endif // QBS_ENABLE_UNIT_TESTS

    bool canWriteToSystemSettings;
    QString testSettingsFilePath;
    {
        QSettings testSettings(
                qbs::Settings::defaultSystemSettingsBaseDir() + "/dummyOrg" + "/dummyApp.conf",
                QSettings::IniFormat);
        testSettings.setValue("dummyKey", "dummyValue");
        testSettings.sync();
        canWriteToSystemSettings = testSettings.status() == QSettings::NoError;
        testSettingsFilePath = testSettings.fileName();
    }
    if (canWriteToSystemSettings)
        QVERIFY(QFile::remove(testSettingsFilePath));

    // Check that trying to write to actual system settings causes access failure.
    params.expectFailure = !canWriteToSystemSettings;
    params.environment.clear();
    params.arguments = QStringList{"--system", "key.subkey.scalar", "s"};
    QCOMPARE(runQbs(params) == 0, canWriteToSystemSettings);
    if (!canWriteToSystemSettings) {
        QVERIFY2(m_qbsStderr.contains("You do not have permission to write to that location."),
                 m_qbsStderr.constData());
    } else {
        // cleanup system settings
        params.arguments = QStringList{"--system", "--unset", "key.subkey.scalar"};
        QCOMPARE(runQbs(params) == 0, canWriteToSystemSettings);
    }
}

void TestBlackbox::qbsConfigAddProfile()
{
    QbsRunParameters params("config");
    QTemporaryDir settingsDir1;
    QTemporaryDir settingsDir2;
    QVERIFY(settingsDir1.isValid());
    QVERIFY(settingsDir2.isValid());
    const QStringList settingsDir1Args = QStringList{"--settings-dir", settingsDir1.path()};
    const QStringList settingsDir2Args = QStringList{"--settings-dir", settingsDir2.path()};

    QFETCH(QStringList, args);
    QFETCH(QString, errorMsg);

    // Step 1: Run --add-profile.
    params.arguments = settingsDir1Args;
    params.arguments << "--add-profile";
    params.arguments << args;
    params.expectFailure = !errorMsg.isEmpty();
    QCOMPARE(runQbs(params) == 0, !params.expectFailure);
    if (params.expectFailure) {
        QVERIFY(QString::fromLocal8Bit(m_qbsStderr).contains(errorMsg));
        return;
    }
    params.expectFailure = false;
    params.arguments = settingsDir1Args;
    params.arguments << "--list";
    QCOMPARE(runQbs(params), 0);
    const QByteArray output1 = m_qbsStdout;

    // Step 2: Set properties manually.
    for (int i = 1; i < args.size(); i += 2) {
        params.arguments = settingsDir2Args;
        params.arguments << ("profiles." + args.first() + '.' + args.at(i)) << args.at(i + 1);
        QCOMPARE(runQbs(params), 0);
    }
    params.arguments = settingsDir2Args;
    params.arguments << "--list";
    QCOMPARE(runQbs(params), 0);
    const QByteArray output2 = m_qbsStdout;

    // Step3: Compare results.
    QCOMPARE(output1, output2);
}

void TestBlackbox::qbsConfigAddProfile_data()
{
    QTest::addColumn<QStringList>("args");
    QTest::addColumn<QString>("errorMsg");
    QTest::newRow("no arguments") << QStringList() << QString("Profile name missing");
    QTest::newRow("empty name") << QStringList{"", "p", "v"}
                                << QString("Profile name must not be empty");
    QTest::newRow("no properties") << QStringList("p")
                                   << QString("Profile properties must be provided");
    QTest::newRow("one property") << QStringList{"p", "p", "v"} << QString();
    QTest::newRow("two properties") << QStringList{"p", "p1", "v1", "p2", "v2"} << QString();
    QTest::newRow("missing value") << QStringList{"p", "p"}
                                   << QString("Profile properties must be key/value pairs");
    QTest::newRow("missing values") << QStringList{"p", "p1", "v1", "p2"}
                                    << QString("Profile properties must be key/value pairs");
}

void TestBlackbox::qbsConfigImport()
{
    QFETCH(QString, format);

    QDir::setCurrent(testDataDir + "/qbs-config-import-export");

    QbsRunParameters params("config");
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    const QStringList settingsDirArgs = QStringList{"--settings-dir", settingsDir.path()};
    params.arguments = settingsDirArgs;
    params.arguments << "--import" << "config." + format;

    QCOMPARE(runQbs(params), 0);

    params.arguments = settingsDirArgs;
    params.arguments << "--list";
    QCOMPARE(runQbs(params), 0);
    const QByteArray output = m_qbsStdout;
    const auto lines = output.split('\n');
    QCOMPARE(lines.count(), 5);
    QCOMPARE(lines[0], "group.key1: \"value1\"");
    QCOMPARE(lines[1], "group.key2: \"value2\"");
    QCOMPARE(lines[2], "key: \"value\"");
    QCOMPARE(lines[3], "listKey: [\"valueOne\", \"valueTwo\"]");
    QCOMPARE(lines[4], "");
}

void TestBlackbox::qbsConfigImport_data()
{
    QTest::addColumn<QString>("format");

    QTest::newRow("text") << QStringLiteral("txt");
    QTest::newRow("json") << QStringLiteral("json");
}

void TestBlackbox::qbsConfigExport()
{
    QFETCH(QString, format);

    QDir::setCurrent(testDataDir + "/qbs-config-import-export");

    QbsRunParameters params("config");
    QTemporaryDir settingsDir;
    const QString fileName = "config." + format;
    const QString filePath = settingsDir.path() + "/" + fileName;
    QVERIFY(settingsDir.isValid());
    const QStringList commonArgs = QStringList{"--settings-dir", settingsDir.path(), "--user"};

    std::pair<QString, QString> values[] = {
        {"key", "value"},
        {"listKey", "[\"valueOne\",\"valueTwo\"]"},
        {"group.key1", "value1"},
        {"group.key2", "value2"}
    };

    for (const auto &value: values) {
        params.arguments = commonArgs;
        params.arguments << value.first << value.second;
        QCOMPARE(runQbs(params), 0);
    }

    params.arguments = commonArgs;
    params.arguments << "--export" << filePath;

    QCOMPARE(runQbs(params), 0);

    QVERIFY(QFileInfo(filePath).canonicalPath() != QFileInfo(fileName).canonicalPath());

    QFile exported(filePath);
    QFile expected(fileName);

    QVERIFY(exported.open(QIODevice::ReadOnly | QIODevice::Text));
    QVERIFY(expected.open(QIODevice::ReadOnly | QIODevice::Text));

    QCOMPARE(exported.readAll(), expected.readAll());
}

void TestBlackbox::qbsConfigExport_data()
{
    QTest::addColumn<QString>("format");

    QTest::newRow("text") << QStringLiteral("txt");
    QTest::newRow("json") << QStringLiteral("json");
}

static QJsonObject getNextSessionPacket(QProcess &session, QByteArray &data)
{
    int totalSize = -1;
    QElapsedTimer timer;
    timer.start();
    QByteArray msg;
    while (totalSize == -1 || msg.size() < totalSize) {
        if (data.isEmpty())
            session.waitForReadyRead(1000);
        if (timer.elapsed() >= testTimeoutInMsecs())
            return QJsonObject();
        data += session.readAllStandardOutput();
        if (totalSize == -1) {
            static const QByteArray magicString = "qbsmsg:";
            const int magicStringOffset = data.indexOf(magicString);
            if (magicStringOffset == -1)
                continue;
            const int sizeOffset = magicStringOffset + magicString.length();
            const int newlineOffset = data.indexOf('\n', sizeOffset);
            if (newlineOffset == -1)
                continue;
            const QByteArray sizeString = data.mid(sizeOffset, newlineOffset - sizeOffset);
            bool isNumber;
            const int size = sizeString.toInt(&isNumber);
            if (!isNumber || size <= 0)
                return QJsonObject();
            data = data.mid(newlineOffset + 1);
            totalSize = size;
        }
        const int bytesToTake = std::min(totalSize - msg.size(), data.size());
        msg += data.left(bytesToTake);
        data = data.mid(bytesToTake);
    }
    return QJsonDocument::fromJson(QByteArray::fromBase64(msg)).object();
}

static void sendSessionPacket(QProcess &sessionProc, const QJsonObject &message)
{
    const QByteArray data = QJsonDocument(message).toJson().toBase64();
    sessionProc.write("qbsmsg:");
    sessionProc.write(QByteArray::number(data.length()));
    sessionProc.write("\n");
    sessionProc.write(data);
}

void TestBlackbox::qbsLanguageServer_data()
{
    QTest::addColumn<QString>("request");
    QTest::addColumn<QString>("location");
    QTest::addColumn<QString>("insertLocation");
    QTest::addColumn<QString>("insertString");
    QTest::addColumn<QString>("expectedReply");

    QTest::addRow("follow to module") << "--goto-def"
                                      << (testDataDir + "/lsp/lsp.qbs:4:9")
                                      << QString() << QString()
                                      << (testDataDir + "/lsp/modules/m/m.qbs:1:1");
    QTest::addRow("follow to submodules")
            << "--goto-def"
            << (testDataDir + "/lsp/lsp.qbs:5:35")
            << QString() << QString()
            << ((testDataDir + "/lsp/modules/Prefix/m1/m1.qbs:1:1\n")
                + (testDataDir + "/lsp/modules/Prefix/m2/m2.qbs:1:1\n")
                + (testDataDir + "/lsp/modules/Prefix/m3/m3.qbs:1:1"));
    QTest::addRow("follow to product")
        << "--goto-def" << (testDataDir + "/lsp/lsp.qbs:9:19") << QString() << QString()
        << (testDataDir + "/lsp/lsp.qbs:2:5");
    QTest::addRow("follow to module, non-invalidating insert")
        << "--goto-def" << (testDataDir + "/lsp/lsp.qbs:4:9") << "5:9"
        << QString("property bool dummy\n") << (testDataDir + "/lsp/modules/m/m.qbs:1:1");
    QTest::addRow("follow to module, invalidating insert")
        << "--goto-def" << (testDataDir + "/lsp/lsp.qbs:4:9") << QString()
        << QString("property bool dummy\n") << QString();
    QTest::addRow("follow to file in product")
        << "--goto-def" << (testDataDir + "/lsp/lsp.qbs:10:25") << QString() << QString()
        << (testDataDir + "/lsp/toplevel.txt:1:1");
    QTest::addRow("follow to file in group (first element)")
        << "--goto-def" << (testDataDir + "/lsp/lsp.qbs:13:34") << QString() << QString()
        << (testDataDir + "/lsp/subdir/file1.txt:1:1");
    QTest::addRow("follow to file in group (last element)")
        << "--goto-def" << (testDataDir + "/lsp/lsp.qbs:13:48") << QString() << QString()
        << (testDataDir + "/lsp/subdir/file2.txt:1:1");
    QTest::addRow("follow \"references\"")
        << "--goto-def" << (testDataDir + "/lsp/lsp.qbs:16:24") << QString() << QString()
        << (testDataDir + "/lsp/other.qbs:1:1");
    QTest::addRow("follow item from import path")
        << "--goto-def" << (testDataDir + "/lsp/lsp.qbs:17:7") << QString() << QString()
        << (testDataDir + "/lsp/MyProduct.qbs:2:74");
    QTest::addRow("follow built-in item")
        << "--goto-def" << (testDataDir + "/lsp/lsp.qbs:18:7") << QString() << QString()
        << QString();
    QTest::addRow("follow to module in binding (first segment)")
        << "--goto-def" << (testDataDir + "/lsp/lsp.qbs:21:12") << QString() << QString()
        << (testDataDir + "/lsp/modules/Prefix/m1/m1.qbs:1:1");
    QTest::addRow("follow to module in binding (last segment)")
        << "--goto-def" << (testDataDir + "/lsp/lsp.qbs:21:17") << QString() << QString()
        << (testDataDir + "/lsp/modules/Prefix/m1/m1.qbs:1:1");
    QTest::addRow("follow to module property")
        << "--goto-def" << (testDataDir + "/lsp/lsp.qbs:21:20") << QString() << QString()
        << (testDataDir + "/lsp/modules/Prefix/m1/m1.qbs:2:19");
    QTest::addRow("follow to module property 2")
        << "--goto-def" << (testDataDir + "/lsp/lsp.qbs:6:13") << QString() << QString()
        << (testDataDir + "/lsp/modules/m/m.qbs:2:19");
    QTest::addRow("completion: LHS, module prefix")
        << "--completion" << (testDataDir + "/lsp/lsp.qbs:7:1") << QString() << QString("P")
        << QString("Prefix.m1\nPrefix.m2\nPrefix.m3");
    QTest::addRow("completion: LHS, module name")
        << "--completion" << (testDataDir + "/lsp/lsp.qbs:7:1") << QString() << QString("Prefix.m")
        << QString("m1\nm2\nm3");
    QTest::addRow("completion: LHS, module property right after dot")
        << "--completion" << (testDataDir + "/lsp/lsp.qbs:7:1") << QString()
        << QString("Prefix.m1.") << QString("p1 bool\np2 string\nx bool");
    QTest::addRow("completion: LHS, module property with identifier prefix")
        << "--completion" << (testDataDir + "/lsp/lsp.qbs:7:1") << QString()
        << QString("Prefix.m1.p") << QString("p1 bool\np2 string");
    QTest::addRow("completion: simple RHS, module property")
        << "--completion" << (testDataDir + "/lsp/lsp.qbs:7:1") << QString()
        << QString("property bool dummy: Prefix.m1.p") << QString("p1 bool\np2 string");
    QTest::addRow("completion: complex RHS, module property")
        << "--completion" << (testDataDir + "/lsp/lsp.qbs:7:1") << QString()
        << QString("property bool dummy: { return Prefix.m1.p") << QString("p1 bool\np2 string");
}

void TestBlackbox::qbsLanguageServer()
{
    QFETCH(QString, request);
    QFETCH(QString, location);
    QFETCH(QString, insertLocation);
    QFETCH(QString, insertString);
    QFETCH(QString, expectedReply);

    QDir::setCurrent(testDataDir + "/lsp");
    QProcess sessionProc;
    sessionProc.start(qbsExecutableFilePath, QStringList("session"));

    QVERIFY(sessionProc.waitForStarted());

    QByteArray incomingData;

    // Wait for and verify hello packet.
    QJsonObject receivedMessage = getNextSessionPacket(sessionProc, incomingData);
    const QString socketPath = receivedMessage.value("lsp-socket").toString();
    QVERIFY(!socketPath.isEmpty());

    // Resolve project.
    QJsonObject resolveMessage;
    resolveMessage.insert("type", "resolve-project");
    resolveMessage.insert("top-level-profile", profileName());
    resolveMessage.insert("project-file-path", QDir::currentPath() + "/lsp.qbs");
    resolveMessage.insert("build-root", QDir::currentPath());
    resolveMessage.insert("settings-directory", settings()->baseDirectory());
    sendSessionPacket(sessionProc, resolveMessage);
    bool receivedReply = false;
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        QVERIFY(!receivedMessage.isEmpty());
        const QString msgType = receivedMessage.value("type").toString();
        if (msgType == "project-resolved") {
            receivedReply = true;
            const QJsonObject error = receivedMessage.value("error").toObject();
            if (!error.isEmpty())
                qDebug() << error;
            QVERIFY(error.isEmpty());
        }
    }
    QVERIFY(receivedReply);

    // Employ client app to send request.
    QProcess lspClient;
    const QFileInfo qbsFileInfo(qbsExecutableFilePath);
    const QString clientFilePath = HostOsInfo::appendExecutableSuffix(
                qbsFileInfo.absolutePath() + "/qbs_lspclient");
    QStringList args{"--socket", socketPath, request, location};
    if (!insertString.isEmpty())
        args << "--insert-code" << insertString;
    if (!insertLocation.isEmpty())
        args << "--insert-location" << insertLocation;
    lspClient.start(clientFilePath, args);
    QVERIFY2(lspClient.waitForStarted(), qPrintable(lspClient.errorString()));
    QVERIFY2(lspClient.waitForFinished(), qPrintable(lspClient.errorString()));
    QString errMsg;
    if (lspClient.exitStatus() != QProcess::NormalExit)
        errMsg = lspClient.errorString();
    if (errMsg.isEmpty())
        errMsg = QString::fromLocal8Bit(lspClient.readAllStandardError());
    QVERIFY2(lspClient.exitCode() == 0, qPrintable(errMsg));
    QVERIFY2(errMsg.isEmpty(), qPrintable(errMsg));
    QString output = QString::fromUtf8(lspClient.readAllStandardOutput().trimmed());
    output.replace("\r\n", "\n");
    QCOMPARE(output, expectedReply);

    QJsonObject quitRequest;
    quitRequest.insert("type", "quit");
    sendSessionPacket(sessionProc, quitRequest);
    QVERIFY(sessionProc.waitForFinished(3000));
}

void TestBlackbox::qbsSession()
{
    QDir::setCurrent(testDataDir + "/qbs-session");
    QProcess sessionProc;
    sessionProc.start(qbsExecutableFilePath, QStringList("session"));

    // Uncomment for debugging.
    /*
    connect(&sessionProc, &QProcess::readyReadStandardError, [&sessionProc] {
        qDebug() << "stderr:" << sessionProc.readAllStandardError();
    });
    */

    QVERIFY(sessionProc.waitForStarted());

    const auto sendPacket = [&sessionProc](const QJsonObject &message) {
        sendSessionPacket(sessionProc, message);
    };

    static const auto envToJson = [](const QProcessEnvironment &env) {
        QJsonObject envObj;
        const QStringList keys = env.keys();
        for (const QString &key : keys)
            envObj.insert(key, env.value(key));
        return envObj;
    };

    static const auto envFromJson = [](const QJsonValue &v) {
        const QJsonObject obj = v.toObject();
        QProcessEnvironment env;
        for (auto it = obj.begin(); it != obj.end(); ++it)
            env.insert(it.key(), it.value().toString());
        return env;
    };

    QByteArray incomingData;

    // Wait for and verify hello packet.
    QJsonObject receivedMessage = getNextSessionPacket(sessionProc, incomingData);
    QCOMPARE(receivedMessage.value("type"), "hello");
    QCOMPARE(receivedMessage.value("api-level").toInt(), 9);
    QCOMPARE(receivedMessage.value("api-compat-level").toInt(), 2);

    // Resolve & verify structure
    QJsonObject resolveMessage;
    resolveMessage.insert("type", "resolve-project");
    resolveMessage.insert("top-level-profile", profileName());
    resolveMessage.insert("configuration-name", "my-config");
    resolveMessage.insert("project-file-path", QDir::currentPath() + "/qbs-session.qbs");
    resolveMessage.insert("build-root", QDir::currentPath());
    resolveMessage.insert("settings-directory", settings()->baseDirectory());
    QJsonObject overriddenValues;
    overriddenValues.insert("products.theLib.cpp.cxxLanguageVersion", "c++17");
    resolveMessage.insert("overridden-properties", overriddenValues);
    resolveMessage.insert("environment", envToJson(QbsRunParameters::defaultEnvironment()));
    resolveMessage.insert("data-mode", "only-if-changed");
    resolveMessage.insert("max-job-count", 2);
    resolveMessage.insert("log-time", true);
    resolveMessage.insert(
        "module-properties",
        QJsonArray::fromStringList(
            {"cpp.cxxLanguageVersion", "cpp.executableSuffix", "cpp.objectSuffix"}));
    sendPacket(resolveMessage);
    bool receivedLogData = false;
    bool receivedStartedSignal = false;
    bool receivedProgressData = false;
    bool receivedReply = false;
    QString objectSuffix;
    QString executableSuffix;
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        QVERIFY(!receivedMessage.isEmpty());
        const QString msgType = receivedMessage.value("type").toString();
        if (msgType == "project-resolved") {
            receivedReply = true;
            const QJsonObject error = receivedMessage.value("error").toObject();
            if (!error.isEmpty())
                qDebug() << error;
            QVERIFY(error.isEmpty());
            const QJsonObject projectData = receivedMessage.value("project-data").toObject();
            QCOMPARE(projectData.value("name").toString(), "qbs-session");
            const QJsonArray products = projectData.value("products").toArray();
            QCOMPARE(products.size(), 2);
            for (const QJsonValue &v : products) {
                const QJsonObject product = v.toObject();
                const QString productName = product.value("name").toString();
                QVERIFY(!productName.isEmpty());
                QVERIFY2(product.value("is-enabled").toBool(), qPrintable(productName));
                const QJsonObject moduleProps = product.value("module-properties").toObject();
                objectSuffix = moduleProps.value("cpp.objectSuffix").toString();
                executableSuffix = moduleProps.value("cpp.executableSuffix").toString();
                bool theLib = false;
                bool theApp = false;
                if (productName == "theLib")
                    theLib = true;
                else if (productName == "theApp")
                    theApp = true;
                QVERIFY2(theLib || theApp, qPrintable(productName));
                const QJsonArray groups = product.value("groups").toArray();
                if (theLib)
                    QVERIFY(groups.size() >= 3);
                else
                    QVERIFY(!groups.isEmpty());
                for (const QJsonValue &v : groups) {
                    const QJsonObject group = v.toObject();
                    const QJsonArray sourceArtifacts
                            = group.value("source-artifacts").toArray();
                    const auto findArtifact = [&sourceArtifacts](const QString &fileName) {
                        for (const QJsonValue &v : sourceArtifacts) {
                            const QJsonObject artifact = v.toObject();
                            if (QFileInfo(artifact.value("file-path").toString()).fileName()
                                    == fileName) {
                                return artifact;
                            }
                        }
                        return QJsonObject();
                    };
                    const QString groupName = group.value("name").toString();
                    const auto getCxxLanguageVersion = [&group, &product] {
                        QJsonObject moduleProperties = group.value("module-properties").toObject();
                        if (moduleProperties.isEmpty())
                            moduleProperties = product.value("module-properties").toObject();
                        return moduleProperties.toVariantMap().value("cpp.cxxLanguageVersion")
                                .toStringList();
                    };
                    if (groupName == "sources") {
                        const QJsonObject artifact = findArtifact("lib.cpp");
                        QVERIFY2(!artifact.isEmpty(), "lib.cpp");
                        QCOMPARE(getCxxLanguageVersion(), {"c++17"});
                    } else if (groupName == "headers") {
                        const QJsonObject artifact = findArtifact("lib.h");
                        QVERIFY2(!artifact.isEmpty(), "lib.h");
                    } else if (groupName == "theApp") {
                        const QJsonObject artifact = findArtifact("main.cpp");
                        QVERIFY2(!artifact.isEmpty(), "main.cpp");
                        QCOMPARE(getCxxLanguageVersion(), {"c++14"});
                    }
                }
            }
            break;
        } else if (msgType == "log-data") {
            if (receivedMessage.value("message").toString().contains("activity"))
                receivedLogData = true;
        } else if (msgType == "task-started") {
            receivedStartedSignal = true;
        } else if (msgType == "task-progress") {
            receivedProgressData = true;
        } else if (msgType != "new-max-progress") {
            QVERIFY2(false, qPrintable(QString("Unexpected message type '%1'").arg(msgType)));
        }
    }
    QVERIFY(receivedReply);
    QVERIFY(receivedLogData);
    QVERIFY(receivedStartedSignal);
    QVERIFY(receivedProgressData);

    // Add a dependency and re-resolve.
    QJsonObject addDepsRequest;
    addDepsRequest.insert("type", "add-dependencies");
    addDepsRequest.insert("product", "theApp");
    addDepsRequest.insert("group", "theApp");
    addDepsRequest.insert("dependencies", QJsonArray::fromStringList({"mymodule"}));
    sendPacket(addDepsRequest);
    receivedMessage = getNextSessionPacket(sessionProc, incomingData);
    QCOMPARE(receivedMessage.value("type").toString(), QString("dependencies-added"));
    QJsonObject error = receivedMessage.value("error").toObject();
    if (!error.isEmpty()) {
        for (const auto item : error[QStringLiteral("items")].toArray()) {
            const auto description = QStringLiteral("Project file updates are not enabled");
            if (item.toObject()[QStringLiteral("description")].toString().contains(description))
                QSKIP("File updates are disabled");
        }
        qDebug() << error;
    }
    QVERIFY(error.isEmpty());
    receivedReply = false;
    sendPacket(resolveMessage);
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        QVERIFY(!receivedMessage.isEmpty());
        const QString msgType = receivedMessage.value("type").toString();
        if (msgType == "project-resolved") {
            receivedReply = true;
            const QJsonObject error = receivedMessage.value("error").toObject();
            if (!error.isEmpty())
                qDebug() << error;
            QVERIFY(error.isEmpty());
        }
    }
    QVERIFY(receivedReply);

    // First build: No install, log time, default command description.
    QJsonObject buildRequest;
    buildRequest.insert("type", "build-project");
    buildRequest.insert("log-time", true);
    buildRequest.insert("install", false);
    buildRequest.insert("data-mode", "only-if-changed");
    sendPacket(buildRequest);
    receivedReply = false;
    receivedLogData = false;
    receivedStartedSignal = false;
    receivedProgressData = false;
    bool receivedCommandDescription = false;
    bool receivedProcessResult = false;
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        QVERIFY(!receivedMessage.isEmpty());
        const QString msgType = receivedMessage.value("type").toString();
        if (msgType == "project-built") {
            receivedReply = true;
            const QJsonObject error = receivedMessage.value("error").toObject();
            if (!error.isEmpty())
                qDebug() << error;
            QVERIFY(error.isEmpty());
            const QJsonObject projectData = receivedMessage.value("project-data").toObject();
            QCOMPARE(projectData.value("name").toString(), "qbs-session");
        } else if (msgType == "log-data") {
            if (receivedMessage.value("message").toString().contains("activity"))
                receivedLogData = true;
        } else if (msgType == "task-started") {
            receivedStartedSignal = true;
        } else if (msgType == "task-progress") {
            receivedProgressData = true;
        } else if (msgType == "command-description") {
            if (receivedMessage.value("message").toString().contains("compiling main.cpp"))
                receivedCommandDescription = true;
        } else if (msgType == "process-result") {
            QCOMPARE(receivedMessage.value("exit-code").toInt(), 0);
            receivedProcessResult = true;
        } else if (msgType != "new-max-progress") {
            QVERIFY2(false, qPrintable(QString("Unexpected message type '%1'").arg(msgType)));
        }
    }
    QVERIFY(receivedReply);
    QVERIFY(receivedLogData);
    QVERIFY(receivedStartedSignal);
    QVERIFY(receivedProgressData);
    QVERIFY(receivedCommandDescription);
    QVERIFY(receivedProcessResult);
    const QString &exeFilePath = QDir::currentPath() + '/'
                                 + relativeProductBuildDir("theApp", "my-config") + '/' + "theApp"
                                 + executableSuffix;
    QVERIFY2(regularFileExists(exeFilePath), qPrintable(exeFilePath));
    const QString defaultInstallRoot = QDir::currentPath() + '/'
            + relativeBuildDir("my-config") + "/install-root";
    QVERIFY2(!directoryExists(defaultInstallRoot), qPrintable(defaultInstallRoot));

    // Clean.
    QJsonObject cleanRequest;
    cleanRequest.insert("type", "clean-project");
    cleanRequest.insert("settings-dir", settings()->baseDirectory());
    cleanRequest.insert("log-time", true);
    sendPacket(cleanRequest);
    receivedReply = false;
    receivedLogData = false;
    receivedStartedSignal = false;
    receivedProgressData = false;
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        QVERIFY(!receivedMessage.isEmpty());
        const QString msgType = receivedMessage.value("type").toString();
        if (msgType == "project-cleaned") {
            receivedReply = true;
            const QJsonObject error = receivedMessage.value("error").toObject();
            if (!error.isEmpty())
                qDebug() << error;
            QVERIFY(error.isEmpty());
        } else if (msgType == "log-data") {
            if (receivedMessage.value("message").toString().contains("activity"))
                receivedLogData = true;
        } else if (msgType == "task-started") {
            receivedStartedSignal = true;
        } else if (msgType == "task-progress") {
            receivedProgressData = true;
        } else if (msgType != "new-max-progress") {
            QVERIFY2(false, qPrintable(QString("Unexpected message type '%1'").arg(msgType)));
        }
    }
    QVERIFY(receivedReply);
    QVERIFY(receivedLogData);
    QVERIFY(receivedStartedSignal);
    QVERIFY(receivedProgressData);
    QVERIFY2(!regularFileExists(exeFilePath), qPrintable(exeFilePath));

    // Second build: Do not log the time, show command lines.
    buildRequest.insert("log-time", false);
    buildRequest.insert("command-echo-mode", "command-line");
    sendPacket(buildRequest);
    receivedReply = false;
    receivedLogData = false;
    receivedStartedSignal = false;
    receivedProgressData = false;
    receivedCommandDescription = false;
    receivedProcessResult = false;
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        QVERIFY(!receivedMessage.isEmpty());
        const QString msgType = receivedMessage.value("type").toString();
        if (msgType == "project-built") {
            receivedReply = true;
            const QJsonObject error = receivedMessage.value("error").toObject();
            if (!error.isEmpty())
                qDebug() << error;
            QVERIFY(error.isEmpty());
            const QJsonObject projectData = receivedMessage.value("project-data").toObject();
            QVERIFY(projectData.isEmpty());
        } else if (msgType == "log-data") {
            if (receivedMessage.value("message").toString().contains("activity"))
                receivedLogData = true;
        } else if (msgType == "task-started") {
            receivedStartedSignal = true;
        } else if (msgType == "task-progress") {
            receivedProgressData = true;
        } else if (msgType == "command-description") {
            if (QDir::fromNativeSeparators(receivedMessage.value("message").toString())
                    .contains("/main.cpp")) {
                receivedCommandDescription = true;
            }
        } else if (msgType == "process-result") {
            QCOMPARE(receivedMessage.value("exit-code").toInt(), 0);
            receivedProcessResult = true;
        } else if (msgType != "new-max-progress") {
            QVERIFY2(false, qPrintable(QString("Unexpected message type '%1'").arg(msgType)));
        }
    }
    QVERIFY(receivedReply);
    QVERIFY(!receivedLogData);
    QVERIFY(receivedStartedSignal);
    QVERIFY(receivedProgressData);
    QVERIFY(receivedCommandDescription);
    QVERIFY(receivedProcessResult);
    QVERIFY2(regularFileExists(exeFilePath), qPrintable(exeFilePath));
    QVERIFY2(!directoryExists(defaultInstallRoot), qPrintable(defaultInstallRoot));

    // Install.
    QJsonObject installRequest;
    installRequest.insert("type", "install-project");
    installRequest.insert("log-time", true);
    const QString customInstallRoot = QDir::currentPath() + "/my-install-root";
    QVERIFY2(!QFile::exists(customInstallRoot), qPrintable(customInstallRoot));
    installRequest.insert("install-root", customInstallRoot);
    sendPacket(installRequest);
    receivedReply = false;
    receivedLogData = false;
    receivedStartedSignal = false;
    receivedProgressData = false;
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        QVERIFY(!receivedMessage.isEmpty());
        const QString msgType = receivedMessage.value("type").toString();
        if (msgType == "install-done") {
            receivedReply = true;
            const QJsonObject error = receivedMessage.value("error").toObject();
            if (!error.isEmpty())
                qDebug() << error;
            QVERIFY(error.isEmpty());
        } else if (msgType == "log-data") {
            if (receivedMessage.value("message").toString().contains("activity"))
                receivedLogData = true;
        } else if (msgType == "task-started") {
            receivedStartedSignal = true;
        } else if (msgType == "task-progress") {
            receivedProgressData = true;
        } else if (msgType != "new-max-progress") {
            QVERIFY2(false, qPrintable(QString("Unexpected message type '%1'").arg(msgType)));
        }
    }
    QVERIFY(receivedReply);
    QVERIFY(receivedLogData);
    QVERIFY(receivedStartedSignal);
    QVERIFY(receivedProgressData);
    QVERIFY2(!directoryExists(defaultInstallRoot), qPrintable(defaultInstallRoot));
    QVERIFY2(directoryExists(customInstallRoot), qPrintable(customInstallRoot));

    // Retrieve modified environment.
    QJsonObject getRunEnvRequest;
    getRunEnvRequest.insert("type", "get-run-environment");
    getRunEnvRequest.insert("product", "theApp");
    const QProcessEnvironment inEnv = QProcessEnvironment::systemEnvironment();
    QVERIFY(!inEnv.contains("MY_MODULE"));
    getRunEnvRequest.insert("base-environment", envToJson(inEnv));
    sendPacket(getRunEnvRequest);
    receivedMessage = getNextSessionPacket(sessionProc, incomingData);
    QCOMPARE(receivedMessage.value("type").toString(), QString("run-environment"));
    error = receivedMessage.value("error").toObject();
    if (!error.isEmpty())
        qDebug() << error;
    QVERIFY(error.isEmpty());
    const QProcessEnvironment outEnv = envFromJson(receivedMessage.value("full-environment"));
    QVERIFY(outEnv.keys().size() > inEnv.keys().size());
    QCOMPARE(outEnv.value("MY_MODULE"), QString("1"));

    // Add two files to library and re-build.
    QJsonObject addFilesRequest;
    addFilesRequest.insert("type", "add-files");
    addFilesRequest.insert("product", "theLib");
    addFilesRequest.insert("group", "sources");
    addFilesRequest.insert("files",
                           QJsonArray::fromStringList({QDir::currentPath() + "/file1.cpp",
                                                       QDir::currentPath() + "/file2.cpp"}));
    sendPacket(addFilesRequest);
    receivedMessage = getNextSessionPacket(sessionProc, incomingData);
    QCOMPARE(receivedMessage.value("type").toString(), QString("files-added"));
    error = receivedMessage.value("error").toObject();
    if (!error.isEmpty()) {
        for (const auto item: error[QStringLiteral("items")].toArray()) {
            const auto description = QStringLiteral("Project file updates are not enabled");
            if (item.toObject()[QStringLiteral("description")].toString().contains(description))
                QSKIP("File updates are disabled");
        }
        qDebug() << error;
    }
    QVERIFY(error.isEmpty());

    receivedReply = false;
    sendPacket(resolveMessage);
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        QVERIFY(!receivedMessage.isEmpty());
        const QString msgType = receivedMessage.value("type").toString();
        if (msgType == "project-resolved") {
            receivedReply = true;
            const QJsonObject error = receivedMessage.value("error").toObject();
            if (!error.isEmpty())
                qDebug() << error;
            QVERIFY(error.isEmpty());
            const QJsonObject projectData = receivedMessage.value("project-data").toObject();
            QJsonArray products = projectData.value("products").toArray();
            bool file1 = false;
            bool file2 = false;
            for (const QJsonValue v : products) {
                const QJsonObject product = v.toObject();
                const QString productName = product.value("full-display-name").toString();
                const QJsonArray groups = product.value("groups").toArray();
                for (const QJsonValue &v : groups) {
                    const QJsonObject group = v.toObject();
                    const QString groupName = group.value("name").toString();
                    const QJsonArray sourceArtifacts = group.value("source-artifacts").toArray();
                    for (const QJsonValue &v : sourceArtifacts) {
                        const QString filePath = v.toObject().value("file-path").toString();
                        if (filePath.endsWith("file1.cpp")) {
                            QCOMPARE(productName, QString("theLib"));
                            QCOMPARE(groupName, QString("sources"));
                            file1 = true;
                        } else if (filePath.endsWith("file2.cpp")) {
                            QCOMPARE(productName, QString("theLib"));
                            QCOMPARE(groupName, QString("sources"));
                            file2 = true;
                        }
                    }
                }
            }
            QVERIFY(file1);
            QVERIFY(file2);
        }
    }
    QVERIFY(receivedReply);

    receivedReply = false;
    receivedProcessResult = false;
    bool compiledFile1 = false;
    bool compiledFile2 = false;
    bool compiledMain = false;
    bool compiledLib = false;
    buildRequest.remove("command-echo-mode");
    sendPacket(buildRequest);
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        QVERIFY(!receivedMessage.isEmpty());
        const QString msgType = receivedMessage.value("type").toString();
        if (msgType == "project-built") {
            receivedReply = true;
            const QJsonObject error = receivedMessage.value("error").toObject();
            if (!error.isEmpty())
                qDebug() << error;
            QVERIFY(error.isEmpty());
        } else if (msgType == "command-description") {
            const QString msg = receivedMessage.value("message").toString();
            if (msg.contains("compiling file1.cpp"))
                compiledFile1 = true;
            else if (msg.contains("compiling file2.cpp"))
                compiledFile2 = true;
            else if (msg.contains("compiling main.cpp"))
                compiledMain = true;
            else if (msg.contains("compiling lib.cpp"))
                compiledLib = true;
        } else if (msgType == "process-result") {
            QCOMPARE(receivedMessage.value("exit-code").toInt(), 0);
            receivedProcessResult = true;
        }
    }
    QVERIFY(receivedReply);
    QVERIFY(!receivedProcessResult);
    QVERIFY(compiledFile1);
    QVERIFY(compiledFile2);
    QVERIFY(!compiledLib);
    QVERIFY(!compiledMain);

    // Remove one of the newly added files again and re-build.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("file1.cpp");
    touch("file2.cpp");
    touch("main.cpp");
    QJsonObject removeFilesRequest;
    removeFilesRequest.insert("type", "remove-files");
    removeFilesRequest.insert("product", "theLib");
    removeFilesRequest.insert("group", "sources");
    removeFilesRequest.insert("files",
                              QJsonArray::fromStringList({QDir::currentPath() + "/file1.cpp"}));
    sendPacket(removeFilesRequest);
    receivedMessage = getNextSessionPacket(sessionProc, incomingData);
    QCOMPARE(receivedMessage.value("type").toString(), QString("files-removed"));
    error = receivedMessage.value("error").toObject();
    if (!error.isEmpty())
        qDebug() << error;
    QVERIFY(error.isEmpty());
    receivedReply = false;
    sendPacket(resolveMessage);
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        QVERIFY(!receivedMessage.isEmpty());
        const QString msgType = receivedMessage.value("type").toString();
        if (msgType == "project-resolved") {
            receivedReply = true;
            const QJsonObject error = receivedMessage.value("error").toObject();
            if (!error.isEmpty())
                qDebug() << error;
            QVERIFY(error.isEmpty());
            const QJsonObject projectData = receivedMessage.value("project-data").toObject();
            QJsonArray products = projectData.value("products").toArray();
            bool file1 = false;
            bool file2 = false;
            for (const QJsonValue v : products) {
                const QJsonObject product = v.toObject();
                const QString productName = product.value("full-display-name").toString();
                const QJsonArray groups = product.value("groups").toArray();
                for (const QJsonValue &v : groups) {
                    const QJsonObject group = v.toObject();
                    const QString groupName = group.value("name").toString();
                    const QJsonArray sourceArtifacts = group.value("source-artifacts").toArray();
                    for (const QJsonValue &v : sourceArtifacts) {
                        const QString filePath = v.toObject().value("file-path").toString();
                        if (filePath.endsWith("file1.cpp")) {
                            file1 = true;
                        } else if (filePath.endsWith("file2.cpp")) {
                            QCOMPARE(productName, QString("theLib"));
                            QCOMPARE(groupName, QString("sources"));
                            file2 = true;
                        }
                    }
                }
            }
            QVERIFY(!file1);
            QVERIFY(file2);
        }
    }
    QVERIFY(receivedReply);

    receivedReply = false;
    receivedProcessResult = false;
    compiledFile1 = false;
    compiledFile2 = false;
    compiledMain = false;
    compiledLib = false;
    sendPacket(buildRequest);
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        QVERIFY(!receivedMessage.isEmpty());
        const QString msgType = receivedMessage.value("type").toString();
        if (msgType == "project-built") {
            receivedReply = true;
            const QJsonObject error = receivedMessage.value("error").toObject();
            if (!error.isEmpty())
                qDebug() << error;
            QVERIFY(error.isEmpty());
        } else if (msgType == "command-description") {
            const QString msg = receivedMessage.value("message").toString();
            if (msg.contains("compiling file1.cpp"))
                compiledFile1 = true;
            else if (msg.contains("compiling file2.cpp"))
                compiledFile2 = true;
            else if (msg.contains("compiling main.cpp"))
                compiledMain = true;
            else if (msg.contains("compiling lib.cpp"))
                compiledLib = true;
        } else if (msgType == "process-result") {
            QCOMPARE(receivedMessage.value("exit-code").toInt(), 0);
            receivedProcessResult = true;
        }
    }
    QVERIFY(receivedReply);
    QVERIFY(receivedProcessResult);
    QVERIFY(!compiledFile1);
    QVERIFY(compiledFile2);
    QVERIFY(!compiledLib);
    QVERIFY(compiledMain);

    // Rename a file and re-build.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(QFile::rename("lib.cpp", "theLib.cpp"));
    QJsonObject renameFilesRequest;
    renameFilesRequest.insert("type", "rename-files");
    renameFilesRequest.insert("product", "theLib");
    renameFilesRequest.insert("group", "sources");
    QJsonObject sourceAndTarget(
        {qMakePair(QString("source-path"), QJsonValue(QDir::currentPath() + "/lib.cpp")),
         qMakePair(QString("target-path"), QJsonValue(QDir::currentPath() + "/theLib.cpp"))});
    renameFilesRequest.insert("files", QJsonArray{sourceAndTarget});
    sendPacket(renameFilesRequest);
    receivedMessage = getNextSessionPacket(sessionProc, incomingData);
    QCOMPARE(receivedMessage.value("type").toString(), QString("files-renamed"));
    error = receivedMessage.value("error").toObject();
    if (!error.isEmpty())
        qDebug() << error;
    QVERIFY(error.isEmpty());
    receivedReply = false;
    sendPacket(resolveMessage);
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        QVERIFY(!receivedMessage.isEmpty());
        const QString msgType = receivedMessage.value("type").toString();
        if (msgType == "project-resolved") {
            receivedReply = true;
            const QJsonObject error = receivedMessage.value("error").toObject();
            if (!error.isEmpty())
                qDebug() << error;
            QVERIFY(error.isEmpty());
            const QJsonObject projectData = receivedMessage.value("project-data").toObject();
            QJsonArray products = projectData.value("products").toArray();
            bool hasOldFile = false;
            bool hasNewFile = false;
            for (const QJsonValue v : products) {
                const QJsonObject product = v.toObject();
                const QString productName = product.value("full-display-name").toString();
                const QJsonArray groups = product.value("groups").toArray();
                for (const QJsonValue &v : groups) {
                    const QJsonObject group = v.toObject();
                    const QString groupName = group.value("name").toString();
                    const QJsonArray sourceArtifacts = group.value("source-artifacts").toArray();
                    for (const QJsonValue &v : sourceArtifacts) {
                        const QString filePath = v.toObject().value("file-path").toString();
                        if (filePath.endsWith("lib.cpp")) {
                            hasOldFile = true;
                        } else if (filePath.endsWith("theLib.cpp")) {
                            QCOMPARE(productName, QString("theLib"));
                            QCOMPARE(groupName, QString("sources"));
                            hasNewFile = true;
                        }
                    }
                }
            }
            QVERIFY(!hasOldFile);
            QVERIFY(hasNewFile);
        }
    }
    QVERIFY(receivedReply);

    receivedReply = false;
    compiledLib = false;
    sendPacket(buildRequest);
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        QVERIFY(!receivedMessage.isEmpty());
        const QString msgType = receivedMessage.value("type").toString();
        if (msgType == "project-built") {
            receivedReply = true;
            const QJsonObject error = receivedMessage.value("error").toObject();
            if (!error.isEmpty())
                qDebug() << error;
            QVERIFY(error.isEmpty());
        } else if (
            msgType == "command-description"
            && receivedMessage.value("message").toString().contains("compiling theLib.cpp")) {
            compiledLib = true;
        }
    }
    QVERIFY(receivedReply);
    QVERIFY(compiledLib);

    // Get generated files.
    QJsonObject genFilesRequestPerFile;
    genFilesRequestPerFile.insert("source-file", QDir::currentPath() + "/main.cpp");
    genFilesRequestPerFile.insert("tags", QJsonArray{QJsonValue("obj")});
    QJsonObject genFilesRequestPerProduct;
    genFilesRequestPerProduct.insert("full-display-name", "theApp");
    genFilesRequestPerProduct.insert("requests", QJsonArray({genFilesRequestPerFile}));
    QJsonObject genFilesRequest;
    genFilesRequest.insert("type", "get-generated-files-for-sources");
    genFilesRequest.insert("products", QJsonArray({genFilesRequestPerProduct}));
    sendPacket(genFilesRequest);
    receivedReply = false;
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        QCOMPARE(receivedMessage.value("type").toString(), QString("generated-files-for-sources"));
        const QJsonArray products = receivedMessage.value("products").toArray();
        QCOMPARE(products.size(), 1);
        const QJsonArray results = products.first().toObject().value("results").toArray();
        QCOMPARE(results.size(), 1);
        const QJsonObject result = results.first().toObject();
        QCOMPARE(result.value("source-file"), QDir::currentPath() + "/main.cpp");
        const QJsonArray generatedFiles = result.value("generated-files").toArray();
        QCOMPARE(generatedFiles.count(), 1);
        QCOMPARE(
            QFileInfo(generatedFiles.first().toString()).fileName(), "main.cpp" + objectSuffix);
        receivedReply = true;
    }
    QVERIFY(receivedReply);

    // Release project.
    const QJsonObject releaseRequest{qMakePair(QString("type"), QJsonValue("release-project"))};
    sendPacket(releaseRequest);
    receivedReply = false;
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        QCOMPARE(receivedMessage.value("type").toString(), QString("project-released"));
        const QJsonObject error = receivedMessage.value("error").toObject();
        if (!error.isEmpty())
            qDebug() << error;
        QVERIFY(error.isEmpty());
        receivedReply = true;
    }
    QVERIFY(receivedReply);

    // Get build graph info.
    QJsonObject loadProjectMessage;
    loadProjectMessage.insert("type", "resolve-project");
    loadProjectMessage.insert("configuration-name", "my-config");
    loadProjectMessage.insert("build-root", QDir::currentPath());
    loadProjectMessage.insert("settings-dir", settings()->baseDirectory());
    loadProjectMessage.insert("restore-behavior", "restore-only");
    loadProjectMessage.insert("data-mode", "only-if-changed");
    sendPacket(loadProjectMessage);
    receivedReply = false;
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        if (receivedMessage.value("type") != "project-resolved")
            continue;
        receivedReply = true;
        const QJsonObject error = receivedMessage.value("error").toObject();
        if (!error.isEmpty())
            qDebug() << error;
        QVERIFY(error.isEmpty());
        const QString bgFilePath = QDir::currentPath() + '/'
                + relativeBuildGraphFilePath("my-config");
        const QJsonObject projectData = receivedMessage.value("project-data").toObject();
        QCOMPARE(projectData.value("build-graph-file-path").toString(), bgFilePath);
        QCOMPARE(projectData.value("overridden-properties"), overriddenValues);
    }
    QVERIFY(receivedReply);

    // Send unknown request.
    const QJsonObject unknownRequest({qMakePair(QString("type"), QJsonValue("blubb"))});
    sendPacket(unknownRequest);
    receivedReply = false;
    while (!receivedReply) {
        receivedMessage = getNextSessionPacket(sessionProc, incomingData);
        QCOMPARE(receivedMessage.value("type").toString(), QString("protocol-error"));
        receivedReply = true;
    }
    QVERIFY(receivedReply);

    QJsonObject quitRequest;
    quitRequest.insert("type", "quit");
    sendPacket(quitRequest);
    QVERIFY(sessionProc.waitForFinished(3000));
}

void TestBlackbox::radAfterIncompleteBuild_data()
{
    QTest::addColumn<QString>("projectFileName");
    QTest::newRow("Project with Rule") << "project_with_rule.qbs";
    QTest::newRow("Project with Transformer") << "project_with_transformer.qbs";
}

void TestBlackbox::radAfterIncompleteBuild()
{
    QDir::setCurrent(testDataDir + "/rad-after-incomplete-build");
    rmDirR(relativeBuildDir());
    const QString projectFileName = "project_with_rule.qbs";

    // Step 1: Have a directory where a file used to be.
    QbsRunParameters params(QStringList() << "-f" << projectFileName);
    QCOMPARE(runQbs(params), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFileName, "oldfile", "oldfile/newfile");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFileName, "oldfile/newfile", "newfile");
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFileName, "newfile", "oldfile/newfile");
    QCOMPARE(runQbs(params), 0);

    // Step 2: Have a file where a directory used to be.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFileName, "oldfile/newfile", "oldfile");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFileName, "oldfile", "newfile");
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(projectFileName, "newfile", "oldfile");
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::subProfileChangeTracking()
{
    QDir::setCurrent(testDataDir + "/subprofile-change-tracking");
    const SettingsPtr s = settings();
    qbs::Internal::TemporaryProfile subProfile("qbs-autotests-subprofile", s.get());
    subProfile.p.setValue("baseProfile", profileName());
    subProfile.p.setValue("cpp.includePaths", QStringList("/tmp/include1"));
    s->sync();
    QCOMPARE(runQbs(), 0);

    subProfile.p.setValue("cpp.includePaths", QStringList("/tmp/include2"));
    s->sync();
    QbsRunParameters params;
    params.command = "resolve";
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("main1.cpp"));
    QVERIFY(m_qbsStdout.contains("main2.cpp"));
}

void TestBlackbox::successiveChanges()
{
    QDir::setCurrent(testDataDir + "/successive-changes");
    QCOMPARE(runQbs(), 0);

    QbsRunParameters params("resolve", QStringList() << "products.theProduct.type:output,blubb");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);

    params.arguments << "project.version:2";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);
    QFile output(relativeProductBuildDir("theProduct") + "/output.out");
    QVERIFY2(output.open(QIODevice::ReadOnly), qPrintable(output.errorString()));
    const QByteArray version = output.readAll();
    QCOMPARE(version.constData(), "2");
}

void TestBlackbox::installedApp()
{
    QDir::setCurrent(testDataDir + "/installed_artifact");

    QCOMPARE(runQbs(), 0);
    const QByteArray output = m_qbsStdout;
    QVERIFY(regularFileExists(
        defaultInstallRoot + appendExecSuffix(QStringLiteral("/usr/bin/installedApp"), output)));

    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("qbs.installRoot:" + testDataDir
                                                            + "/installed-app"))), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(
        testDataDir + appendExecSuffix("/installed-app/usr/bin/installedApp", output)));

    QFile addedFile(defaultInstallRoot + QLatin1String("/blubb.txt"));
    QVERIFY(addedFile.open(QIODevice::WriteOnly));
    addedFile.close();
    QVERIFY(addedFile.exists());
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "--clean-install-root")), 0);
    QVERIFY(regularFileExists(
        defaultInstallRoot + appendExecSuffix(QStringLiteral("/usr/bin/installedApp"), output)));
    QVERIFY(regularFileExists(defaultInstallRoot + QLatin1String("/usr/src/main.cpp")));
    QVERIFY(!addedFile.exists());

    // Check whether changing install parameters on the product causes re-installation.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("installed_artifact.qbs", "qbs.installPrefix: \"/usr\"",
                    "qbs.installPrefix: '/usr/local'");
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(
        defaultInstallRoot
        + appendExecSuffix(QStringLiteral("/usr/local/bin/installedApp"), output)));
    QVERIFY(regularFileExists(defaultInstallRoot + QLatin1String("/usr/local/src/main.cpp")));

    // Check whether changing install parameters on the artifact causes re-installation.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("installed_artifact.qbs", "qbs.installDir: \"bin\"",
                    "qbs.installDir: 'custom'");
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(
        defaultInstallRoot
        + appendExecSuffix(QStringLiteral("/usr/local/custom/installedApp"), output)));

    // Check whether changing install parameters on a source file causes re-installation.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("installed_artifact.qbs", "qbs.installDir: \"src\"",
                    "qbs.installDir: 'source'");
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(defaultInstallRoot + QLatin1String("/usr/local/source/main.cpp")));

    // Check whether changing install parameters on the command line causes re-installation.
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("qbs.installRoot:" + relativeBuildDir()
                                                 + "/blubb"))), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(relativeBuildDir() + "/blubb/usr/local/source/main.cpp"));

    // Check --no-install
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "--no-install")), 0);
    QCOMPARE(QDir(defaultInstallRoot).entryList(QDir::NoDotAndDotDot).size(), 0);

    // Check --no-build (with and without an existing build graph)
    QbsRunParameters params("install", QStringList() << "--no-build");
    QCOMPARE(runQbs(params), 0);
    rmDirR(relativeBuildDir());
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Build graph not found"), m_qbsStderr.constData());
}

void TestBlackbox::installDuplicates()
{
    QDir::setCurrent(testDataDir + "/install-duplicates");

    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("Cannot install files"));
}

void TestBlackbox::installDuplicatesNoError()
{
    QDir::setCurrent(testDataDir + "/install-duplicates-no-error");

    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::installedSourceFiles()
{
    QDir::setCurrent(testDataDir + "/installed-source-files");

    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(defaultInstallRoot + QLatin1String("/readme.txt")));
    QVERIFY(regularFileExists(defaultInstallRoot + QLatin1String("/main.cpp")));
}

void TestBlackbox::toolLookup()
{
    QbsRunParameters params(QStringLiteral("setup-toolchains"), QStringList("--help"));
    params.profile.clear();
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::topLevelSearchPath()
{
    QDir::setCurrent(testDataDir + "/toplevel-searchpath");

    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("MyProduct"), m_qbsStderr.constData());
    params.arguments << ("project.qbsSearchPaths:" + QDir::currentPath() + "/qbs-resources");
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::checkProjectFilePath()
{
    QDir::setCurrent(testDataDir + "/project_filepath_check");
    QbsRunParameters params(QStringList("-f") << "project1.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("main.cpp"), m_qbsStdout.constData());
    QCOMPARE(runQbs(params), 0);

    params.arguments = QStringList("-f") << "project2.qbs";
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("project file"));

    params.arguments = QStringList("-f") << "project2.qbs";
    params.command = "resolve";
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("main2.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::checkTimestamps()
{
    QDir::setCurrent(testDataDir + "/check-timestamps");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling file.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QVERIFY(QFile::remove(relativeBuildGraphFilePath()));
    WAIT_FOR_NEW_TIMESTAMP();
    touch("file.h");
    QCOMPARE(runQbs(QStringList("--check-timestamps")), 0);
    QVERIFY2(m_qbsStdout.contains("compiling file.cpp"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::chooseModuleInstanceByPriorityAndVersion()
{
    QFETCH(QString, idol);
    QFETCH(QStringList, expectedSubStrings);
    QFETCH(bool, expectSuccess);
    QDir::setCurrent(testDataDir + "/choose-module-instance");
    rmDirR(relativeBuildDir());
    QbsRunParameters params(QStringList("modules.qbs.targetPlatform:" + idol));
    params.expectFailure = !expectSuccess;
    if (expectSuccess) {
        QCOMPARE(runQbs(params), 0);
    } else {
        QVERIFY(runQbs(params) != 0);
        return;
    }

    const QString installRoot = relativeBuildDir() + "/install-root/";
    QVERIFY(QFile::exists(installRoot + "/gerbil.txt"));
    QFile file(installRoot + "/gerbil.txt");
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString content = QString::fromUtf8(file.readAll());
    for (const auto &str : expectedSubStrings) {
        if (content.contains(str))
            continue;
        qDebug() << "content:" << content;
        qDebug() << "substring:" << str;
        QFAIL("missing substring");
    }
}

void TestBlackbox::chooseModuleInstanceByPriorityAndVersion_data()
{
    QTest::addColumn<QString>("idol");
    QTest::addColumn<QStringList>("expectedSubStrings");
    QTest::addColumn<bool>("expectSuccess");
    QTest::newRow("ringo")
            << "Beatles" << QStringList() << false;
    QTest::newRow("ritchie1")
            << "Deep Purple" << QStringList{"slipped", "litchi", "ritchie"} << true;
    QTest::newRow("ritchie2")
            << "Rainbow" << QStringList{"slipped", "litchi", "ritchie"} << true;
    QTest::newRow("lord")
            << "Whitesnake" << QStringList{"chewed", "cord", "lord"} << true;
}

class TemporaryDefaultProfileRemover
{
public:
    TemporaryDefaultProfileRemover(qbs::Settings *settings)
        : m_settings(settings), m_defaultProfile(settings->defaultProfile())
    {
        m_settings->remove(QStringLiteral("defaultProfile"));
    }

    ~TemporaryDefaultProfileRemover()
    {
        if (!m_defaultProfile.isEmpty())
            m_settings->setValue(QStringLiteral("defaultProfile"), m_defaultProfile);
    }

private:
    qbs::Settings *m_settings;
    const QString m_defaultProfile;
};

void TestBlackbox::assembly()
{
    QDir::setCurrent(testDataDir + "/assembly");

    QCOMPARE(runQbs(QbsRunParameters("build", {"-p", "properties"})), 0);

    const QVariantMap properties = ([&]() {
        QFile propertiesFile(relativeProductBuildDir("properties") + "/properties.json");
        if (propertiesFile.open(QIODevice::ReadOnly))
            return QJsonDocument::fromJson(propertiesFile.readAll()).toVariant().toMap();
        return QVariantMap{};
    })();
    QVERIFY(!properties.empty());
    const auto toolchain = properties.value("qbs.toolchain").toStringList();
    const auto architecture = properties.value("qbs.architecture").toString();
    QVERIFY(!toolchain.empty());
    const bool haveGcc = toolchain.contains("gcc");
    const bool haveMSVC = toolchain.contains("msvc");

    if (toolchain.contains("emscripten")) {
        QSKIP("Skipping test for emscripten");
    }

    if (haveMSVC && (architecture.contains("arm"))) {
        QSKIP("Skipping test for MSVC ARM");
    }

    QCOMPARE(runQbs(QbsRunParameters("build")), 0);

    QCOMPARE(m_qbsStdout.contains("assembling testa.s"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("compiling testb.S"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("compiling testc.sx"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("creating libtesta.a"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("creating libtestb.a"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("creating libtestc.a"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("creating testd.lib"), haveMSVC);
}

void TestBlackbox::autotestWithDependencies()
{
    QDir::setCurrent(testDataDir + "/autotest-with-dependencies");
    QCOMPARE(runQbs({"resolve"}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QCOMPARE(runQbs(QStringList({"-p", "autotest-runner"})), 0);
    QVERIFY2(m_qbsStdout.contains("i am the test app")
             && m_qbsStdout.contains("i am the helper"), m_qbsStdout.constData());
}

void TestBlackbox::autotestTimeout()
{
    QFETCH(QStringList, resolveParams);
    QFETCH(bool, expectFailure);
    QFETCH(QString, errorDetails);
    QDir::setCurrent(testDataDir + "/autotest-timeout");
    QbsRunParameters resolveParameters("resolve", resolveParams);
    QCOMPARE(runQbs(resolveParameters), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QbsRunParameters buildParameters(QStringList({"-p", "autotest-runner"}));
    buildParameters.expectFailure = expectFailure;
    if (expectFailure) {
        QVERIFY(runQbs(buildParameters) != 0);
        QVERIFY(
            m_qbsStderr.contains("cancelled") && m_qbsStderr.contains("timeout")
            && m_qbsStderr.contains(errorDetails.toLocal8Bit()));
    }
    else
        QVERIFY(runQbs(buildParameters) == 0);
}

void TestBlackbox::autotestTimeout_data()
{
    QTest::addColumn<QStringList>("resolveParams");
    QTest::addColumn<bool>("expectFailure");
    QTest::addColumn<QString>("errorDetails");
    QTest::newRow("no timeout") << QStringList() << false << QString();
    QTest::newRow("timeout on test")
        << QStringList({"products.testApp.autotest.timeout:2"}) << true << QString("testApp");
    QTest::newRow("timeout on runner")
        << QStringList({"products.autotest-runner.timeout:2"}) << true << QString("testApp");
}

void TestBlackbox::autotests_data()
{
    QTest::addColumn<QString>("evilPropertySpec");
    QTest::addColumn<QByteArray>("expectedErrorMessage");
    QTest::newRow("missing arguments") << QString("products.test1.autotest.arguments:[]")
                                       << QByteArray("This test needs exactly one argument");
    QTest::newRow("missing working dir") << QString("products.test2.autotest.workingDir:''")
                                         << QByteArray("Test resource not found");
    QTest::newRow("missing flaky specifier")
            << QString("products.test3.autotest.allowFailure:false")
            << QByteArray("I am an awful test");
    QTest::newRow("everything's fine") << QString() << QByteArray();
}

void TestBlackbox::autotests()
{
    QDir::setCurrent(testDataDir + "/autotests");
    QFETCH(QString, evilPropertySpec);
    QFETCH(QByteArray, expectedErrorMessage);
    QbsRunParameters resolveParams("resolve");
    if (!evilPropertySpec.isEmpty())
        resolveParams.arguments << evilPropertySpec;
    QCOMPARE(runQbs(resolveParams), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QbsRunParameters testParams(QStringList{"-p", "autotest-runner"});
    if (!evilPropertySpec.isEmpty())
        testParams.expectFailure = true;
    QCOMPARE(runQbs(testParams) == 0, !testParams.expectFailure);
    if (testParams.expectFailure) {
        QVERIFY2(m_qbsStderr.contains(expectedErrorMessage), m_qbsStderr.constData());
        return;
    }
    QVERIFY2(m_qbsStdout.contains("running test test1")
             && m_qbsStdout.contains("running test test2")
             && m_qbsStdout.contains("running test test3"), m_qbsStdout.constData());
    QCOMPARE(m_qbsStdout.count("PASS"), 2);
    QCOMPARE(m_qbsStderr.count("FAIL"), 1);
}

void TestBlackbox::auxiliaryInputsFromDependencies()
{
    QDir::setCurrent(testDataDir + "/aux-inputs-from-deps");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("generating dummy.out"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("products.dep.sleep:false"))), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("generating dummy.out"), m_qbsStdout.constData());
}

void TestBlackbox::explicitlyDependsOn()
{
    QFETCH(QString, useExplicitlyDependsOn);
    QFETCH(QString, useExplicitlyDependsOnFromDependencies);
    QFETCH(QString, useModule);
    QFETCH(bool, expectFailure);

    QDir::setCurrent(testDataDir + "/explicitly-depends-on");
    QbsRunParameters params("",
                QStringList("products.prod1.useExplicitlyDependsOn:" + useExplicitlyDependsOn)
                << "products.prod1.useExplicitlyDependsOnFromDependencies:"
                    + useExplicitlyDependsOnFromDependencies
                << "projects.proj1.useModule:"
                    + useModule);
    params.expectFailure = expectFailure;

    rmDirR(relativeBuildDir());

    if (params.expectFailure) {
        // Build should fail because a rule cycle is created within the product when
        // explicitlyDependsOn is used.
        QVERIFY(runQbs(params) != 0);
        QVERIFY2(m_qbsStderr.contains("Cycle detected in rule dependencies"),
                 m_qbsStderr.constData());
    } else {
        // When explicitlyDependsOnFromDependencies is used, build should succeed due to the
        // "final" tag being pulled in from dependencies.
        QCOMPARE(runQbs(params), 0);

        if (useModule == QLatin1String("false")) {
            QVERIFY2(m_qbsStdout.contains("creating 'product-fish.txt' tagged with 'final'"),
                     m_qbsStdout.constData());
            QVERIFY2(m_qbsStdout.contains("Using explicitlyDependsOnArtifact: product-fish.txt"),
                     m_qbsStdout.constData());
            QVERIFY2(m_qbsStdout.contains("step1 -> step2"), m_qbsStdout.constData());
            QVERIFY2(m_qbsStdout.contains("step2 -> step3"), m_qbsStdout.constData());
            QVERIFY2(m_qbsStdout.contains("step3 -> final"), m_qbsStdout.constData());
        } else {
            QVERIFY2(!m_qbsStdout.contains("creating 'product-fish.txt' tagged with 'final'"),
                     m_qbsStdout.constData());
            QVERIFY2(m_qbsStdout.contains("Using explicitlyDependsOnArtifact: module-fish.txt"),
                     m_qbsStdout.constData());
            QVERIFY2(m_qbsStdout.contains("step1 -> step2"), m_qbsStdout.constData());
            QVERIFY2(m_qbsStdout.contains("step2 -> step3"), m_qbsStdout.constData());
            QVERIFY2(m_qbsStdout.contains("step3 -> final"), m_qbsStdout.constData());
        }
    }
}

void TestBlackbox::explicitlyDependsOn_data()
{
    QTest::addColumn<QString>("useExplicitlyDependsOn");
    QTest::addColumn<QString>("useExplicitlyDependsOnFromDependencies");
    QTest::addColumn<QString>("useModule");
    QTest::addColumn<bool>("expectFailure");

    QTest::newRow("useExplicitlyDependsOn -> causes cycle")
            << "true" << "false" << "false" << true;
    QTest::newRow("explicitlyDependsOnFromDependencies + product")
            << "false" << "true" << "false" << false;
    QTest::newRow("explicitlyDependsOnFromDependencies + module + filesAreTargets")
            << "false" << "true" << "true" << false;
}

static bool haveMakeNsis()
{
    QStringList regKeys;
    regKeys << QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\NSIS")
            << QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\NSIS");

    QStringList paths = QProcessEnvironment::systemEnvironment().value("PATH")
            .split(HostOsInfo::pathListSeparator(), Qt::SkipEmptyParts);

    for (const QString &key : std::as_const(regKeys)) {
        QSettings settings(key, QSettings::NativeFormat);
        QString str = settings.value(QStringLiteral(".")).toString();
        if (!str.isEmpty())
            paths.prepend(str);
    }

    bool haveMakeNsis = false;
    for (const QString &path : std::as_const(paths)) {
        if (regularFileExists(QDir::fromNativeSeparators(path) +
                          HostOsInfo::appendExecutableSuffix(QStringLiteral("/makensis")))) {
            haveMakeNsis = true;
            break;
        }
    }

    return haveMakeNsis;
}

void TestBlackbox::nsis()
{
    if (!haveMakeNsis()) {
        QSKIP("makensis is not installed");
        return;
    }

    QDir::setCurrent(testDataDir + "/nsis");
    QVERIFY(runQbs() == 0);

    const bool targetIsWindows = m_qbsStdout.contains("is Windows: true");
    const bool targetIsNotWindows = m_qbsStdout.contains("is Windows: false");
    QCOMPARE(targetIsWindows, !targetIsNotWindows);

    QCOMPARE(m_qbsStdout.contains("compiling hello.nsi"), targetIsWindows);
    QCOMPARE(
        m_qbsStdout.contains("SetCompressor ignored due to previous call with the /FINAL switch"),
        targetIsWindows);
    QVERIFY(!QFile::exists(defaultInstallRoot + "/you-should-not-see-a-file-with-this-name.exe"));
}

void TestBlackbox::nsisDependencies()
{
    if (!haveMakeNsis()) {
        QSKIP("makensis is not installed");
        return;
    }

    QDir::setCurrent(testDataDir + "/nsisDependencies");
    QCOMPARE(runQbs(), 0);

    const bool targetIsWindows = m_qbsStdout.contains("is Windows: true");
    const bool targetIsNotWindows = m_qbsStdout.contains("is Windows: false");

    QCOMPARE(targetIsWindows, !targetIsNotWindows);
    QCOMPARE(m_qbsStdout.contains("compiling hello.nsi"), targetIsWindows);
}

void TestBlackbox::outOfDateMarking()
{
    QDir::setCurrent(testDataDir + "/out-of-date-marking");
    for (int i = 0; i < 25; ++i) {
        QCOMPARE(runQbs(), 0);
        QVERIFY2(m_qbsStdout.contains("generating myheader.h"), qPrintable(QString::number(i)));
        QVERIFY2(m_qbsStdout.contains("compiling main.c"), qPrintable(QString::number(i)));
    }
}

void TestBlackbox::enableExceptions()
{
    QFETCH(QString, file);
    QFETCH(bool, enable);
    QFETCH(bool, expectSuccess);

    QDir::setCurrent(testDataDir + QStringLiteral("/enableExceptions"));

    QbsRunParameters params;
    params.arguments = QStringList() << "-f" << file
                                     << (QStringLiteral("modules.cpp.enableExceptions:")
                                         + (enable ? "true" : "false"));
    params.expectFailure = !expectSuccess;
    rmDirR(relativeBuildDir());
    if (!params.expectFailure)
        QCOMPARE(runQbs(params), 0);
    else
        QVERIFY(runQbs(params) != 0);
}

void TestBlackbox::enableExceptions_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<bool>("enable");
    QTest::addColumn<bool>("expectSuccess");

    QTest::newRow("no exceptions, enabled") << "none.qbs" << true << true;
    QTest::newRow("no exceptions, disabled") << "none.qbs" << false << true;

    QTest::newRow("C++ exceptions, enabled") << "exceptions.qbs" << true << true;
    QTest::newRow("C++ exceptions, disabled") << "exceptions.qbs" << false << false;

    if (HostOsInfo::isMacosHost()) {
        QTest::newRow("Objective-C exceptions, enabled") << "exceptions-objc.qbs" << true << true;
        QTest::newRow("Objective-C exceptions in Objective-C++ source, enabled") << "exceptions-objcpp.qbs" << true << true;
        QTest::newRow("C++ exceptions in Objective-C++ source, enabled") << "exceptions-objcpp-cpp.qbs" << true << true;
        QTest::newRow("Objective-C, disabled") << "exceptions-objc.qbs" << false << false;
        QTest::newRow("Objective-C exceptions in Objective-C++ source, disabled") << "exceptions-objcpp.qbs" << false << false;
        QTest::newRow("C++ exceptions in Objective-C++ source, disabled") << "exceptions-objcpp-cpp.qbs" << false << false;
    }
}

void TestBlackbox::enableRtti()
{
    QDir::setCurrent(testDataDir + QStringLiteral("/enableRtti"));

    QbsRunParameters params;

    params.arguments = QStringList() << "modules.cpp.enableRtti:true";
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(params), 0);

    if (HostOsInfo::isMacosHost()) {
        params.arguments = QStringList() << "modules.cpp.enableRtti:true"
                                         << "project.treatAsObjcpp:true";
        rmDirR(relativeBuildDir());
        QCOMPARE(runQbs(params), 0);
    }

    params.expectFailure = true;

    params.arguments = QStringList() << "modules.cpp.enableRtti:false";
    rmDirR(relativeBuildDir());
    QVERIFY(runQbs(params) != 0);

    if (HostOsInfo::isMacosHost()) {
        params.arguments = QStringList() << "modules.cpp.enableRtti:false"
                                         << "project.treatAsObjcpp:true";
        rmDirR(relativeBuildDir());
        QVERIFY(runQbs(params) != 0);
    }
}

void TestBlackbox::envMerging()
{
    QDir::setCurrent(testDataDir + "/env-merging");
    QbsRunParameters params("resolve");
    QString pathVal = params.environment.value("PATH");
    pathVal.prepend(HostOsInfo::pathListSeparator()).prepend("/opt/blackbox/bin");
    const QString keyName = HostOsInfo::isWindowsHost() ? "pATh" : "PATH";
    params.environment.insert(keyName, pathVal);
    QCOMPARE(runQbs(params), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    params.command = "build";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains(QByteArray("PATH=/opt/tool/bin")
                                  + HostOsInfo::pathListSeparator().toLatin1())
             && m_qbsStdout.contains(HostOsInfo::pathListSeparator().toLatin1()
                                     + QByteArray("/opt/blackbox/bin")),
             m_qbsStdout.constData());
}

void TestBlackbox::envNormalization()
{
    QDir::setCurrent(testDataDir + "/env-normalization");
    QbsRunParameters params;
    params.environment.insert("myvar", "x");
    QCOMPARE(runQbs(params), 0);
    if (HostOsInfo::isWindowsHost())
        QVERIFY2(m_qbsStdout.contains("\"MYVAR\":\"x\""), m_qbsStdout.constData());
    else
        QVERIFY2(m_qbsStdout.contains("\"myvar\":\"x\""), m_qbsStdout.constData());
}

void TestBlackbox::generatedArtifactAsInputToDynamicRule()
{
    QDir::setCurrent(testDataDir + "/generated-artifact-as-input-to-dynamic-rule");
    QCOMPARE(runQbs(), 0);
    const QString oldFile = relativeProductBuildDir("p") + "/old.txt";
    QVERIFY2(regularFileExists(oldFile), qPrintable(oldFile));
    WAIT_FOR_NEW_TIMESTAMP();
    QFile inputFile("input.txt");
    QVERIFY2(inputFile.open(QIODevice::WriteOnly), qPrintable(inputFile.errorString()));
    inputFile.resize(0);
    inputFile.write("new.txt");
    inputFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!regularFileExists(oldFile), qPrintable(oldFile));
    const QString newFile = relativeProductBuildDir("p") + "/new.txt";
    QVERIFY2(regularFileExists(newFile), qPrintable(oldFile));
    QVERIFY2(m_qbsStdout.contains("generating"), m_qbsStdout.constData());
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("generating"), m_qbsStdout.constData());
}

void TestBlackbox::generateLinkerMapFile()
{
    QDir::setCurrent(testDataDir + "/generate-linker-map-file");
    QCOMPARE(runQbs(), 0);
    const bool isUsed = m_qbsStdout.contains("use test: true");
    const bool isNotUsed = m_qbsStdout.contains("use test: false");
    QVERIFY(isUsed != isNotUsed);
    if (isUsed) {
        QVERIFY(QFile::exists(relativeProductBuildDir("app-map")
            + "/app-map.map"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("app-nomap")
            + "/app-nomap.map"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("app-nomap-default")
            + "/app-nomap-default.map"));
    } else {
        QSKIP("Unsupported toolchain. Skipping.");
    }
}

void TestBlackbox::generator()
{
    QFETCH(QString, inputFile);
    QFETCH(QStringList, toBeCompiled);
    QDir::setCurrent(testDataDir + "/generator");
    if (!inputFile.isEmpty()) {
        WAIT_FOR_NEW_TIMESTAMP();
        QFile input(inputFile);
        QFile output("input.txt");
        QVERIFY2(!output.exists() || output.remove(), qPrintable(output.errorString()));
        QVERIFY2(input.copy(output.fileName()), qPrintable(input.errorString()));
        touch(output.fileName());
    }
    QCOMPARE(runQbs(), 0);
    QCOMPARE(toBeCompiled.contains("main.cpp"), m_qbsStdout.contains("compiling main.cpp"));
    QCOMPARE(toBeCompiled.contains("file1.cpp"), m_qbsStdout.contains("compiling file1.cpp"));
    QCOMPARE(toBeCompiled.contains("file2.cpp"), m_qbsStdout.contains("compiling file2.cpp"));
}

void TestBlackbox::generator_data()
{
    QTest::addColumn<QString>("inputFile");
    QTest::addColumn<QStringList>("toBeCompiled");
    QTest::newRow("both") << "input.both.txt" << QStringList{"main.cpp", "file1.cpp", "file2.cpp"};
    QTest::newRow("file1") << "input.file1.txt" << QStringList{"file1.cpp"};
    QTest::newRow("file2") << "input.file2.txt" << QStringList{"file2.cpp"};
    QTest::newRow("none") << "input.none.txt" << QStringList();
    QTest::newRow("both again") << "input.both.txt" << QStringList{"file1.cpp", "file2.cpp"};
    QTest::newRow("no update") << QString() << QStringList();
}

void TestBlackbox::nodejs()
{
    const SettingsPtr s = settings();
    qbs::Profile p(profileName(), s.get());

    int status;
    findNodejs(&status);
    QCOMPARE(status, 0);

    QDir::setCurrent(testDataDir + QLatin1String("/nodejs"));

    status = runQbs();
    if (p.value("nodejs.toolchainInstallPath").toString().isEmpty()
            && status != 0 && m_qbsStderr.contains("toolchainInstallPath")) {
        QSKIP("nodejs.toolchainInstallPath not set and automatic detection failed");
    }

    if (p.value("nodejs.packageManagerPrefixPath").toString().isEmpty()
            && status != 0 && m_qbsStderr.contains("nodejs.packageManagerPrefixPath")) {
        QSKIP("nodejs.packageManagerFilePath not set and automatic detection failed");
    }

    if (p.value("nodejs.interpreterFilePath").toString().isEmpty()
            && status != 0 && m_qbsStderr.contains("interpreterPath")) {
        QSKIP("nodejs.interpreterFilePath not set and automatic detection failed");
    }

    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QCOMPARE(status, 0);

    QbsRunParameters params;
    params.command = QLatin1String("run");
    QCOMPARE(runQbs(params), 0);
    QVERIFY((bool)m_qbsStdout.contains("hello world"));
    QVERIFY(regularFileExists(relativeProductBuildDir("hello") + "/hello.js"));
}

void TestBlackbox::typescript()
{
    if (qEnvironmentVariableIsSet("GITHUB_ACTIONS"))
        QSKIP("Skip this test when running on GitHub");

    if (qEnvironmentVariableIsSet("EMSDK"))
        QSKIP("Skip this test when running with wasm");

    const SettingsPtr s = settings();
    qbs::Profile p(profileName(), s.get());

    int status;
    findTypeScript(&status);
    QCOMPARE(status, 0);

    QDir::setCurrent(testDataDir + QLatin1String("/typescript"));

    QbsRunParameters params;
    params.expectFailure = true;
    status = runQbs(params);
    if (p.value("typescript.toolchainInstallPath").toString().isEmpty() && status != 0) {
        if (m_qbsStderr.contains("Path\" must be specified"))
            QSKIP("typescript probe failed");
        if (m_qbsStderr.contains("typescript.toolchainInstallPath"))
            QSKIP("typescript.toolchainInstallPath not set and automatic detection failed");
        if (m_qbsStderr.contains("nodejs.interpreterFilePath"))
            QSKIP("nodejs.interpreterFilePath not set and automatic detection failed");
    }

    if (status != 0)
        qDebug() << m_qbsStderr;
    QCOMPARE(status, 0);

    params.expectFailure = false;
    params.command = QStringLiteral("run");
    params.arguments = QStringList() << "-p" << "animals";
    QCOMPARE(runQbs(params), 0);

    QVERIFY(regularFileExists(relativeProductBuildDir("animals") + "/animals.js"));
    QVERIFY(regularFileExists(relativeProductBuildDir("animals") + "/extra.js"));
    QVERIFY(regularFileExists(relativeProductBuildDir("animals") + "/main.js"));
}

void TestBlackbox::undefinedTargetPlatform()
{
    QDir::setCurrent(testDataDir + "/undefined-target-platform");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::importInPropertiesCondition()
{
    QDir::setCurrent(testDataDir + "/import-in-properties-condition");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::importSearchPath()
{
    QDir::setCurrent(testDataDir + "/import-searchpath");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("compiling somefile.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::importingProduct()
{
    QDir::setCurrent(testDataDir + "/importing-product");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::importsConflict()
{
    QDir::setCurrent(testDataDir + "/imports-conflict");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::includeLookup()
{
    QDir::setCurrent(testDataDir + "/includeLookup");
    QCOMPARE(runQbs({"resolve"}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QbsRunParameters params;
    params.command = "run";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("definition.."), m_qbsStdout.constData());
}

void TestBlackbox::inputTagsChangeTracking_data()
{
    QTest::addColumn<QString>("generateInput");
    QTest::newRow("source artifact") << QString("no");
    QTest::newRow("generated artifact (static)") << QString("static");
    QTest::newRow("generated artifact (dynamic)") << QString("dynamic");
}

void TestBlackbox::inputTagsChangeTracking()
{
    QDir::setCurrent(testDataDir + "/input-tags-change-tracking");
    const QString xOut = QDir::currentPath() + '/' + relativeProductBuildDir("p") + "/x.out";
    const QString yOut = QDir::currentPath() + '/' + relativeProductBuildDir("p") + "/y.out";
    QFETCH(QString, generateInput);
    const QbsRunParameters resolveParams("resolve",
                                         QStringList("products.p.generateInput:" + generateInput));
    QCOMPARE(runQbs(resolveParams), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("generating input.txt") == (generateInput == "static"));
    QVERIFY2(!QFile::exists(xOut), qPrintable(xOut));
    QVERIFY2(!QFile::exists(yOut), qPrintable(yOut));
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("input-tags-change-tracking.qbs", "Tags: [\"txt\", \"empty\"]",
                    "Tags: \"txt\"");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(QFile::exists(xOut), qPrintable(xOut));
    QVERIFY2(!QFile::exists(yOut), qPrintable(yOut));
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("input-tags-change-tracking.qbs", "Tags: \"txt\"",
                    "Tags: [\"txt\", \"y\"]");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!QFile::exists(xOut), qPrintable(xOut));
    QVERIFY2(QFile::exists(yOut), qPrintable(yOut));
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("input-tags-change-tracking.qbs", "Tags: [\"txt\", \"y\"]",
                    "Tags: [\"txt\", \"empty\"]");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!QFile::exists(xOut), qPrintable(xOut));
    QVERIFY2(!QFile::exists(yOut), qPrintable(yOut));
}

void TestBlackbox::outputArtifactAutoTagging()
{
    QDir::setCurrent(testDataDir + QLatin1String("/output-artifact-auto-tagging"));

    QCOMPARE(runQbs(), 0);
    QVERIFY(
        regularFileExists(relativeExecutableFilePath("output-artifact-auto-tagging", m_qbsStdout)));
}

void TestBlackbox::outputRedirection()
{
    QDir::setCurrent(testDataDir + "/output-redirection");
    QCOMPARE(runQbs(), 0);
    TEXT_FILE_COMPARE("output.txt", relativeProductBuildDir("the-product") + "/output.txt");
    TEXT_FILE_COMPARE("output.bin", relativeProductBuildDir("the-product") + "/output.bin");
}

void TestBlackbox::wildCardsAndRules()
{
    QDir::setCurrent(testDataDir + "/wildcards-and-rules");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("creating output artifact"));
    QFile output(relativeProductBuildDir("wildcards-and-rules") + "/test.mytype");
    QVERIFY2(output.open(QIODevice::ReadOnly), qPrintable(output.errorString()));
    QCOMPARE(output.readAll().count('\n'), 1);
    output.close();

    // Add input.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("input2.inp");
    QbsRunParameters params;
    params.expectFailure = true;
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("creating output artifact"));
    QVERIFY2(output.open(QIODevice::ReadOnly), qPrintable(output.errorString()));
    QCOMPARE(output.readAll().count('\n'), 2);
    output.close();

    // Add "explicitlyDependsOn".
    WAIT_FOR_NEW_TIMESTAMP();
    touch("dep.dep");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("creating output artifact"));

    // Add nothing.
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("creating output artifact"));
}

void TestBlackbox::wildCardsAndChangeTracking_data()
{
    QTest::addColumn<QString>("dirToModify");
    QTest::addColumn<bool>("expectReResolve");

    QTest::newRow("root path") << QString(".") << false;
    QTest::newRow("dir with recursive match") << QString("recursive1") << false;
    QTest::newRow("non-recursive base dir") << QString("nonrecursive") << true;
    QTest::newRow("empty base dir with file patterns") << QString("nonrecursive/empty") << true;
}

void TestBlackbox::wildCardsAndChangeTracking()
{
    QFETCH(QString, dirToModify);
    QFETCH(bool, expectReResolve);

    const QString srcDir = testDataDir + "/wildcards-and-change-tracking";
    QDir::setCurrent(srcDir);
    rmDirR("default");
    QDir::current().mkdir("nonrecursive/empty");

    QCOMPARE(runQbs({"resolve"}), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    touch(dirToModify + "/blubb.txt");
    QCOMPARE(runQbs({"resolve"}), 0);
    QCOMPARE(m_qbsStdout.contains("Resolving"), expectReResolve);
}

void TestBlackbox::loadableModule()
{
    QDir::setCurrent(testDataDir + QLatin1String("/loadablemodule"));

    QCOMPARE(runQbs({"resolve"}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QbsRunParameters params;
    params.command = "run";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("foo = 42"), m_qbsStdout.constData());
}

void TestBlackbox::localDeployment()
{
    QDir::setCurrent(testDataDir + "/localDeployment");
    QFile main("main.cpp");
    QVERIFY(main.open(QIODevice::ReadOnly));
    QByteArray content = main.readAll();
    content.replace('\r', "");
    QCOMPARE(runQbs({"resolve"}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");

    QbsRunParameters params;
    params.command = "run";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains(content), m_qbsStdout.constData());
}

void TestBlackbox::makefileGenerator()
{
    QDir::setCurrent(testDataDir + "/makefile-generator");
    const QbsRunParameters params("generate", QStringList{"-g", "makefile"});
    QCOMPARE(runQbs(params), 0);
    if (HostOsInfo::isWindowsHost())
        return;
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QProcess make;
    make.setWorkingDirectory(QDir::currentPath() + '/' + relativeBuildDir());
    const QString customInstallRoot = QDir::currentPath() + "/my-install-root";
    make.start("make", QStringList{"INSTALL_ROOT=" + customInstallRoot, "install"});
    QVERIFY(waitForProcessSuccess(make));
    QVERIFY(QFile::exists(relativeExecutableFilePath("the app", m_qbsStdout)));
    QVERIFY(!QFile::exists(relativeBuildGraphFilePath()));
    QProcess app;
    app.start(customInstallRoot + "/usr/local/bin/the app", QStringList());
    QVERIFY(waitForProcessSuccess(app));
    const QByteArray appStdout = app.readAllStandardOutput();
    QVERIFY2(appStdout.contains("Hello, World!"), appStdout.constData());
    make.start("make", QStringList("clean"));
    QVERIFY(waitForProcessSuccess(make));
    QVERIFY(!QFile::exists(relativeExecutableFilePath("the app", m_qbsStdout)));
}

void TestBlackbox::maximumCLanguageVersion()
{
    QDir::setCurrent(testDataDir + "/maximum-c-language-version");
    QCOMPARE(runQbs(QbsRunParameters("resolve",
                                     QStringList("products.app.enableNewestModule:true"))), 0);
    const bool isMsvc = m_qbsStdout.contains("is msvc: true");
    if (isMsvc && m_qbsStdout.contains("is old msvc: true"))
        QSKIP("MSVC supports setting the C language version only from version 16.8, and Clang from version 13.");
    QCOMPARE(runQbs(QStringList({"--command-echo-mode", "command-line", "-n"})), 0);
    QVERIFY2(m_qbsStdout.contains("c11") || m_qbsStdout.contains("c1x"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QbsRunParameters("resolve",
                                     QStringList("products.app.enableNewestModule:false"))), 0);
    QCOMPARE(runQbs(QStringList({"--command-echo-mode", "command-line", "-n"})), 0);
    if (isMsvc)
        QVERIFY2(!m_qbsStdout.contains("c11"), m_qbsStdout.constData());
    else
        QVERIFY2(m_qbsStdout.contains("c99"), m_qbsStdout.constData());
}

void TestBlackbox::maximumCxxLanguageVersion()
{
    QDir::setCurrent(testDataDir + "/maximum-cxx-language-version");
    QCOMPARE(runQbs(QbsRunParameters("resolve",
                                     QStringList("products.app.enableNewestModule:true"))), 0);
    QCOMPARE(runQbs(QStringList({"--command-echo-mode", "command-line", "-n"})), 0);
    QVERIFY2(m_qbsStdout.contains("c++23") || m_qbsStdout.contains("c++2b")
             || m_qbsStdout.contains("c++latest"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QbsRunParameters("resolve",
                                     QStringList("products.app.enableNewestModule:false"))), 0);
    QCOMPARE(runQbs(QStringList({"--command-echo-mode", "command-line", "-n"})), 0);
    QVERIFY2(m_qbsStdout.contains("c++14") || m_qbsStdout.contains("c++1y"),
             m_qbsStdout.constData());
}

void TestBlackbox::minimumSystemVersion()
{
    rmDirR(relativeBuildDir());
    QDir::setCurrent(testDataDir + "/minimumSystemVersion");
    QFETCH(QString, file);
    QFETCH(QString, output);
    QbsRunParameters params({ "-f", file + ".qbs" });
    params.command = "resolve";
    QCOMPARE(runQbs(params), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    params.command = "run";
    QCOMPARE(runQbs(params), 0);
    if (m_qbsStdout.contains("Unsupported compiler"))
        QSKIP("Unsupported compiler");
    if (!m_qbsStdout.contains(output.toUtf8())) {
        qDebug() << "expected output:" << qPrintable(output);
        qDebug() << "actual output:" << m_qbsStdout.constData();
    }
    QVERIFY(m_qbsStdout.contains(output.toUtf8()));
}

static qbs::Version fromMinimumDeploymentTargetValue(int v, bool isMacOS)
{
    if (isMacOS && v < 100000)
        return qbs::Version(v / 100, v / 10 % 10, v % 10);
    return qbs::Version(v / 10000, v / 100 % 100, v % 100);
}

static int toMinimumDeploymentTargetValue(const qbs::Version &v, bool isMacOS)
{
    if (isMacOS && v < qbs::Version(10, 10))
        return (v.majorVersion() * 100) + (v.minorVersion() * 10) + v.patchLevel();
    return (v.majorVersion() * 10000) + (v.minorVersion() * 100) + v.patchLevel();
}

static qbs::Version defaultClangMinimumDeploymentTarget()
{
    QProcess process;
    process.start("/usr/bin/xcrun", {"-sdk", "macosx", "clang++",
                                     "-target", "x86_64-apple-macosx-macho",
                                     "-dM", "-E", "-x", "objective-c++", "/dev/null"});
    if (waitForProcessSuccess(process)) {
        const auto lines = process.readAllStandardOutput().split('\n');
        for (const auto &line : lines) {
            static const QByteArray prefix =
                "#define __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ ";
            if (line.startsWith(prefix)) {
                bool ok = false;
                int v = line.mid(prefix.size()).trimmed().toInt(&ok);
                if (ok)
                    return fromMinimumDeploymentTargetValue(v, true);
                break;
            }
        }
    }

    return qbs::Version();
}

void TestBlackbox::minimumSystemVersion_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("output");

    // Don't check for the full "version X.Y.Z\n" on macOS as some older versions of otool don't
    // show the patch version. Instead, simply check for "version X.Y" with no trailing \n.

    const QString unspecified = []() -> QString {
        if (HostOsInfo::isMacosHost()) {
            const auto v = defaultClangMinimumDeploymentTarget();
            auto result = "__MAC_OS_X_VERSION_MIN_REQUIRED="
                    + QString::number(toMinimumDeploymentTargetValue(v, true));
            if (v >= qbs::Version(10, 14))
                result += "\nminos ";
            else
                result += "\nversion ";
            result += QString::number(v.majorVersion()) + "." + QString::number(v.minorVersion());
            return result;
        }

        if (HostOsInfo::isWindowsHost())
            return "WINVER is not defined\n";

        return "";
    }();

    const QString specific = []() -> QString {
        if (HostOsInfo::isMacosHost()) {
            if (HostOsInfo::hostOSArchitecture() == "arm64")
                return "__MAC_OS_X_VERSION_MIN_REQUIRED=110000\nminos 11.0\n";
            else
                return "__MAC_OS_X_VERSION_MIN_REQUIRED=1070\nversion 10.7\n";
        }

        if (HostOsInfo::isWindowsHost())
            return "WINVER=1538\n6.02 operating system version\n6.02 subsystem version\n";

        return "";
    }();

    QTest::newRow("unspecified") << "unspecified" << unspecified;
    QTest::newRow("unspecified-forced") << "unspecified-forced" << unspecified;
    if (HostOsInfo::isWindowsHost() || HostOsInfo::isMacosHost())
        QTest::newRow("specific") << "specific" << specific;
    if (HostOsInfo::isWindowsHost())
        QTest::newRow("fakewindows") << "fakewindows" << "WINVER=1283\n5.03 operating system "
                                                         "version\n5.03 subsystem version\n";
    if (HostOsInfo::isMacosHost()) {
        if (HostOsInfo::hostOSArchitecture() == "arm64") {
            QTest::newRow("macappstore") << "macappstore"
                                         << "__MAC_OS_X_VERSION_MIN_REQUIRED=110000";
        } else {
            QTest::newRow("macappstore") << "macappstore"
                                         << "__MAC_OS_X_VERSION_MIN_REQUIRED=1071";
        }
    }
}

void TestBlackbox::missingBuildGraph()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QDir::setCurrent(tmpDir.path());
    QFETCH(QString, configName);
    const QStringList commands({"clean", "dump-nodes-tree", "status", "update-timestamps"});
    const QString actualConfigName = configName.isEmpty() ? QString("default") : configName;
    QbsRunParameters params;
    params.expectFailure = true;
    params.arguments << QLatin1String("config:") + actualConfigName;
    for (const QString &command : std::as_const(commands)) {
        params.command = command;
        QVERIFY2(runQbs(params) != 0, qPrintable(command));
        const QString expectedErrorMessage = QString("Build graph not found for "
                                                     "configuration '%1'").arg(actualConfigName);
        if (!m_qbsStderr.contains(expectedErrorMessage.toLocal8Bit())) {
            qDebug() << command;
            qDebug() << expectedErrorMessage;
            qDebug() << m_qbsStderr;
            QFAIL("unexpected error message");
        }
    }
}

void TestBlackbox::missingBuildGraph_data()
{
    QTest::addColumn<QString>("configName");
    QTest::newRow("implicit config name") << QString();
    QTest::newRow("explicit config name") << QString("customConfig");
}

void TestBlackbox::missingDependency()
{
    QDir::setCurrent(testDataDir + "/missing-dependency");
    QbsRunParameters params;
    params.expectFailure = true;
    params.arguments << "-p" << "theApp";
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(!m_qbsStderr.contains("ASSERT"), m_qbsStderr.constData());
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "-p" << "theDep")), 0);
    params.expectFailure = false;
    params.arguments << "-vv";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStderr.contains("false positive"));
}

void TestBlackbox::missingProjectFile()
{
    QDir::setCurrent(testDataDir + "/missing-project-file/empty-dir");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("No project file given and none found in current directory"),
             m_qbsStderr.constData());
    QDir::setCurrent(testDataDir + "/missing-project-file");
    params.arguments << "-f" << "empty-dir";
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("No project file found in directory"), m_qbsStderr.constData());
    params.arguments = QStringList() << "-f" << "ambiguous-dir";
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("More than one project file found in directory"),
             m_qbsStderr.constData());
    params.expectFailure = false;
    params.arguments = QStringList() << "-f" << "project-dir";
    QCOMPARE(runQbs(params), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    touch("project-dir/file.cpp");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling file.cpp"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::missingOverridePrefix()
{
    QDir::setCurrent(testDataDir + "/missing-override-prefix");
    QbsRunParameters params;
    params.expectFailure = true;
    params.arguments << "blubb.whatever:false";
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Property override key 'blubb.whatever' not understood"),
             m_qbsStderr.constData());
}

void TestBlackbox::moduleConditions()
{
    QDir::setCurrent(testDataDir + "/module-conditions");
    QCOMPARE(runQbs(), 0);
    QCOMPARE(m_qbsStdout.count("loaded m1"), 1);
    QCOMPARE(m_qbsStdout.count("loaded m2"), 2);
    QCOMPARE(m_qbsStdout.count("loaded m3"), 1);
    QCOMPARE(m_qbsStdout.count("loaded m4"), 1);
}

void TestBlackbox::movedFileDependency()
{
    QDir::setCurrent(testDataDir + "/moved-file-dependency");
    const QString subdir2 = QDir::currentPath() + "/subdir2";
    QVERIFY(QDir::current().mkdir(subdir2));
    const QString oldHeaderFilePath = QDir::currentPath() + "/subdir1/theheader.h";
    const QString newHeaderFilePath = subdir2 + "/theheader.h";
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());

    QFile f(oldHeaderFilePath);
    QVERIFY2(f.rename(newHeaderFilePath), qPrintable(f.errorString()));
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());

    f.setFileName(newHeaderFilePath);
    QVERIFY2(f.rename(oldHeaderFilePath), qPrintable(f.errorString()));
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::msvcAsmLinkerFlags()
{
    QDir::setCurrent(testDataDir + "/msvc-asm-flags");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::badInterpreter()
{
    if (!HostOsInfo::isAnyUnixHost())
        QSKIP("only applies on Unix");

    QDir::setCurrent(testDataDir + QLatin1String("/badInterpreter"));
    QCOMPARE(runQbs(), 0);

    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");

    QbsRunParameters params("run");
    params.expectFailure = true;

    const QRegularExpression reNoSuchFileOrDir("bad interpreter:.* No such file or directory");
    const QRegularExpression rePermissionDenied("bad interpreter:.* Permission denied");

    params.arguments = QStringList() << "-p" << "script-interp-missing";
    QCOMPARE(runQbs(params), 1);
    QString strerr = QString::fromLocal8Bit(m_qbsStderr);
    QVERIFY2(strerr.contains(reNoSuchFileOrDir), m_qbsStderr);

    params.arguments = QStringList() << "-p" << "script-interp-noexec";
    QCOMPARE(runQbs(params), 1);
    strerr = QString::fromLocal8Bit(m_qbsStderr);
    QVERIFY2(strerr.contains(reNoSuchFileOrDir) || strerr.contains(rePermissionDenied)
             || strerr.contains("script-noexec: bad interpreter: execve: Exec format error"),
             qPrintable(strerr));

    params.arguments = QStringList() << "-p" << "script-noexec";
    QCOMPARE(runQbs(params), 1);
    QCOMPARE(runQbs(QbsRunParameters("run", QStringList() << "-p" << "script-ok")), 0);
}

void TestBlackbox::bomSources()
{
    QDir::setCurrent(testDataDir + "/bom-sources");
    const bool success = runQbs() == 0;
    if (!success)
        QSKIP("Assuming compiler cannot deal with byte order mark");
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    touch("theheader.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::buildDataOfDisabledProduct()
{
    QDir::setCurrent(testDataDir + QLatin1String("/build-data-of-disabled-product"));
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("compiling test.cpp"), m_qbsStdout.constData());

    // Touch a source file, disable the product, rebuild the project, verify nothing happens.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("test.cpp");
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("products.app.condition:false"))), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("linking"), m_qbsStdout.constData());

    // Enable the product again, rebuild the project, verify that only the changed source file
    // is rebuilt.
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("products.app.condition:true"))), 0);
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling main.cpp"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("compiling test.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::qbsVersion()
{
    const auto v = qbs::LanguageInfo::qbsVersion();
    QDir::setCurrent(testDataDir + QLatin1String("/qbsVersion"));
    QbsRunParameters params;
    params.arguments = QStringList()
            << "project.qbsVersion:" + v.toString()
            << "project.qbsVersionMajor:" + QString::number(v.majorVersion())
            << "project.qbsVersionMinor:" + QString::number(v.minorVersion())
            << "project.qbsVersionPatch:" + QString::number(v.patchLevel());
    QCOMPARE(runQbs(params), 0);

    params.arguments.push_back("project.qbsVersionPatch:" + QString::number(v.patchLevel() + 1));
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
}

void TestBlackbox::transitiveInvalidDependencies()
{
    QDir::setCurrent(testDataDir + "/transitive-invalid-dependencies");
    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("b.present = false"), m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains("c.present = true"), m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains("d.present = false"), m_qbsStdout);
}

void TestBlackbox::transitiveOptionalDependencies()
{
    QDir::setCurrent(testDataDir + "/transitive-optional-dependencies");
    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::groupsInModules()
{
    QDir::setCurrent(testDataDir + "/groups-in-modules");
    QCOMPARE(runQbs({"resolve"}), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling rock.coal to rock.diamond"));
    QVERIFY(m_qbsStdout.contains("compiling chunk.coal to chunk.diamond"));
    QVERIFY(m_qbsStdout.contains("compiling helper2.c"));
    QVERIFY(!m_qbsStdout.contains("compiling helper3.c"));
    QVERIFY(m_qbsStdout.contains("compiling helper4.c"));
    QVERIFY(m_qbsStdout.contains("compiling helper5.c"));
    QVERIFY(!m_qbsStdout.contains("compiling helper6.c"));
    QVERIFY(m_qbsStdout.contains("compiling helper7.c"));
    QVERIFY(m_qbsStdout.contains("compiling helper8.c"));

    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling rock.coal to rock.diamond"));
    QVERIFY(!m_qbsStdout.contains("compiling chunk.coal to chunk.diamond"));

    WAIT_FOR_NEW_TIMESTAMP();
    touch("modules/helper/diamondc.c");

    waitForFileUnlock();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling diamondc.c"));
    QVERIFY(m_qbsStdout.contains("compiling rock.coal to rock.diamond"));
    QVERIFY(m_qbsStdout.contains("compiling chunk.coal to chunk.diamond"));
    QVERIFY(regularFileExists(relativeProductBuildDir("groups-in-modules") + "/rock.diamond"));
    QFile output(relativeProductBuildDir("groups-in-modules") + "/rock.diamond");
    QVERIFY(output.open(QIODevice::ReadOnly));
    QCOMPARE(output.readAll().trimmed(), QByteArray("diamond"));
}

void TestBlackbox::grpc_data()
{
    QTest::addColumn<QString>("projectFile");
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<bool>("hasModules");

    QStringList pkgConfigArgs({"project.qbsModuleProviders:qbspkgconfig"});
    // on macOS, openSSL is hidden from pkg-config by default
    if (qbs::Internal::HostOsInfo::isMacosHost()) {
        pkgConfigArgs
            << "moduleProviders.qbspkgconfig.extraPaths:/usr/local/opt/openssl@1.1/lib/pkgconfig";
    }
    QTest::newRow("cpp-pkgconfig") << QString("grpc_cpp.qbs") << pkgConfigArgs << true;
    QStringList conanArgs(
        {"project.qbsModuleProviders:conan", "moduleProviders.conan.installDirectory:build"});
    QTest::newRow("cpp-conan") << QString("grpc_cpp.qbs") << conanArgs << true;
}

void TestBlackbox::grpc()
{
    QDir::setCurrent(testDataDir + "/grpc");
    QFETCH(QString, projectFile);
    QFETCH(QStringList, arguments);
    QFETCH(bool, hasModules);

    rmDirR(relativeBuildDir());
    if (QTest::currentDataTag() == QLatin1String("cpp-conan")) {
        if (!prepareAndRunConan())
            QSKIP("conan is not prepared, check messages above");
    }

    QbsRunParameters resolveParams("resolve", QStringList{"-f", projectFile});
    resolveParams.arguments << arguments;
    QCOMPARE(runQbs(resolveParams), 0);
    const bool withGrpc = m_qbsStdout.contains("has grpc: true");
    const bool withoutGrpc = m_qbsStdout.contains("has grpc: false");
    QVERIFY2(withGrpc || withoutGrpc, m_qbsStdout.constData());
    if (withoutGrpc)
        QSKIP("grpc module not present");

    const bool hasMods = m_qbsStdout.contains("has modules: true");
    const bool dontHaveMods = m_qbsStdout.contains("has modules: false");
    QVERIFY2(hasMods == !dontHaveMods, m_qbsStdout.constData());
    QCOMPARE(hasMods, hasModules);

    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");

    QbsRunParameters runParams;
    QCOMPARE(runQbs(runParams), 0);
}

void TestBlackbox::hostOsProperties()
{
    QDir::setCurrent(testDataDir + "/host-os-properties");
    QCOMPARE(runQbs(QStringLiteral("resolve")), 0);
    if (m_qbsStdout.contains("target platform/arch differ from host platform/arch"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QCOMPARE(runQbs(QStringLiteral("run")), 0);
    QVERIFY2(m_qbsStdout.contains(
                 ("HOST_ARCHITECTURE = " + HostOsInfo::hostOSArchitecture().toUtf8()).data()),
             m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains(
                ("HOST_PLATFORM = " + HostOsInfo::hostOSIdentifier().toUtf8()).data()),
             m_qbsStdout.constData());
}

void TestBlackbox::ico()
{
    QDir::setCurrent(testDataDir + "/ico");
    QbsRunParameters params;
    params.expectFailure = true;
    params.arguments << "--command-echo-mode" << "command-line";
    const int status = runQbs(params);
    if (status != 0) {
        if (m_qbsStderr.contains("Could not find icotool in any of the following locations:"))
            QSKIP("icotool is not installed");
        if (!m_qbsStderr.isEmpty())
            qDebug("%s", m_qbsStderr.constData());
        if (!m_qbsStdout.isEmpty())
            qDebug("%s", m_qbsStdout.constData());
    }
    QCOMPARE(status, 0);

    QVERIFY(QFileInfo::exists(relativeProductBuildDir("icon") + "/icon.ico"));
    {
        QFile f(relativeProductBuildDir("icon") + "/icon.ico");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto b = f.readAll().toStdString();
        QCOMPARE(b.at(2), '\x1'); // icon
        QCOMPARE(b.at(4), '\x2'); // 2 images
        QVERIFY(b.find("\x89PNG") == std::string::npos);
    }

    QVERIFY(QFileInfo::exists(relativeProductBuildDir("icon-alpha") + "/icon-alpha.ico"));
    {
        QFile f(relativeProductBuildDir("icon-alpha") + "/icon-alpha.ico");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto b = f.readAll().toStdString();
        QCOMPARE(b.at(2), '\x1'); // icon
        QCOMPARE(b.at(4), '\x2'); // 2 images
        QVERIFY(b.find("\x89PNG") == std::string::npos);
        QVERIFY2(m_qbsStdout.contains("--alpha-threshold="), m_qbsStdout.constData());
    }

    QVERIFY(QFileInfo::exists(relativeProductBuildDir("icon-big") + "/icon-big.ico"));
    {
        QFile f(relativeProductBuildDir("icon-big") + "/icon-big.ico");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto b = f.readAll().toStdString();
        QCOMPARE(b.at(2), '\x1'); // icon
        QCOMPARE(b.at(4), '\x5'); // 5 images
        QVERIFY(b.find("\x89PNG") != std::string::npos);
    }

    QVERIFY(QFileInfo::exists(relativeProductBuildDir("cursor") + "/cursor.cur"));
    {
        QFile f(relativeProductBuildDir("cursor") + "/cursor.cur");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto b = f.readAll();
        QVERIFY(b.size() > 0);
        QCOMPARE(b.at(2), '\x2'); // cursor
        QCOMPARE(b.at(4), '\x2'); // 2 images
        QCOMPARE(b.at(10), '\0');
        QCOMPARE(b.at(12), '\0');
        QCOMPARE(b.at(26), '\0');
        QCOMPARE(b.at(28), '\0');
    }

    QVERIFY(QFileInfo::exists(relativeProductBuildDir("cursor-hotspot") + "/cursor-hotspot.cur"));
    {
        QFile f(relativeProductBuildDir("cursor-hotspot") + "/cursor-hotspot.cur");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto b = f.readAll();
        QVERIFY(b.size() > 0);
        QCOMPARE(b.at(2), '\x2'); // cursor
        QCOMPARE(b.at(4), '\x2'); // 2 images
        const bool hasCursorHotspotBug = m_qbsStderr.contains(
                                                              "does not support setting the hotspot for cursor files with multiple images");
        if (hasCursorHotspotBug) {
            QCOMPARE(b.at(10), '\0');
            QCOMPARE(b.at(12), '\0');
            QCOMPARE(b.at(26), '\0');
            QCOMPARE(b.at(28), '\0');
            qWarning() << "this version of icoutil does not support setting the hotspot "
                  "for cursor files with multiple images";
        } else {
            QCOMPARE(b.at(10), '\x8');
            QCOMPARE(b.at(12), '\x9');
            QCOMPARE(b.at(26), '\x10');
            QCOMPARE(b.at(28), '\x11');
        }
    }

    QVERIFY(QFileInfo::exists(relativeProductBuildDir("cursor-hotspot-single")
                              + "/cursor-hotspot-single.cur"));
    {
        QFile f(relativeProductBuildDir("cursor-hotspot-single") + "/cursor-hotspot-single.cur");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto b = f.readAll();
        QVERIFY(b.size() > 0);
        QCOMPARE(b.at(2), '\x2'); // cursor
        QCOMPARE(b.at(4), '\x1'); // 1 image

        // No version check needed because the hotspot can always be set if there's only one image
        QCOMPARE(b.at(10), '\x8');
        QCOMPARE(b.at(12), '\x9');
    }

    QVERIFY(QFileInfo::exists(relativeProductBuildDir("iconset") + "/dmg.ico"));
    {
        QFile f(relativeProductBuildDir("iconset") + "/dmg.ico");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto b = f.readAll();
        QVERIFY(b.size() > 0);
        QCOMPARE(b.at(2), '\x1'); // icon
        QCOMPARE(b.at(4), '\x5'); // 5 images
    }

    QVERIFY(QFileInfo::exists(relativeProductBuildDir("iconset") + "/dmg.cur"));
    {
        QFile f(relativeProductBuildDir("iconset") + "/dmg.cur");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto b = f.readAll();
        QVERIFY(b.size() > 0);
        QCOMPARE(b.at(2), '\x2'); // cursor
        QCOMPARE(b.at(4), '\x5'); // 5 images
        QCOMPARE(b.at(10), '\0');
        QCOMPARE(b.at(12), '\0');
        QCOMPARE(b.at(26), '\0');
        QCOMPARE(b.at(28), '\0');
    }
}

void TestBlackbox::importAssignment()
{
    QDir::setCurrent(testDataDir + "/import-assignment");
    QCOMPARE(runQbs(QStringList("project.qbsSearchPaths:" + QDir::currentPath())), 0);
    QVERIFY2(m_qbsStdout.contains("key 1 = value1") && m_qbsStdout.contains("key 2 = value2"),
             m_qbsStdout.constData());
}

void TestBlackbox::importChangeTracking()
{
    QDir::setCurrent(testDataDir + "/import-change-tracking");
    QCOMPARE(runQbs(QStringList({"-f", "import-change-tracking.qbs"})), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running probe1"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom2 prepare script"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change in imported file that is not used in any rule or command.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("irrelevant.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe1 "), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change in directly imported file only used by one prepare script.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("custom1prepare1.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe1 "), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change in recursively imported file only used by one prepare script.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("custom1prepare2.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe1 "), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change in imported file used only by one command.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("custom1command.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe1 "), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 prepare script"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change in file only used by one prepare script, using directory import.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("custom2prepare/custom2prepare2.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe1 "), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom2 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change in file used only by one command, imported via search path.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("imports/custom2command/custom2command1.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe1 "), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change in directly imported file only used by one Probe
    WAIT_FOR_NEW_TIMESTAMP();
    touch("probe1.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running probe1"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change in indirectly imported file only used by one Probe
    WAIT_FOR_NEW_TIMESTAMP();
    touch("probe2.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running probe1"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 prepare script"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());

    // Change everything at once.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("irrelevant.js");
    touch("custom1prepare1.js");
    touch("custom1prepare2.js");
    touch("custom1command.js");
    touch("custom2prepare/custom2prepare1.js");
    touch("imports/custom2command/custom2command2.js");
    touch("probe2.js");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("Resolving"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running probe1"), m_qbsStdout.constData());
    QVERIFY2(!m_qbsStdout.contains("running probe2"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom1 prepare script"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom2 prepare script"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom1 command"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("running custom2 command"), m_qbsStdout.constData());
}

void TestBlackbox::probesInNestedModules()
{
    QDir::setCurrent(testDataDir + "/probes-in-nested-modules");
    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);

    QCOMPARE(m_qbsStdout.count("running probe a"), 1);
    QCOMPARE(m_qbsStdout.count("running probe b"), 1);
    QCOMPARE(m_qbsStdout.count("running probe c"), 1);
    QCOMPARE(m_qbsStdout.count("running second probe a"), 1);

    QVERIFY(m_qbsStdout.contains("product a, outer.somethingElse = goodbye"));
    QVERIFY(m_qbsStdout.contains("product b, inner.something = hahaha"));
    QVERIFY(m_qbsStdout.contains("product c, inner.something = hello"));

    QVERIFY(m_qbsStdout.contains("product a, inner.something = hahaha"));
    QVERIFY(m_qbsStdout.contains("product a, outer.something = hahaha"));
}

QTEST_MAIN(TestBlackbox)
