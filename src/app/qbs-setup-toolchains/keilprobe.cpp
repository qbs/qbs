/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
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
#include "keilprobe.h"

#include "../shared/logging/consolelogger.h"

#include <logging/translator.h>

#include <tools/hostosinfo.h>
#include <tools/profile.h>

#include <QtCore/qdir.h>
#include <QtCore/qprocess.h>
#include <QtCore/qsettings.h>
#include <QtCore/qtemporaryfile.h>

using namespace qbs;
using Internal::Tr;
using Internal::HostOsInfo;

static QStringList knownKeilCompilerNames()
{
    return {QStringLiteral("c51"), QStringLiteral("armcc")};
}

static QString guessKeilArchitecture(const QFileInfo &compiler)
{
    const auto baseName = compiler.baseName();
    if (baseName == QLatin1String("c51"))
        return QStringLiteral("mcs51");
    if (baseName == QLatin1String("armcc"))
        return QStringLiteral("arm");
    return {};
}

static Profile createKeilProfileHelper(const ToolchainInstallInfo &info,
                                       Settings *settings,
                                       QString profileName = QString())
{
    const QFileInfo compiler = info.compilerPath;
    const QString architecture = guessKeilArchitecture(compiler);

    // In case the profile is auto-detected.
    if (profileName.isEmpty()) {
        if (!info.compilerVersion.isValid()) {
            profileName = QStringLiteral("keil-unknown-%1").arg(architecture);
        } else {
            const QString version = info.compilerVersion.toString(QLatin1Char('_'),
                                                                  QLatin1Char('_'));
            profileName = QStringLiteral("keil-%1-%2").arg(
                        version, architecture);
        }
    }

    Profile profile(profileName, settings);
    profile.setValue(QStringLiteral("cpp.toolchainInstallPath"), compiler.absolutePath());
    profile.setValue(QStringLiteral("qbs.toolchainType"), QStringLiteral("keil"));
    if (!architecture.isEmpty())
        profile.setValue(QStringLiteral("qbs.architecture"), architecture);

    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.").arg(
                     profile.name(), compiler.absoluteFilePath());
    return profile;
}

static Version dumpKeilCompilerVersion(const QFileInfo &compiler)
{
    const QString arch = guessKeilArchitecture(compiler);
    if (arch == QLatin1String("mcs51")) {
        QTemporaryFile fakeIn;
        if (!fakeIn.open()) {
            qbsWarning() << Tr::tr("Unable to open temporary file %1 for output: %2")
                            .arg(fakeIn.fileName(), fakeIn.errorString());
            return Version{};
        }
        fakeIn.write("#define VALUE_TO_STRING(x) #x\n");
        fakeIn.write("#define VALUE(x) VALUE_TO_STRING(x)\n");
        fakeIn.write("#define VAR_NAME_VALUE(var) \"\"\"|\"#var\"|\"VALUE(var)\n");
        fakeIn.write("#ifdef __C51__\n");
        fakeIn.write("#pragma message(VAR_NAME_VALUE(__C51__))\n");
        fakeIn.write("#endif\n");
        fakeIn.write("#ifdef __CX51__\n");
        fakeIn.write("#pragma message(VAR_NAME_VALUE(__CX51__))\n");
        fakeIn.write("#endif\n");
        fakeIn.close();

        const QStringList args = {fakeIn.fileName()};
        QProcess p;
        p.start(compiler.absoluteFilePath(), args);
        p.waitForFinished(3000);
        const auto es = p.exitStatus();
        if (es != QProcess::NormalExit) {
            const QByteArray out = p.readAll();
            qbsWarning() << Tr::tr("Compiler dumping failed:\n%1")
                            .arg(QString::fromUtf8(out));
            return Version{};
        }

        const QByteArray dump = p.readAllStandardOutput();
        const int verCode = extractVersion(dump, "\"__C51__\"|\"");
        if (verCode < 0) {
            qbsWarning() << Tr::tr("No '__C51__' token was found"
                                   " in the compiler dump:\n%1")
                            .arg(QString::fromUtf8(dump));
            return Version{};
        }
        return Version{verCode / 100, verCode % 100};
    } else if (arch == QLatin1String("arm")) {
        const QStringList args = {QStringLiteral("-E"),
                                  QStringLiteral("--list-macros"),
                                  QStringLiteral("nul")};
        QProcess p;
        p.start(compiler.absoluteFilePath(), args);
        p.waitForFinished(3000);
        const auto es = p.exitStatus();
        if (es != QProcess::NormalExit) {
            const QByteArray out = p.readAll();
            qbsWarning() << Tr::tr("Compiler dumping failed:\n%1")
                            .arg(QString::fromUtf8(out));
            return Version{};
        }

        const QByteArray dump = p.readAll();
        const int verCode = extractVersion(dump, "__ARMCC_VERSION ");
        if (verCode < 0) {
            qbsWarning() << Tr::tr("No '__ARMCC_VERSION' token was found "
                                   "in the compiler dump:\n%1")
                            .arg(QString::fromUtf8(dump));
            return Version{};
        }
        return Version{verCode / 1000000, (verCode / 10000) % 100, verCode % 10000};
    }

    return Version{};
}

static std::vector<ToolchainInstallInfo> installedKeilsFromPath()
{
    std::vector<ToolchainInstallInfo> infos;
    const auto compilerNames = knownKeilCompilerNames();
    for (const QString &compilerName : compilerNames) {
        const QFileInfo keilPath(
                    findExecutable(
                        HostOsInfo::appendExecutableSuffix(compilerName)));
        if (!keilPath.exists())
            continue;
        const Version version = dumpKeilCompilerVersion(keilPath);
        infos.push_back({keilPath, version});
    }
    std::sort(infos.begin(), infos.end());
    return infos;
}

// Parse the 'tools.ini' file to fetch a toolchain version.
// Note: We can't use QSettings here!
static QString extractVersion(const QString &toolsIniFile, const QString &section)
{
    QFile f(toolsIniFile);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QTextStream in(&f);
    enum State { Enter, Lookup, Exit } state = Enter;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        // Search for section.
        const int firstBracket = line.indexOf(QLatin1Char('['));
        const int lastBracket = line.lastIndexOf(QLatin1Char(']'));
        const bool hasSection = (firstBracket == 0 && lastBracket != -1
                && (lastBracket + 1) == line.size());
        switch (state) {
        case Enter: {
            if (hasSection) {
                const auto content = line.midRef(firstBracket + 1,
                                                 lastBracket - firstBracket - 1);
                if (content == section)
                    state = Lookup;
            }
        }
            break;
        case Lookup: {
            if (hasSection)
                return {}; // Next section found.
            const int versionIndex = line.indexOf(QLatin1String("VERSION="));
            if (versionIndex < 0)
                continue;
            QString version = line.mid(8);
            if (version.startsWith(QLatin1Char('V')))
                version.remove(0, 1);
            return version;
        }
            break;
        default:
            return {};
        }
    }
    return {};
}

static std::vector<ToolchainInstallInfo> installedKeilsFromRegistry()
{
    std::vector<ToolchainInstallInfo> infos;

    if (HostOsInfo::isWindowsHost()) {

#ifdef Q_OS_WIN64
        static const char kRegistryNode[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\" \
                                            "Windows\\CurrentVersion\\Uninstall\\Keil \u00B5Vision4";
#else
        static const char kRegistryNode[] = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\" \
                                            "Windows\\CurrentVersion\\Uninstall\\Keil \u00B5Vision4";
#endif

        QSettings registry(QLatin1String(kRegistryNode), QSettings::NativeFormat);
        const auto productGroups = registry.childGroups();
        for (const QString &productKey : productGroups) {
            if (!productKey.startsWith(QStringLiteral("App")))
                continue;
            registry.beginGroup(productKey);
            const QString productPath = registry.value(QStringLiteral("ProductDir"))
                    .toString();
            // Fetch the toolchain executable path.
            QFileInfo keilPath;
            if (productPath.endsWith(QStringLiteral("ARM")))
                keilPath.setFile(productPath + QStringLiteral("\\ARMCC\\bin\\armcc.exe"));
            else if (productPath.endsWith(QStringLiteral("C51")))
                keilPath.setFile(productPath + QStringLiteral("\\BIN\\c51.exe"));

            if (keilPath.exists()) {
                // Fetch the toolchain version.
                const QDir rootPath(registry.value(QStringLiteral("Directory")).toString());
                const QString toolsIniFilePath = rootPath.absoluteFilePath(
                            QStringLiteral("tools.ini"));
                for (auto index = 1; index <= 2; ++index) {
                    const QString section = registry.value(
                                QStringLiteral("Section %1").arg(index)).toString();
                    const QString version = extractVersion(toolsIniFilePath, section);
                    if (!version.isEmpty()) {
                        infos.push_back({keilPath, Version::fromString(version)});
                        break;
                    }
                }
            }
            registry.endGroup();
        }
    }

    std::sort(infos.begin(), infos.end());
    return infos;
}

bool isKeilCompiler(const QString &compilerName)
{
    return Internal::any_of(knownKeilCompilerNames(), [compilerName](
                            const QString &knownName) {
        return compilerName.contains(knownName);
    });
}

void createKeilProfile(const QFileInfo &compiler, Settings *settings,
                       QString profileName)
{
    const ToolchainInstallInfo info = {compiler, Version{}};
    createKeilProfileHelper(info, settings, std::move(profileName));
}

void keilProbe(Settings *settings, std::vector<Profile> &profiles)
{
    qbsInfo() << Tr::tr("Trying to detect KEIL toolchains...");

    // Make sure that a returned infos are sorted before using the std::set_union!
    const std::vector<ToolchainInstallInfo> regInfos = installedKeilsFromRegistry();
    const std::vector<ToolchainInstallInfo> pathInfos = installedKeilsFromPath();
    std::vector<ToolchainInstallInfo> allInfos;
    allInfos.reserve(regInfos.size() + pathInfos.size());
    std::set_union(regInfos.cbegin(), regInfos.cend(),
                   pathInfos.cbegin(), pathInfos.cend(),
                   std::back_inserter(allInfos));

    for (const ToolchainInstallInfo &info : allInfos) {
        const auto profile = createKeilProfileHelper(info, settings);
        profiles.push_back(profile);
    }

    if (allInfos.empty())
        qbsInfo() << Tr::tr("No KEIL toolchains found.");
}
