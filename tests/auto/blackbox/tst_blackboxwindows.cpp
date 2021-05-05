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
#include <tools/profile.h>
#include <tools/qttools.h>

#include <QtCore/qdir.h>
#include <QtCore/qregularexpression.h>

using qbs::Internal::HostOsInfo;
using qbs::Profile;

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

void TestBlackboxWindows::innoSetup()
{
    const SettingsPtr s = settings();
    Profile profile(profileName(), s.get());

    QDir::setCurrent(testDataDir + "/innosetup");

    QCOMPARE(runQbs({"resolve"}), 0);
    const bool withInnosetup = m_qbsStdout.contains("has innosetup: true");
    const bool withoutInnosetup = m_qbsStdout.contains("has innosetup: false");
    QVERIFY2(withInnosetup || withoutInnosetup, m_qbsStdout.constData());
    if (withoutInnosetup)
        QSKIP("innosetup module not present");

    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling test.iss"));
    QVERIFY(m_qbsStdout.contains("compiling Example1.iss"));
    QVERIFY(regularFileExists(relativeProductBuildDir("QbsSetup") + "/qbs.setup.test.exe"));
    QVERIFY(regularFileExists(relativeProductBuildDir("Example1") + "/Example1.exe"));
}

void TestBlackboxWindows::innoSetupDependencies()
{
    const SettingsPtr s = settings();
    Profile profile(profileName(), s.get());

    QDir::setCurrent(testDataDir + "/innosetupDependencies");

    QCOMPARE(runQbs({"resolve"}), 0);
    const bool withInnosetup = m_qbsStdout.contains("has innosetup: true");
    const bool withoutInnosetup = m_qbsStdout.contains("has innosetup: false");
    QVERIFY2(withInnosetup || withoutInnosetup, m_qbsStdout.constData());
    if (withoutInnosetup)
        QSKIP("innosetup module not present");

    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling test.iss"));
    QVERIFY(regularFileExists(relativeBuildDir() + "/qbs.setup.test.exe"));
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
        QSKIP("No current signtool path pattern exists");

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


static bool haveWiX(const Profile &profile)
{
    if (profile.value("wix.toolchainInstallPath").isValid() &&
            profile.value("wix.toolchainInstallRoot").isValid()) {
        return true;
    }

    QStringList regKeys;
    regKeys << QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows Installer XML\\")
            << QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Installer XML\\");

    QStringList paths = QProcessEnvironment::systemEnvironment().value("PATH")
            .split(HostOsInfo::pathListSeparator(), QBS_SKIP_EMPTY_PARTS);

    for (const QString &key : qAsConst(regKeys)) {
        const QStringList versions = QSettings(key, QSettings::NativeFormat).childGroups();
        for (const QString &version : versions) {
            QSettings settings(key + version, QSettings::NativeFormat);
            QString str = settings.value(QStringLiteral("InstallRoot")).toString();
            if (!str.isEmpty())
                paths.prepend(str);
        }
    }

    for (const QString &path : qAsConst(paths)) {
        if (regularFileExists(QDir::fromNativeSeparators(path) +
                          HostOsInfo::appendExecutableSuffix(QStringLiteral("/candle"))) &&
            regularFileExists(QDir::fromNativeSeparators(path) +
                          HostOsInfo::appendExecutableSuffix(QStringLiteral("/light")))) {
            return true;
        }
    }

    return false;
}

void TestBlackboxWindows::wix()
{
    const SettingsPtr s = settings();
    Profile profile(profileName(), s.get());

    if (!haveWiX(profile)) {
        QSKIP("WiX is not installed");
        return;
    }

    QByteArray arch = profile.value("qbs.architecture").toString().toLatin1();
    if (arch.isEmpty())
        arch = QByteArrayLiteral("x86");

    QDir::setCurrent(testDataDir + "/wix");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling QbsSetup.wxs"), m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains("linking qbs.msi"), m_qbsStdout);
    QVERIFY(regularFileExists(relativeProductBuildDir("QbsSetup") + "/qbs.msi"));

    if (HostOsInfo::isWindowsHost()) {
        QVERIFY2(m_qbsStdout.contains("compiling QbsBootstrapper.wxs"), m_qbsStdout);
        QVERIFY2(m_qbsStdout.contains("linking qbs-setup-" + arch + ".exe"), m_qbsStdout);
        QVERIFY(regularFileExists(relativeProductBuildDir("QbsBootstrapper")
                                  + "/qbs-setup-" + arch + ".exe"));
    }
}

void TestBlackboxWindows::wixDependencies()
{
    const SettingsPtr s = settings();
    Profile profile(profileName(), s.get());

    if (!haveWiX(profile)) {
        QSKIP("WiX is not installed");
        return;
    }

    QByteArray arch = profile.value("qbs.architecture").toString().toLatin1();
    if (arch.isEmpty())
        arch = QByteArrayLiteral("x86");

    QDir::setCurrent(testDataDir + "/wixDependencies");
    QbsRunParameters params;
    if (!HostOsInfo::isWindowsHost())
        params.arguments << "qbs.targetOS:windows";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("compiling QbsSetup.wxs"), m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains("linking qbs.msi"), m_qbsStdout);
    QVERIFY(regularFileExists(relativeBuildDir() + "/qbs.msi"));
}

QTEST_MAIN(TestBlackboxWindows)
