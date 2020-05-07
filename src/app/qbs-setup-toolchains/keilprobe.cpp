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
    return {QStringLiteral("c51"), QStringLiteral("c251"),
            QStringLiteral("c166"), QStringLiteral("armcc"),
            QStringLiteral("armclang")};
}

static QString guessKeilArchitecture(const QFileInfo &compiler)
{
    const auto baseName = compiler.baseName();
    if (baseName == QLatin1String("c51"))
        return QStringLiteral("mcs51");
    if (baseName == QLatin1String("c251"))
        return QStringLiteral("mcs251");
    if (baseName == QLatin1String("c166"))
        return QStringLiteral("c166");
    if (baseName == QLatin1String("armcc"))
        return QStringLiteral("arm");
    if (baseName == QLatin1String("armclang"))
        return QStringLiteral("arm");
    return {};
}

static bool isArmClangCompiler(const QFileInfo &compiler)
{
    return compiler.baseName() == QLatin1String("armclang");
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
            if (architecture == QLatin1String("arm") && isArmClangCompiler(compiler)) {
                profileName = QStringLiteral("keil-llvm-%1-%2").arg(
                            version, architecture);
            } else {
                profileName = QStringLiteral("keil-%1-%2").arg(
                            version, architecture);
            }
        }
    }

    Profile profile(profileName, settings);
    profile.setValue(QStringLiteral("cpp.toolchainInstallPath"), compiler.absolutePath());
    profile.setValue(QStringLiteral("cpp.compilerName"), compiler.fileName());
    profile.setValue(QStringLiteral("qbs.toolchainType"), QStringLiteral("keil"));
    if (!architecture.isEmpty())
        profile.setValue(QStringLiteral("qbs.architecture"), architecture);

    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.").arg(
                     profile.name(), compiler.absoluteFilePath());
    return profile;
}

static Version dumpMcsCompilerVersion(const QFileInfo &compiler)
{
    QTemporaryFile fakeIn;
    if (!fakeIn.open()) {
        qbsWarning() << Tr::tr("Unable to open temporary file %1 for output: %2")
                        .arg(fakeIn.fileName(), fakeIn.errorString());
        return Version{};
    }

    fakeIn.write("#define VALUE_TO_STRING(x) #x\n");
    fakeIn.write("#define VALUE(x) VALUE_TO_STRING(x)\n");

    // Prepare for C51 compiler.
    fakeIn.write("#if defined(__C51__) || defined(__CX51__)\n");
    fakeIn.write("#  define VAR_NAME_VALUE(var) \"(\"\"\"\"|\"#var\"|\"VALUE(var)\"|\"\"\"\")\"\n");
    fakeIn.write("#  if defined (__C51__)\n");
    fakeIn.write("#    pragma message (VAR_NAME_VALUE(__C51__))\n");
    fakeIn.write("#  endif\n");
    fakeIn.write("#  if defined(__CX51__)\n");
    fakeIn.write("#    pragma message (VAR_NAME_VALUE(__CX51__))\n");
    fakeIn.write("#  endif\n");
    fakeIn.write("#endif\n");

    // Prepare for C251 compiler.
    fakeIn.write("#if defined(__C251__)\n");
    fakeIn.write("#  define VAR_NAME_VALUE(var) \"\"|#var|VALUE(var)|\"\"\n");
    fakeIn.write("#  if defined(__C251__)\n");
    fakeIn.write("#    warning (VAR_NAME_VALUE(__C251__))\n");
    fakeIn.write("#  endif\n");
    fakeIn.write("#endif\n");

    fakeIn.close();

    const QStringList args = {fakeIn.fileName()};
    QProcess p;
    p.start(compiler.absoluteFilePath(), args);
    p.waitForFinished(3000);

    const QStringList knownKeys = {QStringLiteral("__C51__"),
                                   QStringLiteral("__CX51__"),
                                   QStringLiteral("__C251__")};

    auto extractVersion = [&knownKeys](const QByteArray &output) {
        QTextStream stream(output);
        QString line;
        while (stream.readLineInto(&line)) {
            if (!line.startsWith(QLatin1String("***")))
                continue;
            enum { KEY_INDEX = 1, VALUE_INDEX = 2, ALL_PARTS = 4 };
            const QStringList parts = line.split(QLatin1String("\"|\""));
            if (parts.count() != ALL_PARTS)
                continue;
            if (!knownKeys.contains(parts.at(KEY_INDEX)))
                continue;
            return parts.at(VALUE_INDEX).toInt();
        }
        return -1;
    };

    const QByteArray dump = p.readAllStandardOutput();
    const int verCode = extractVersion(dump);
    if (verCode < 0) {
        qbsWarning() << Tr::tr("No %1 tokens was found"
                               " in the compiler dump:\n%2")
                        .arg(knownKeys.join(QLatin1Char(',')))
                        .arg(QString::fromUtf8(dump));
        return Version{};
    }
    return Version{verCode / 100, verCode % 100};
}

static Version dumpC166CompilerVersion(const QFileInfo &compiler)
{
    QTemporaryFile fakeIn;
    if (!fakeIn.open()) {
        qbsWarning() << Tr::tr("Unable to open temporary file %1 for output: %2")
                        .arg(fakeIn.fileName(), fakeIn.errorString());
        return Version{};
    }

    fakeIn.write("#if defined(__C166__)\n");
    fakeIn.write("# warning __C166__\n");
    fakeIn.write("# pragma __C166__\n");
    fakeIn.write("#endif\n");

    fakeIn.close();

    const QStringList args = {fakeIn.fileName()};
    QProcess p;
    p.start(compiler.absoluteFilePath(), args);
    p.waitForFinished(3000);

    // Extract the compiler version pattern in the form, like:
    //
    // *** WARNING C320 IN LINE 41 OF c51.c: __C166__
    // *** WARNING C2 IN LINE 42 OF c51.c: '757': unknown #pragma/control, line ignored
    //
    // where the '__C166__' is a key, and the '757' is a value (aka version).
    auto extractVersion = [](const QString &output) {
        const QStringList lines = output.split(QStringLiteral("\r\n"));
        for (auto it = lines.cbegin(); it != lines.cend();) {
            if (it->startsWith(QLatin1String("***")) && it->endsWith(QLatin1String("__C166__"))) {
                ++it;
                if (it == lines.cend() || !it->startsWith(QLatin1String("***")))
                    break;
                const int startIndex = it->indexOf(QLatin1Char('\''));
                if (startIndex == -1)
                    break;
                const int stopIndex = it->indexOf(QLatin1Char('\''), startIndex + 1);
                if (stopIndex == -1)
                    break;
                const QString v = it->mid(startIndex + 1, stopIndex - startIndex - 1);
                return v.toInt();
            }
            ++it;
        }
        return -1;
    };

    const QByteArray dump = p.readAllStandardOutput();
    const int verCode = extractVersion(QString::fromUtf8(dump));
    if (verCode < 0) {
        qbsWarning() << Tr::tr("No __C166__ token was found"
                               " in the compiler dump:\n%1")
                        .arg(QString::fromUtf8(dump));
        return Version{};
    }
    return Version{verCode / 100, verCode % 100};
}

static Version dumpArmCCCompilerVersion(const QFileInfo &compiler)
{
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

static Version dumpArmClangCompilerVersion(const QFileInfo &compiler)
{
    const QStringList args = {QStringLiteral("-dM"), QStringLiteral("-E"),
                              QStringLiteral("-xc"),
                              QStringLiteral("--target=arm-arm-none-eabi"),
                              QStringLiteral("-mcpu=cortex-m0"),
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

static Version dumpKeilCompilerVersion(const QFileInfo &compiler)
{
    const QString arch = guessKeilArchitecture(compiler);
    if (arch == QLatin1String("mcs51") || arch == QLatin1String("mcs251")) {
        return dumpMcsCompilerVersion(compiler);
    } else if (arch == QLatin1String("c166")) {
        return dumpC166CompilerVersion(compiler);
    } else if (arch == QLatin1String("arm")) {
        if (isArmClangCompiler(compiler))
            return dumpArmClangCompilerVersion(compiler);
        return dumpArmCCCompilerVersion(compiler);
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
            QVector<QFileInfo> keilPaths;
            if (productPath.endsWith(QStringLiteral("ARM"))) {
                keilPaths << QFileInfo(productPath + QStringLiteral("/ARMCC/bin/armcc.exe"));
                keilPaths << QFileInfo(productPath + QStringLiteral("/ARMCLANG/bin/armclang.exe"));
            } else if (productPath.endsWith(QStringLiteral("C51"))) {
                keilPaths << QFileInfo(productPath + QStringLiteral("/BIN/c51.exe"));
            } else if (productPath.endsWith(QStringLiteral("C251"))) {
                keilPaths << QFileInfo(productPath + QStringLiteral("/BIN/c251.exe"));
            } else if (productPath.endsWith(QStringLiteral("C166"))) {
                keilPaths << QFileInfo(productPath + QStringLiteral("/BIN/c166.exe"));
            }

            // Fetch the toolchain version.
            const QDir rootPath(registry.value(QStringLiteral("Directory")).toString());
            const QString toolsIniFilePath = rootPath.absoluteFilePath(
                        QStringLiteral("tools.ini"));

            for (const QFileInfo &keilPath : qAsConst(keilPaths)) {
                if (keilPath.exists()) {
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
