/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "probe.h"

#include "clangclprobe.h"
#include "gccprobe.h"
#include "iarewprobe.h"
#include "keilprobe.h"
#include "msvcprobe.h"
#include "sdccprobe.h"
#include "xcodeprobe.h"

#include <logging/translator.h>
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/settings.h>
#include <tools/toolchains.h>

#include <QtCore/qdir.h>
#include <QtCore/qtextstream.h>

using namespace qbs;
using Internal::HostOsInfo;
using Internal::Tr;

static QTextStream qStdout(stdout);
static QTextStream qStderr(stderr);

QString findExecutable(const QString &fileName)
{
    QString fullFileName = fileName;
    if (HostOsInfo::isWindowsHost()
            && !fileName.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
        fullFileName += QLatin1String(".exe");
    }
    const QString path = QString::fromLocal8Bit(qgetenv("PATH"));
    const auto ppaths = path.split(HostOsInfo::pathListSeparator());
    for (const QString &ppath : ppaths) {
        const QString fullPath = ppath + QLatin1Char('/') + fullFileName;
        if (QFileInfo::exists(fullPath))
            return QDir::cleanPath(fullPath);
    }
    return {};
}

QStringList toolchainTypeFromCompilerName(const QString &compilerName)
{
    if (compilerName == QLatin1String("cl.exe"))
        return canonicalToolchain(QStringLiteral("msvc"));
    if (compilerName == QLatin1String("clang-cl.exe"))
        return canonicalToolchain(QLatin1String("clang-cl"));
    const auto types = { QStringLiteral("clang"), QStringLiteral("llvm"),
                         QStringLiteral("mingw"), QStringLiteral("gcc") };
    for (const auto &type : types) {
        if (compilerName.contains(type))
            return canonicalToolchain(type);
    }
    if (compilerName == QLatin1String("g++"))
        return canonicalToolchain(QStringLiteral("gcc"));
    if (isIarCompiler(compilerName))
        return canonicalToolchain(QStringLiteral("iar"));
    if (isKeilCompiler(compilerName))
        return canonicalToolchain(QStringLiteral("keil"));
    if (isSdccCompiler(compilerName))
        return canonicalToolchain(QStringLiteral("sdcc"));
    return {};
}

void probe(Settings *settings)
{
    QList<Profile> profiles;
    if (HostOsInfo::isWindowsHost()) {
        msvcProbe(settings, profiles);
        clangClProbe(settings, profiles);
    } else {
        gccProbe(settings, profiles, QStringLiteral("gcc"));
        gccProbe(settings, profiles, QStringLiteral("clang"));

        if (HostOsInfo::isMacosHost()) {
            xcodeProbe(settings, profiles);
        }
    }

    mingwProbe(settings, profiles);
    iarProbe(settings, profiles);
    keilProbe(settings, profiles);
    sdccProbe(settings, profiles);

    if (profiles.empty()) {
        qStderr << Tr::tr("Could not detect any toolchains. No profile created.") << endl;
    } else if (profiles.size() == 1 && settings->defaultProfile().isEmpty()) {
        const QString profileName = profiles.front().name();
        qStdout << Tr::tr("Making profile '%1' the default.").arg(profileName) << endl;
        settings->setValue(QStringLiteral("defaultProfile"), profileName);
    }
}

void createProfile(const QString &profileName, const QString &toolchainType,
                   const QString &compilerFilePath, Settings *settings)
{
    QFileInfo compiler(compilerFilePath);
    if (compilerFilePath == compiler.fileName() && !compiler.exists())
        compiler = QFileInfo(findExecutable(compilerFilePath));

    if (!compiler.exists()) {
        throw qbs::ErrorInfo(Tr::tr("Compiler '%1' not found")
                             .arg(compilerFilePath));
    }

    QStringList toolchainTypes;
    if (toolchainType.isEmpty())
        toolchainTypes = toolchainTypeFromCompilerName(compiler.fileName());
    else
        toolchainTypes = canonicalToolchain(toolchainType);

    if (toolchainTypes.contains(QLatin1String("msvc")))
        createMsvcProfile(compiler, settings, profileName);
    else if (toolchainTypes.contains(QLatin1String("clang-cl")))
        createClangClProfile(compiler, settings, profileName);
    else if (toolchainTypes.contains(QLatin1String("gcc")))
        createGccProfile(compiler, settings, toolchainTypes, profileName);
    else if (toolchainTypes.contains(QLatin1String("iar")))
        createIarProfile(compiler, settings, profileName);
    else if (toolchainTypes.contains(QLatin1String("keil")))
        createKeilProfile(compiler, settings, profileName);
    else if (toolchainTypes.contains(QLatin1String("sdcc")))
        createSdccProfile(compiler, settings, profileName);
    else
        throw qbs::ErrorInfo(Tr::tr("Cannot create profile: Unknown toolchain type."));
}

int extractVersion(const QByteArray &macroDump, const QByteArray &keyToken)
{
    const int startIndex = macroDump.indexOf(keyToken);
    if (startIndex == -1)
        return -1;
    const int endIndex = macroDump.indexOf('\n', startIndex);
    if (endIndex == -1)
        return -1;
    const auto keyLength = keyToken.length();
    const int version = macroDump.mid(startIndex + keyLength,
                                      endIndex - startIndex - keyLength)
            .toInt();
    return version;
}
