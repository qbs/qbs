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
#include "../shared/logging/consolelogger.h"
#include "../shared/qbssettings.h"
#include "../shared/specialplatformssetup.h"

#include <tools/hostosinfo.h>

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>

#include <iostream>
#include <cstdlib>

namespace qbs {
using Internal::HostOsInfo;

class MaddePlatformsSetup : public SpecialPlatformsSetup
{
public:
    MaddePlatformsSetup(Settings *settings);
private:
    QString defaultBaseDirectory() const;
    QString platformTypeName() const { return QLatin1String("MADDE"); }
    QList<PlatformInfo> gatherPlatformInfo();

    QStringList gatherMaddeTargetNames();
    PlatformInfo gatherMaddePlatformInfo(const QString &targetName);

    QString m_maddeBinDir;
};


MaddePlatformsSetup::MaddePlatformsSetup(Settings *settings) : SpecialPlatformsSetup(settings)
{
}

QString MaddePlatformsSetup::defaultBaseDirectory() const
{
    if (HostOsInfo::isWindowsHost())
        return QLatin1String("C:/QtSDK/Madde");
    return QDir::homePath() + QLatin1String("/QtSDK/Madde");
}

QList<SpecialPlatformsSetup::PlatformInfo> MaddePlatformsSetup::gatherPlatformInfo()
{
    m_maddeBinDir = baseDirectory() + QLatin1String("/bin");

    const QStringList &maddeTargetNames = gatherMaddeTargetNames();
    if (maddeTargetNames.isEmpty())
        throw Exception(tr("Error: No targets found."));

    QList<PlatformInfo> platforms;
    foreach (const QString &targetName, maddeTargetNames)
        platforms << gatherMaddePlatformInfo(targetName);
    return platforms;
}

QStringList MaddePlatformsSetup::gatherMaddeTargetNames()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString madListCommandLine = m_maddeBinDir + QLatin1String("/mad list");
    if (HostOsInfo::isWindowsHost()) {
        const QString pathKey = QLatin1String("PATH");
        const QString oldPath = env.value(pathKey);
        QString newPath = m_maddeBinDir;
        if (!oldPath.isEmpty())
            newPath.append(QLatin1Char(';') + oldPath);
        env.insert(pathKey, newPath);
        madListCommandLine.prepend(m_maddeBinDir + QLatin1String("/sh.exe "));
    }
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
        if (lineContents.count() != 2)
            throw Exception(tr("Unexpected output from command '%1'.").arg(madListCommandLine));
        if (lineContents.at(1) == "(installed)" || lineContents.at(1) == "(default)")
            targetNames << QString::fromLocal8Bit(lineContents.first());
    }
    return targetNames;
}

SpecialPlatformsSetup::PlatformInfo MaddePlatformsSetup::gatherMaddePlatformInfo(const QString &target)
{
    const QString targetDir = baseDirectory() + QLatin1String("/targets/") + target;
    const QString infoFilePath = targetDir + QLatin1String("/information");
    QFile infoFile(infoFilePath);
    if (!infoFile.open(QIODevice::ReadOnly)) {
        throw Exception(tr("Cannot open file '%1': %2.")
            .arg(QDir::toNativeSeparators(infoFilePath), infoFile.errorString()));
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
        platformInfo.sysrootDir = baseDirectory() + QLatin1String("/sysroots/")
            + QString::fromLocal8Bit(lineContents.at(1));
        break;
    }
    if (platformInfo.sysrootDir.isEmpty())
        throw Exception(tr("Error: No sysroot information found for target '%1'.").arg(target));

    platformInfo.name = target;
    platformInfo.targetOS << QLatin1String("unix") << QLatin1String("linux");
    if (target.contains(QLatin1String("harmattan")))
        platformInfo.targetOS << QLatin1String("meego") << QLatin1String("maemo6");
    platformInfo.toolchainDir = targetDir + QLatin1String("/bin");
    platformInfo.compilerName = QLatin1String("g++");
    platformInfo.qtBinDir = platformInfo.toolchainDir;
    platformInfo.qtIncDir = platformInfo.sysrootDir + QLatin1String("/usr/include/qt4");
    platformInfo.qtMkspecPath = platformInfo.sysrootDir
            + QLatin1String("/usr/share/qt4/mkspecs/default");
    platformInfo.environment.insert(QLatin1String("SYSROOT_DIR"), platformInfo.sysrootDir);
    const QString maddeMadLibDir = baseDirectory() + QLatin1String("/madlib");
    platformInfo.environment.insert(QLatin1String("PERL5LIB"), maddeMadLibDir + QLatin1String("/perl5"));

    const QString maddeMadBinDir = baseDirectory() + QLatin1String("/madbin");
    const QString targetBinDir = targetDir + QLatin1String("/bin");

    // !!! The order matters here !!!
    const QChar sep = HostOsInfo::pathListSeparator();
    const QString pathValue = targetBinDir + sep + m_maddeBinDir + sep + maddeMadLibDir + sep
        + maddeMadBinDir;
    platformInfo.environment.insert(QLatin1String("PATH"), pathValue);

    const QString mangleWhiteList = QLatin1String("/usr") + sep + QLatin1String("/lib") + sep
        + QLatin1String("/opt");
    platformInfo.environment.insert(QLatin1String("GCCWRAPPER_PATHMANGLE"), mangleWhiteList);

    return platformInfo;
}

} // namespace qbs


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    SettingsPtr settings = qbsSettings();
    ConsoleLogger::instance(settings.data());
    qbs::MaddePlatformsSetup setup(settings.data());
    try {
        setup.setup();
    } catch (const qbs::SpecialPlatformsSetup::Exception &ex) {
        qbsError() << setup.tr("Error: %1").arg(ex.errorMessage);
        return EXIT_FAILURE;
    }

    if (setup.helpRequested())
        std::cout << qPrintable(setup.helpString()) << std::endl;
    return EXIT_SUCCESS;
}
