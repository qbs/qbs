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
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include <cstdio>
#include <cstdlib>

struct SetupException {};

struct PlatformInfo
{
    QString name;
    QString toolchainDir;
    QString sysrootDir;
    QHash<QString, QString> environment;
};

class SetupMaddePlatforms
{
public:
    SetupMaddePlatforms();
    void setup();

private:
    QString translate(const char *str);
    void printUsage(bool error);
    void checkUsage();
    QStringList gatherMaddeTargetNames();
    QByteArray runProcess(const QString &commandLine, const QProcessEnvironment &env);
    void handleProcessError(const QString &commandLine, const QString &message,
        const QByteArray &output);
    PlatformInfo gatherPlatformInfo(const QString &target);
    void writeConfigFile(const PlatformInfo &platformInfo, const QString &configBaseDir);

    QTextStream m_stdout;
    QTextStream m_stderr;

    QString m_maddeBaseDir;
    QString m_maddeBinDir;
};


SetupMaddePlatforms::SetupMaddePlatforms() : m_stdout(stdout), m_stderr(stderr) { }

void SetupMaddePlatforms::setup()
{
    checkUsage();

    m_maddeBaseDir = qApp->arguments().at(1);
    m_maddeBinDir = m_maddeBaseDir + QLatin1String("/bin");

    m_stdout << translate("Looking for installed MADDE targets...") << endl;
    const QStringList &maddeTargetNames = gatherMaddeTargetNames();
    if (maddeTargetNames.isEmpty()) {
        m_stderr << translate("Error: No targets found.") << endl;
        throw SetupException();
    }

    QList<PlatformInfo> platforms;
    foreach (const QString &targetName, maddeTargetNames)
        platforms << gatherPlatformInfo(targetName);

    const QString qbsPath = QCoreApplication::applicationDirPath() + QLatin1String("/qbs");
    const QString commandLine = qbsPath + QLatin1String(" platforms print-config-base-dir");
    const QString configBaseDir = QString::fromLocal8Bit(runProcess(commandLine,
        QProcessEnvironment::systemEnvironment())).trimmed();

    m_stdout << translate("Writing platform info...") << endl;
    foreach (const PlatformInfo &pi, platforms)
        writeConfigFile(pi, configBaseDir);

    m_stdout << translate("Done.") << endl;
}

QString SetupMaddePlatforms::translate(const char *str)
{
    return QCoreApplication::translate("SetupMaddePlatforms", str);
}

void SetupMaddePlatforms::printUsage(bool error)
{
    if (!error) {
        m_stdout << translate("This tool sets up qbs platform definitions for all installed\n"
            "targets in a given MADDE installation.\n"
            "This allows you to quickly enable qbs to build for Fremantle and Harmattan.")
        << endl;
    }

    QTextStream &stream = error ? m_stderr : m_stdout;
    stream << translate("Usage: ") << QCoreApplication::applicationFilePath() << ' '
        << translate("<MADDE base directory>") << endl;
}

void SetupMaddePlatforms::checkUsage()
{
    if (qApp->arguments().count() != 2) {
        m_stderr << translate("Error: Wrong number of arguments.") << endl;
        printUsage(true);
        throw SetupException();
    }
}

QByteArray SetupMaddePlatforms::runProcess(const QString &commandLine,
    const QProcessEnvironment &env)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.setProcessEnvironment(env);
    proc.start(commandLine);
    if (!proc.waitForStarted())
        handleProcessError(commandLine, proc.errorString(), QByteArray());
    if (proc.state() == QProcess::Running && !proc.waitForFinished(5000)) {
        if (proc.error() == QProcess::UnknownError)
            handleProcessError(commandLine, translate("Process hangs"), QByteArray());
        handleProcessError(commandLine, proc.errorString(), QByteArray());
    }
    const QByteArray output = proc.readAll();
    if (proc.exitStatus() == QProcess::CrashExit)
        handleProcessError(commandLine, proc.errorString(), output);
    if (proc.exitCode() != 0) {
        handleProcessError(commandLine, translate("Unexpected exit code %1").arg(proc.exitCode()),
            output);
    }
    return output;
}

void SetupMaddePlatforms::handleProcessError(const QString &commandLine, const QString &message,
    const QByteArray &output)
{
    m_stderr << translate("Error running command '%1': %2.")
        .arg(QDir::toNativeSeparators(commandLine), message) << endl;
    if (!output.isEmpty()) {
        m_stderr << translate("Output was:") << endl
            << QString::fromLocal8Bit(output) << endl;
    }
    throw SetupException();
}

QStringList SetupMaddePlatforms::gatherMaddeTargetNames()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString madListCommandLine = m_maddeBinDir + QLatin1String("/mad list");
#ifdef Q_OS_WIN
    const QString pathKey = QLatin1String("PATH");
    const QString oldPath = env.value(pathKey);
    QString newPath = m_maddeBinDir;
    if (!oldPath.isEmpty())
        newPath.append(QLatin1Char(';') + oldPath);
    env.insert(pathKey, newPath);
    madListCommandLine.prepend(m_maddeBinDir + QLatin1String("/sh.exe "));
#endif
    QByteArray madListOutput = runProcess(madListCommandLine, env).trimmed();

    QBuffer buf(&madListOutput);
    buf.open(QIODevice::ReadOnly);
    bool inTargetSection = false;
    QStringList targetNames;
    while (buf.canReadLine()) {
        const QByteArray line = buf.readLine().simplified();
        if (!inTargetSection) {
            if (line == "Targets:")
                inTargetSection = true;
            continue;
        }
        if (line.isEmpty())
            break;
        const QList<QByteArray> lineContents = line.split(' ');
        if (lineContents.count() != 2) {
            m_stderr << translate("Error: Unexpected output from command '%1'.")
                .arg(madListCommandLine) << endl;
            throw SetupException();
        }
        if (lineContents.at(1) == "(installed)" || lineContents.at(1) == "(default)")
            targetNames << QString::fromLocal8Bit(lineContents.first());
    }
    return targetNames;
}

PlatformInfo SetupMaddePlatforms::gatherPlatformInfo(const QString &target)
{
    const QString targetDir = m_maddeBaseDir + QLatin1String("/targets/") + target;
    const QString infoFilePath = targetDir + QLatin1String("/information");
    QFile infoFile(infoFilePath);
    if (!infoFile.open(QIODevice::ReadOnly)) {
        m_stderr << translate("Error opening file '%1': %2.")
            .arg(QDir::toNativeSeparators(infoFilePath), infoFile.errorString());
        throw SetupException();
    }
    PlatformInfo platformInfo;
    QByteArray fileContent = infoFile.readAll();
    QBuffer buf(&fileContent); // QFile::canReadLine() always returns false. WTF?
    buf.open(QIODevice::ReadOnly);
    while (buf.canReadLine()) {
        const QList<QByteArray> lineContents = buf.readLine().simplified().split(' ');
        if (lineContents.count() != 2)
            continue;
        if (lineContents.first() != "sysroot")
            continue;
        platformInfo.sysrootDir = m_maddeBaseDir + QLatin1String("/sysroots/")
            + QString::fromLocal8Bit(lineContents.at(1));
        break;
    }
    if (platformInfo.sysrootDir.isEmpty()) {
        m_stderr << translate("Error: No sysroot information found for target '%1'.").arg(target)
            << endl;
        throw SetupException();
    }

    platformInfo.name = target;
    platformInfo.toolchainDir = targetDir + QLatin1String("/bin");
    platformInfo.environment.insert(QLatin1String("SYSROOT_DIR"), platformInfo.sysrootDir);
    const QString maddeMadLibDir = m_maddeBaseDir + QLatin1String("/madlib");
    platformInfo.environment.insert(QLatin1String("PERL5LIB"), maddeMadLibDir);

    // TODO: This should not be necessary; qbs should handle it.
    QChar sep;
#ifdef Q_OS_UNIX
    sep = QLatin1Char(':');
#elif defined(Q_OS_WIN)
    sep = QLatin1Char(';');
#else
#error "Weird platform this is. Not support it we do."
#endif

    const QString maddeMadBinDir = m_maddeBaseDir + QLatin1String("/madbin");
    const QString targetBinDir = targetDir + QLatin1String("/bin");

    // !!! The order matters here !!!
    const QString pathValue = targetBinDir + sep + m_maddeBinDir + sep + maddeMadLibDir
        + sep + maddeMadBinDir;
    platformInfo.environment.insert(QLatin1String("PATH"), pathValue);

    const QString mangleWhiteList = QLatin1String("/usr") + sep + QLatin1String("/lib") + sep
        + QLatin1String("/opt");
    platformInfo.environment.insert(QLatin1String("GCCWRAPPER_PATHMANGLE"), mangleWhiteList);

    return platformInfo;
}

void SetupMaddePlatforms::writeConfigFile(const PlatformInfo &platformInfo,
    const QString &configBaseDir)
{
    const QString configDir = configBaseDir + QLatin1Char('/') + platformInfo.name;

    /*
     * A more correct solution would be to recursively remove and then recreate the directory.
     * However, since it only contains one file, that currently seems like overkill.
     */
    if (!QDir::root().mkpath(configDir)) {
        m_stderr << translate("Error creating directory '%1'.")
            .arg(QDir::toNativeSeparators(configDir)) << endl;
        throw SetupException();
    }
    const QString configFilePath = configDir + QLatin1String("/setup.ini");
    if (QFileInfo(configFilePath).exists()) {
        QFile configFile(configFilePath);
        if (!configFile.remove()) {
            m_stderr << translate("Error removing old config file '%1': %2.")
                .arg(QDir::toNativeSeparators(configFilePath), configFile.errorString());
            throw SetupException();
        }
    }

    QSettings settings(configFilePath, QSettings::IniFormat);

    settings.setValue(QLatin1String("toolchain"), QLatin1String("gcc"));
    settings.setValue(QLatin1String("endianness"), QLatin1String("little-endian"));
    settings.setValue(QLatin1String("targetOS"), QLatin1String("linux"));

    // Can theoretically be Intel-based for MeeGo, but I don't think anyone uses that platform.
    settings.setValue(QLatin1String("architecture"), QLatin1String("arm"));

    settings.beginGroup(QLatin1String("cpp"));
    settings.setValue(QLatin1String("toolchainInstallPath"), platformInfo.toolchainDir);
    settings.setValue(QLatin1String("sysroot"), platformInfo.sysrootDir);
    settings.endGroup();

    settings.beginGroup(QLatin1String("qt/core"));
    settings.setValue(QLatin1String("binPath"), platformInfo.toolchainDir);
    settings.setValue(QLatin1String("libPath"),
        platformInfo.sysrootDir + QLatin1String("/usr/lib"));
    settings.setValue(QLatin1String("incPath"),
        platformInfo.sysrootDir + QLatin1String("/usr/include/qt4"));
    settings.setValue(QLatin1String("mkspecsPath"),
        platformInfo.sysrootDir + QLatin1String("/usr/share/qt4/mkspecs"));
    settings.setValue(QLatin1String("qtNamespace"), QString());
    settings.setValue(QLatin1String("qtLibInfix"), QString());
    settings.endGroup();

    settings.beginGroup(QLatin1String("environment"));
    for (QHash<QString, QString>::ConstIterator it = platformInfo.environment.constBegin();
         it != platformInfo.environment.constEnd(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();

    settings.sync();
    if (settings.status() != QSettings::NoError) {
        m_stderr << translate("Error writing platform config file '%1'.")
            .arg(QDir::toNativeSeparators(configFilePath));
    }

    m_stdout << translate("Successfully set up platform '%1'.").arg(platformInfo.name) << endl;
}


int main(int argc, char *argv[])
{
    try {
        QCoreApplication app(argc, argv);
        SetupMaddePlatforms().setup();
    } catch (const SetupException &) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
