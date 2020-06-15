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

#include "tst_blackboxbase.h"

#include "../shared.h"

#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/installoptions.h>
#include <tools/profile.h>

#include <QtCore/qdiriterator.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qtemporarydir.h>

using qbs::Internal::HostOsInfo;
using qbs::Internal::removeDirectoryWithContents;
using qbs::InstallOptions;
using qbs::Profile;

static QString initQbsExecutableFilePath()
{
    const QString qbsInstallDir = QString::fromLocal8Bit(qgetenv("QBS_INSTALL_DIR"));
    return HostOsInfo::appendExecutableSuffix(QDir::cleanPath(!qbsInstallDir.isEmpty()
        ? qbsInstallDir + QLatin1String("/bin/qbs")
        : QCoreApplication::applicationDirPath() + QLatin1String("/qbs")));
}

static bool supportsBuildDirectoryOption(const QString &command) {
    return !(QStringList() << "help" << "config" << "config-ui"
             << "setup-android" << "setup-qt" << "setup-toolchains" << "create-project")
            .contains(command);
}

static bool supportsSettingsDirOption(const QString &command) {
    return !(QStringList() << "help" << "create-project").contains(command);
}

TestBlackboxBase::TestBlackboxBase(const QString &testDataSrcDir, const QString &testName)
    : testDataDir(testWorkDir(testName)),
      testSourceDir(testDataSourceDir(testDataSrcDir)),
      qbsExecutableFilePath(initQbsExecutableFilePath()),
      defaultInstallRoot(relativeBuildDir() + QLatin1Char('/') + InstallOptions::defaultInstallRoot())
{
    QLocale::setDefault(QLocale::c());
}

int TestBlackboxBase::runQbs(const QbsRunParameters &params)
{
    QStringList args;
    if (!params.command.isEmpty())
        args << params.command;
    if (!params.settingsDir.isEmpty() && supportsSettingsDirOption(params.command))
        args << "--settings-dir" << params.settingsDir;
    if (supportsBuildDirectoryOption(params.command)) {
        args.push_back(QStringLiteral("-d"));
        args.push_back(params.buildDirectory.isEmpty()
                       ? QStringLiteral(".")
                       : params.buildDirectory);
    }
    args << params.arguments;
    const bool commandImpliesResolve = params.command.isEmpty() || params.command == "resolve"
            || params.command == "build" || params.command == "install" || params.command == "run"
            || params.command == "generate";
    if (!params.profile.isEmpty() && commandImpliesResolve) {
        args.push_back(QLatin1String("profile:") + params.profile);
    }
    QProcess process;
    if (!params.workingDir.isEmpty())
        process.setWorkingDirectory(params.workingDir);
    process.setProcessEnvironment(params.environment);
    process.start(qbsExecutableFilePath, args);
    if (!process.waitForStarted() || !process.waitForFinished(testTimeoutInMsecs())
            || process.exitStatus() != QProcess::NormalExit) {
        m_qbsStderr = process.readAllStandardError();
        if (!params.expectCrash) {
            QTest::qFail("qbs did not run correctly", __FILE__, __LINE__);
            qDebug("%s", qPrintable(process.errorString()));
        }
        return -1;
    }

    m_qbsStderr = process.readAllStandardError();
    m_qbsStdout = process.readAllStandardOutput();
    sanitizeOutput(&m_qbsStderr);
    sanitizeOutput(&m_qbsStdout);
    const bool shouldLog = (process.exitStatus() != QProcess::NormalExit
            || process.exitCode() != 0) && !params.expectFailure;
    if (!m_qbsStderr.isEmpty()
            && (shouldLog || qEnvironmentVariableIsSet("QBS_AUTOTEST_ALWAYS_LOG_STDERR")))
        qDebug("%s", m_qbsStderr.constData());
    if (!m_qbsStdout.isEmpty()
            && (shouldLog || qEnvironmentVariableIsSet("QBS_AUTOTEST_ALWAYS_LOG_STDOUT")))
        qDebug("%s", m_qbsStdout.constData());
    return process.exitCode();
}

/*!
  Recursive copy from directory to another.
  Note that this updates the file stamps on Linux but not on Windows.
  */
void TestBlackboxBase::ccp(const QString &sourceDirPath, const QString &targetDirPath)
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
        QFile src(fit.filePath());
        QVERIFY2(src.copy(targetPath), qPrintable(src.errorString()));
    }
}

void TestBlackboxBase::rmDirR(const QString &dir)
{
    QString errorMessage;
    removeDirectoryWithContents(dir, &errorMessage);
}

QByteArray TestBlackboxBase::unifiedLineEndings(const QByteArray &ba)
{
    if (HostOsInfo::isWindowsHost()) {
        QByteArray result;
        result.reserve(ba.size());
        for (const char &c : ba) {
            if (c != '\r')
                result.append(c);
        }
        return result;
    } else {
        return ba;
    }
}

void TestBlackboxBase::sanitizeOutput(QByteArray *ba)
{
    if (HostOsInfo::isWindowsHost())
        ba->replace('\r', "");
}

void TestBlackboxBase::initTestCase()
{
    QVERIFY(regularFileExists(qbsExecutableFilePath));

    const SettingsPtr s = settings();
    if (profileName() != "none" && !s->profiles().contains(profileName()))
        QFAIL(QByteArray("The build profile '" + profileName().toLocal8Bit() +
                         "' could not be found. Please set it up on your machine."));

    validateTestProfile();

    // Initialize the test data directory.
    QVERIFY(testDataDir != testSourceDir);
    rmDirR(testDataDir);
    QDir().mkpath(testDataDir);
    ccp(testSourceDir, testDataDir);
    QDir().mkpath(testDataDir + "/find");
    ccp(testSourceDir + "/../find", testDataDir + "/find");
    QVERIFY(copyDllExportHeader(testSourceDir, testDataDir));
}

void TestBlackboxBase::validateTestProfile()
{
    const SettingsPtr s = settings();
    if (profileName() != "none" && !s->profiles().contains(profileName()))
        QFAIL(QByteArray("The build profile '" + profileName().toLocal8Bit() +
                         "' could not be found. Please set it up on your machine."));
    if (!m_needsQt)
        return;
    const QStringList qmakeFilePaths = Profile(profileName(), s.get())
            .value("moduleProviders.Qt.qmakeFilePaths").toStringList();
    if (!qmakeFilePaths.empty())
        return;
    if (!findExecutable(QStringList{"qmake"}).isEmpty())
        return;
    QSKIP(QByteArray("The build profile '" + profileName().toLocal8Bit() +
                     "' is not a valid Qt profile and Qt was not found "
                     "in the global search paths."));

}

QString TestBlackboxBase::findExecutable(const QStringList &fileNames)
{
    const QStringList path = QString::fromLocal8Bit(qgetenv("PATH"))
            .split(HostOsInfo::pathListSeparator(), QBS_SKIP_EMPTY_PARTS);

    for (const QString &fileName : fileNames) {
        QFileInfo fi(fileName);
        if (fi.isAbsolute())
            return fi.exists() ? fileName : QString();
        for (const QString &ppath : path) {
            const QString fullPath
                    = HostOsInfo::appendExecutableSuffix(ppath + QLatin1Char('/') + fileName);
            if (QFileInfo(fullPath).exists())
                return QDir::cleanPath(fullPath);
        }
    }
    return {};
}

QMap<QString, QString> TestBlackboxBase::findJdkTools(int *status)
{
    QTemporaryDir temp;
    QDir::setCurrent(testDataDir + "/find");
    QbsRunParameters params = QStringList() << "-f" << "find-jdk.qbs";
    params.buildDirectory = temp.path();
    const int res = runQbs(params);
    if (status)
        *status = res;
    QFile file(temp.path() + "/" + relativeProductBuildDir("find-jdk") + "/jdk.json");
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const auto tools = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    return {
        {"java", QDir::fromNativeSeparators(tools["java"].toString())},
        {"javac", QDir::fromNativeSeparators(tools["javac"].toString())},
        {"jar", QDir::fromNativeSeparators(tools["jar"].toString())}
    };
}

qbs::Version TestBlackboxBase::qmakeVersion(const QString &qmakeFilePath)
{
    QStringList arguments;
    arguments << "-query" << "QT_VERSION";
    QProcess qmakeProcess;
    qmakeProcess.start(qmakeFilePath, arguments);
    if (!qmakeProcess.waitForStarted() || !qmakeProcess.waitForFinished()
        || qmakeProcess.exitStatus() != QProcess::NormalExit) {
        qDebug() << "qmake '" << qmakeFilePath << "' could not be run.";
        return qbs::Version();
    }
    QByteArray result = qmakeProcess.readAll().simplified();
    qbs::Version version = qbs::Version::fromString(result);
    if (!version.isValid())
        qDebug() << "qmake '" << qmakeFilePath << "' version is not valid.";
    return version;
}
