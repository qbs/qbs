/****************************************************************************
**
** Copyright (C) 2020 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "tst_blackboxbaremetal.h"

#include "../shared.h"

#include <QtCore/qdir.h>
#include <QtCore/qregularexpression.h>

static bool extractToolset(const QByteArray &output,
                           QByteArray &toolchain, QByteArray &architecture)
{
    const QRegularExpression re("%%([\\w\\-]+)%%, %%(\\w+)%%");
    QRegularExpressionMatchIterator it = re.globalMatch(output);
    if (!it.hasNext())
        return false;
    const QRegularExpressionMatch match = it.next();
    toolchain = match.captured(1).toLocal8Bit();
    architecture = match.captured(2).toLocal8Bit();
    return true;
}

static bool extractCompilerIncludePaths(const QByteArray &output, QStringList &compilerIncludePaths)
{
    const QRegularExpression re("%%([^%%]+)%%");
    QRegularExpressionMatchIterator it = re.globalMatch(output);
    if (!it.hasNext())
        return false;
    const QRegularExpressionMatch match = it.next();
    compilerIncludePaths = match.captured(1).split(",");
    return true;
}

static bool extractQuitedValue(const QByteArray &output, QString &pattern)
{
    const QRegularExpression re("%%(.+)%%");
    const QRegularExpressionMatch match = re.match(output);
    if (!match.hasMatch())
        return false;
    pattern = match.captured(1);
    return true;
}

static QByteArray unsupportedToolsetMessage(const QByteArray &output)
{
    QByteArray toolchain;
    QByteArray architecture;
    extractToolset(output, toolchain, architecture);
    return "Unsupported toolchain '" + toolchain
          + "' for architecture '" + architecture + "'";
}

static QByteArray brokenProbeMessage(const QByteArray &output)
{
    QByteArray toolchain;
    QByteArray architecture;
    extractToolset(output, toolchain, architecture);
    return "Broken probe for toolchain '" + toolchain
          + "' for architecture '" + architecture + "'";
}

TestBlackboxBareMetal::TestBlackboxBareMetal()
    : TestBlackboxBase (SRCDIR "/testdata-baremetal", "blackbox-baremetal")
{
}

void TestBlackboxBareMetal::targetPlatform()
{
    QDir::setCurrent(testDataDir + "/target-platform");
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("-n"))), 0);
    if (m_qbsStdout.contains("unsupported toolset:"))
        QSKIP(unsupportedToolsetMessage(m_qbsStdout));
    const bool hasNoPlatform = m_qbsStdout.contains("has no platform: true");
    QCOMPARE(hasNoPlatform, true);
    const bool hasNoOS = m_qbsStdout.contains("has no os: true");
    QCOMPARE(hasNoOS, true);
}

void TestBlackboxBareMetal::application_data()
{
    QTest::addColumn<QString>("testPath");
    QTest::newRow("one-object-application") << "/one-object-application";
    QTest::newRow("two-object-application") << "/two-object-application";
    QTest::newRow("one-object-asm-application") << "/one-object-asm-application";
}

void TestBlackboxBareMetal::application()
{
    QFETCH(QString, testPath);
    QDir::setCurrent(testDataDir + testPath);
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("-n"))), 0);
    if (m_qbsStdout.contains("unsupported toolset:"))
        QSKIP(unsupportedToolsetMessage(m_qbsStdout));
    QCOMPARE(runQbs(QbsRunParameters("build")), 0);
    if (m_qbsStdout.contains("targetPlatform differs from hostPlatform"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QVERIFY2(m_qbsStdout.contains("Hello from app"), m_qbsStdout.constData());
}

void TestBlackboxBareMetal::staticLibraryDependencies()
{
    QDir::setCurrent(testDataDir + "/static-library-dependencies");
    QCOMPARE(runQbs(QStringList{"-p", "lib-a,lib-b,lib-c,lib-d,lib-e"}), 0);
    QCOMPARE(runQbs(QStringList{"--command-echo-mode", "command-line"}), 0);
    const QByteArray output = m_qbsStdout + '\n' + m_qbsStderr;
    QVERIFY(output.contains("lib-a"));
    QVERIFY(output.contains("lib-b"));
    QVERIFY(output.contains("lib-c"));
    QVERIFY(output.contains("lib-d"));
    QVERIFY(output.contains("lib-e"));
}

void TestBlackboxBareMetal::externalStaticLibraries()
{
    QDir::setCurrent(testDataDir + "/external-static-libraries");
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("-n"))), 0);
    if (m_qbsStdout.contains("unsupported toolset:"))
        QSKIP(unsupportedToolsetMessage(m_qbsStdout));
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxBareMetal::sharedLibraries()
{
    QDir::setCurrent(testDataDir + "/shared-libraries");
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("-n"))), 0);
    if (m_qbsStdout.contains("unsupported toolset:"))
        QSKIP(unsupportedToolsetMessage(m_qbsStdout));
    QCOMPARE(runQbs(QbsRunParameters("build")), 0);
    if (m_qbsStdout.contains("targetPlatform differs from hostPlatform"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QCOMPARE(runQbs(QbsRunParameters("run")), 0);
    QVERIFY2(m_qbsStdout.contains("Hello from app"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("Hello from lib"), m_qbsStdout.constData());
}

void TestBlackboxBareMetal::userIncludePaths()
{
    QDir::setCurrent(testDataDir + "/user-include-paths");
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxBareMetal::systemIncludePaths()
{
    QDir::setCurrent(testDataDir + "/system-include-paths");
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxBareMetal::distributionIncludePaths()
{
    QDir::setCurrent(testDataDir + "/distribution-include-paths");
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxBareMetal::compilerIncludePaths()
{
    QDir::setCurrent(testDataDir + "/compiler-include-paths");
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("-n"))), 0);
    if (!m_qbsStdout.contains("compilerIncludePaths:"))
        QFAIL("No compiler include paths exists");

    QStringList includePaths;
    QVERIFY(extractCompilerIncludePaths(m_qbsStdout, includePaths));
    QVERIFY(includePaths.count() > 0);
    for (const auto &includePath : includePaths) {
        const QDir dir(includePath);
        QVERIFY(dir.exists());
    }
}

void TestBlackboxBareMetal::preincludeHeaders()
{
    QDir::setCurrent(testDataDir + "/preinclude-headers");
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("-n"))), 0);
    if (m_qbsStdout.contains("unsupported toolset:"))
        QSKIP(unsupportedToolsetMessage(m_qbsStdout));
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxBareMetal::defines()
{
    QDir::setCurrent(testDataDir + "/defines");
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxBareMetal::compilerListingFiles_data()
{
    QTest::addColumn<bool>("generateListing");
    QTest::addColumn<QString>("customListingSuffix");
    QTest::newRow("do-not-generate-compiler-listing") << false << "";
    QTest::newRow("generate-default-compiler-listing") << true << "";
    QTest::newRow("generate-custom-compiler-listing") << true << ".lll";
}

void TestBlackboxBareMetal::compilerListingFiles()
{
    QFETCH(bool, generateListing);
    QFETCH(QString, customListingSuffix);
    QDir::setCurrent(testDataDir + "/compiler-listing");

    rmDirR(relativeBuildDir());
    QStringList args = {QStringLiteral("modules.cpp.generateCompilerListingFiles:%1")
                            .arg(generateListing ? "true" : "false")};
    if (!customListingSuffix.isEmpty())
        args << QStringLiteral("modules.cpp.compilerListingSuffix:%1").arg(customListingSuffix);

    QCOMPARE(runQbs(QbsRunParameters("resolve", args)), 0);
    if (m_qbsStdout.contains("unsupported toolset:"))
        QSKIP(unsupportedToolsetMessage(m_qbsStdout));
    if (!m_qbsStdout.contains("compiler listing suffix:"))
        QFAIL("No current compiler listing suffix pattern exists");

    QString compilerListingSuffix;
    if (!extractQuitedValue(m_qbsStdout, compilerListingSuffix))
        QFAIL("Unable to extract current compiler listing suffix");

    if (!customListingSuffix.isEmpty())
        QCOMPARE(compilerListingSuffix, customListingSuffix);

    QCOMPARE(runQbs(QbsRunParameters(args)), 0);
    const QString productBuildDir = relativeProductBuildDir("compiler-listing");
    const QString hash = inputDirHash(".");
    const QString mainListing = productBuildDir + "/" + hash
                                + "/main.c" + compilerListingSuffix;
    QCOMPARE(regularFileExists(mainListing), generateListing);
    const QString funListing = productBuildDir + "/" + hash
                               + "/fun.c" + compilerListingSuffix;
    QCOMPARE(regularFileExists(funListing), generateListing);
}

void TestBlackboxBareMetal::linkerMapFile_data()
{
    QTest::addColumn<bool>("generateMap");
    QTest::addColumn<QString>("customMapSuffix");
    QTest::newRow("do-not-generate-linker-map") << false << "";
    QTest::newRow("generate-default-linker-map") << true << "";
    QTest::newRow("generate-custom-linker-map") << true << ".mmm";
}

void TestBlackboxBareMetal::linkerMapFile()
{
    QFETCH(bool, generateMap);
    QFETCH(QString, customMapSuffix);
    QDir::setCurrent(testDataDir + "/linker-map");

    rmDirR(relativeBuildDir());
    QStringList args = {QStringLiteral("modules.cpp.generateLinkerMapFile:%1")
                            .arg(generateMap ? "true" : "false")};
    if (!customMapSuffix.isEmpty())
        args << QStringLiteral("modules.cpp.linkerMapSuffix:%1").arg(customMapSuffix);

    QCOMPARE(runQbs(QbsRunParameters("resolve", args)), 0);
    if (!m_qbsStdout.contains("linker map suffix:"))
        QFAIL("No current linker map suffix pattern exists");

    QString linkerMapSuffix;
    if (!extractQuitedValue(m_qbsStdout, linkerMapSuffix))
        QFAIL("Unable to extract current linker map suffix");

    if (!customMapSuffix.isEmpty())
        QCOMPARE(linkerMapSuffix, customMapSuffix);

    QCOMPARE(runQbs(QbsRunParameters(args)), 0);
    const QString productBuildDir = relativeProductBuildDir("linker-map");
    const QString linkerMap = productBuildDir + "/linker-map" + linkerMapSuffix;
    QCOMPARE(regularFileExists(linkerMap), generateMap);
}

void TestBlackboxBareMetal::compilerDefinesByLanguage()
{
    QDir::setCurrent(testDataDir + "/compiler-defines-by-language");
    QbsRunParameters params(QStringList{ "-f", "compiler-defines-by-language.qbs" });
    QCOMPARE(runQbs(params), 0);
}

void TestBlackboxBareMetal::toolchainProbe()
{
    QDir::setCurrent(testDataDir + "/toolchain-probe");
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("-n"))), 0);
    if (m_qbsStdout.contains("broken probe:"))
        QFAIL(brokenProbeMessage(m_qbsStdout));
}

QTEST_MAIN(TestBlackboxBareMetal)
