/****************************************************************************
**
** Copyright (C) 2021 Denis Shienkov <denis.shienkov@gmail.com>
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

#include "tst_blackboxwindows.h"

#include "../shared.h"

#include <tools/hostosinfo.h>

#include <QtCore/qdir.h>
#include <QtCore/qregularexpression.h>

using qbs::Internal::HostOsInfo;

struct SigntoolInfo
{
    enum class CodeSignResult { Failed = 0, Signed, Unsigned };
    CodeSignResult result = CodeSignResult::Failed;
    bool timestamped = false;
    QString hashAlgorithm;
    QString subjectName;
};

Q_DECLARE_METATYPE(SigntoolInfo::CodeSignResult)

static SigntoolInfo extractSigntoolInfo(const QString &signtoolPath, const QString &appPath)
{
    QProcess signtool;
    signtool.start(signtoolPath, { QStringLiteral("verify"), QStringLiteral("/v"), appPath });
    if (!signtool.waitForStarted() || !signtool.waitForFinished())
        return {};
    const auto output = signtool.readAllStandardError();
    SigntoolInfo signtoolInfo;
    if (output.contains("No signature found")) {
        signtoolInfo.result = SigntoolInfo::CodeSignResult::Unsigned;
    } else {
        signtoolInfo.result = SigntoolInfo::CodeSignResult::Signed;
        const auto output = signtool.readAllStandardOutput();
        const auto lines = output.split('\n');
        for (const auto &line: lines) {
            {
                const QRegularExpression re("^Hash of file \\((.+)\\):.+$");
                const QRegularExpressionMatch match = re.match(line);
                if (match.hasMatch()) {
                    signtoolInfo.hashAlgorithm = match.captured(1).toLocal8Bit();
                    continue;
                }
            }
            {
                const QRegularExpression re("Issued to: (.+)");
                const QRegularExpressionMatch match = re.match(line);
                if (match.hasMatch()) {
                    signtoolInfo.subjectName = match.captured(1).toLocal8Bit().trimmed();
                    continue;
                }
            }
            if (line.startsWith("The signature is timestamped:")) {
                signtoolInfo.timestamped = true;
                break;
            } else if (line.startsWith("File is not timestamped.")) {
                break;
            }
        }
    }
    return signtoolInfo;
}

static QString extractSigntoolPath(const QByteArray &output)
{
    const QRegularExpression re("%%(.+)%%");
    QRegularExpressionMatchIterator it = re.globalMatch(output);
    if (!it.hasNext())
        return {};
    const QRegularExpressionMatch match = it.next();
    return match.captured(1).toUtf8();
}

TestBlackboxWindows::TestBlackboxWindows()
    : TestBlackboxBase (SRCDIR "/testdata-windows", "blackbox-windows")
{
}

void TestBlackboxWindows::initTestCase()
{
    if (!HostOsInfo::isWindowsHost()) {
        QSKIP("only applies on Windows");
        return;
    }

    TestBlackboxBase::initTestCase();
}

void TestBlackboxWindows::standaloneCodesign()
{
    QFETCH(SigntoolInfo::CodeSignResult, result);
    QFETCH(QString, hashAlgorithm);
    QFETCH(QString, subjectName);
    QFETCH(QString, signingTimestamp);

    QDir::setCurrent(testDataDir + "/codesign");
    QbsRunParameters params(QStringList{"qbs.installPrefix:''"});
    params.arguments << QStringLiteral("project.enableSigning:%1").arg(
        (result == SigntoolInfo::CodeSignResult::Signed) ? "true" : "false")
                     << QStringLiteral("project.hashAlgorithm:%1").arg(hashAlgorithm)
                     << QStringLiteral("project.subjectName:%1").arg(subjectName)
                     << QStringLiteral("project.signingTimestamp:%1").arg(signingTimestamp);

    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(params), 0);

    if (!m_qbsStdout.contains("signtool path:"))
        QFAIL("No current signtool path pattern exists");

    const auto signtoolPath = extractSigntoolPath(m_qbsStdout);
    QVERIFY(QFileInfo(signtoolPath).exists());

    const QStringList outputBinaries = {"A.exe", "B.dll"};
    for (const auto &outputBinary : outputBinaries) {
        const auto outputBinaryPath = defaultInstallRoot + "/" + outputBinary;
        QVERIFY(QFileInfo(outputBinaryPath).exists());

        const SigntoolInfo signtoolInfo = extractSigntoolInfo(signtoolPath, outputBinaryPath);
        QVERIFY(signtoolInfo.result != SigntoolInfo::CodeSignResult::Failed);
        QCOMPARE(signtoolInfo.result, result);
        QCOMPARE(signtoolInfo.hashAlgorithm, hashAlgorithm);
        QCOMPARE(signtoolInfo.subjectName, subjectName);
        QCOMPARE(signtoolInfo.timestamped, !signingTimestamp.isEmpty());
    }
}

void TestBlackboxWindows::standaloneCodesign_data()
{
    QTest::addColumn<SigntoolInfo::CodeSignResult>("result");
    QTest::addColumn<QString>("hashAlgorithm");
    QTest::addColumn<QString>("subjectName");
    QTest::addColumn<QString>("signingTimestamp");

    QTest::newRow("standalone, unsigned")
        << SigntoolInfo::CodeSignResult::Unsigned << "" << "" << "";
    QTest::newRow("standalone, signed, sha1, qbs@community.test, no timestamp")
        << SigntoolInfo::CodeSignResult::Signed << "sha1" << "qbs@community.test" << "";
    QTest::newRow("standalone, signed, sha256, qbs@community.test, RFC3061 timestamp")
        << SigntoolInfo::CodeSignResult::Signed << "sha256" << "qbs@community.test"
        << "http://timestamp.digicert.com";
}

QTEST_MAIN(TestBlackboxWindows)
