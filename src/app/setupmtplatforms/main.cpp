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
#include "../shared/specialplatformssetup.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

#include <iostream>
#include <cstdlib>

namespace qbs {

class MtPlatformsSetup : public SpecialPlatformsSetup
{
private:
    QString defaultBaseDirectory() const;
    QString platformTypeName() const { return QLatin1String("MT"); }
    QList<PlatformInfo> gatherPlatformInfo();

    QStringList gatherMtPlatformNames();
    PlatformInfo gatherMtPlatformInfo(const QString &platform);

    QString m_mtSubDir;
};


QString MtPlatformsSetup::defaultBaseDirectory() const
{
    return QLatin1String("YouShouldKnowWhereItIs"); // TODO: Insert real one once it has a "good" path name.
}

QList<SpecialPlatformsSetup::PlatformInfo> MtPlatformsSetup::gatherPlatformInfo()
{
    // TODO: De-obfuscate.
    const QStringList mtSubDirNames
        = QDir(baseDirectory()).entryList(QStringList(QLatin1String("m*")));
    if (mtSubDirNames.count() != 1) {
        throw Exception(tr("Unexpected directory layout in '%1'.")
            .arg(QDir::toNativeSeparators(baseDirectory())));
    }
    m_mtSubDir = baseDirectory() + QLatin1Char('/') + mtSubDirNames.first();

    const QStringList &platformNames = gatherMtPlatformNames();
    if (platformNames.isEmpty()) {
        throw Exception(tr("No MT platforms found at '%1'.")
            .arg(QDir::toNativeSeparators(baseDirectory())));
    }

    QList<PlatformInfo> platforms;
    foreach (const QString platformName, platformNames)
        platforms << gatherMtPlatformInfo(platformName);
    return platforms;
}

QStringList MtPlatformsSetup::gatherMtPlatformNames()
{
    QStringList platformNames;
    QDirIterator dit(m_mtSubDir, QDir::Dirs | QDir::NoDotAndDotDot);
    while (dit.hasNext()) {
        dit.next();
        const QFileInfo &fi = dit.fileInfo();
        if (!fi.isDir() || fi.fileName() == QLatin1String("qttools"))
            continue;
        platformNames << fi.fileName();
    }
    return platformNames;
}

SpecialPlatformsSetup::PlatformInfo MtPlatformsSetup::gatherMtPlatformInfo(const QString &platform)
{
    PlatformInfo platformInfo;
    const QString prefix = QLatin1String("mt-");
    platformInfo.name = platform;
    if (!platformInfo.name.startsWith(prefix))
        platformInfo.name.prepend(prefix);

    const QString platformDir = m_mtSubDir + QLatin1Char('/') + platform;
    platformInfo.toolchainDir = platformDir + QLatin1String("/toolchain/bin");
    QDir tcDir(platformInfo.toolchainDir);
    const QStringList &files = tcDir.entryList(QStringList(QLatin1String("*g++")), QDir::Files);
    if (files.count() != 1)
        throw Exception(tr("Unexpected toolchain directory contents."));
    platformInfo.compilerName = files.first();
    platformInfo.sysrootDir = platformDir + QLatin1String("/sysroot");
    const QString qtToolsDir = platformDir + QLatin1String("/qttools");
    platformInfo.qtBinDir = qtToolsDir + QLatin1String("/bin");
    platformInfo.qtIncDir = platformInfo.sysrootDir + QLatin1String("/usr/include/qt5");
    platformInfo.qtMkspecsDir = qtToolsDir + QLatin1String("/share/qt5/mkspecs");

#ifdef Q_OS_LINUX
    platformInfo.environment.insert(QLatin1String("PATH"), QLatin1String("/opt/mt/bin"));
#endif
    platformInfo.environment.insert(QLatin1String("MT_SYSROOT"), platformInfo.sysrootDir);
    platformInfo.environment.insert(QLatin1String("PKG_CONFIG_SYSROOT_DIR"),
        platformInfo.sysrootDir);
    platformInfo.environment.insert(QLatin1String("PKG_CONFIG_PATH"),
        platformInfo.sysrootDir + QLatin1String("/usr/lib/pkgconfig"));
    platformInfo.environment.insert(QLatin1String("PKG_CONFIG_ALLOW_SYSTEM_LIBS"),
        QLatin1String("1"));
    platformInfo.environment.insert(QLatin1String("PKG_CONFIG_ALLOW_SYSTEM_CFLAGS"),
        QLatin1String("1"));

    return platformInfo;
}

} // namespace qbs


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    qbs::MtPlatformsSetup setup;
    try {
        setup.setup();
    } catch (const qbs::SpecialPlatformsSetup::Exception &ex) {
        std::cerr << qPrintable(setup.tr("Error: %1").arg(ex.errorMessage)) << std::endl;
        return EXIT_FAILURE;
    }

    if (setup.helpRequested())
        std::cout << qPrintable(setup.helpString()) << std::endl;
    return EXIT_SUCCESS;
}
