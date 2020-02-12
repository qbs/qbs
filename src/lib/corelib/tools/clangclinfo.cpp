/****************************************************************************
**
** Copyright (C) 2020 Ivan Komissarov (abbapoh@gmail.com)
** Contact: http://www.qt.io/licensing
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

#include "clangclinfo.h"

#include "hostosinfo.h"
#include "msvcinfo.h"
#include "stlutils.h"

namespace qbs {
namespace Internal {

static std::vector<MSVCInstallInfo> compatibleMsvcs(Logger &logger)
{
    auto msvcs = MSVCInstallInfo::installedMSVCs(logger);
    auto filter = [](const MSVCInstallInfo &info)
    {
        const auto versions = info.version.split(QLatin1Char('.'));
        if (versions.empty())
            return true;
        bool ok = false;
        const int major = versions.at(0).toInt(&ok);
        return !(ok && major >= 15); // support MSVC2017 and above
    };
    const auto it = std::remove_if(msvcs.begin(), msvcs.end(), filter);
    msvcs.erase(it, msvcs.end());
    for (const auto &msvc: msvcs) {
        auto vcvarsallPath = msvc.findVcvarsallBat();
        if (vcvarsallPath.isEmpty())
            continue;
    }
    return msvcs;
}

static QString findCompatibleVcsarsallBat(const std::vector<MSVCInstallInfo> &msvcs)
{
    for (const auto &msvc: msvcs) {
        const auto vcvarsallPath = msvc.findVcvarsallBat();
        if (!vcvarsallPath.isEmpty())
            return vcvarsallPath;
    }
    return {};
}

static QString wow6432Key()
{
#ifdef Q_OS_WIN64
    return QStringLiteral("\\Wow6432Node");
#else
    return {};
#endif
}

static QString getToolchainInstallPath(const QFileInfo &compiler)
{
    return compiler.path(); // 1 level up
}

QVariantMap ClangClInfo::toVariantMap() const
{
    return {
        {QStringLiteral("toolchainInstallPath"), toolchainInstallPath},
        {QStringLiteral("vcvarsallPath"), vcvarsallPath},
    };
}

ClangClInfo ClangClInfo::fromCompilerFilePath(const QString &path, Logger &logger)
{
    const auto compilerName = QStringLiteral("clang-cl");
    const auto vcvarsallPath = findCompatibleVcsarsallBat(compatibleMsvcs(logger));
    if (vcvarsallPath.isEmpty()) {
        logger.qbsWarning()
                << Tr::tr("%1 requires installed Visual Studio 2017 or newer, but none was found.")
                   .arg(compilerName);
        return {};
    }

    const auto toolchainInstallPath = getToolchainInstallPath(path);
    return {toolchainInstallPath, vcvarsallPath};
}

std::vector<ClangClInfo> ClangClInfo::installedCompilers(
        const std::vector<QString> &extraPaths, Logger &logger)
{
    std::vector<QString> compilerPaths;
    compilerPaths.reserve(extraPaths.size());
    std::copy_if(extraPaths.begin(), extraPaths.end(),
                 std::back_inserter(compilerPaths),
                 [](const QString &path){ return !path.isEmpty(); });
    const auto compilerName = HostOsInfo::appendExecutableSuffix(QStringLiteral("clang-cl"));

    const QSettings registry(
            QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE%1\\LLVM\\LLVM").arg(wow6432Key()),
            QSettings::NativeFormat);
    const auto key = QStringLiteral(".");
    if (registry.contains(key)) {
        const auto compilerPath = QDir::fromNativeSeparators(registry.value(key).toString())
                + QStringLiteral("/bin/") + compilerName;
        if (QFileInfo(compilerPath).exists())
            compilerPaths.push_back(compilerPath);
    }

    // this branch can be useful in case user had two LLVM installations (e.g. 32bit & 64bit)
    // but uninstalled one - in that case, registry will be empty
    static const char * const envVarCandidates[] = {"ProgramFiles", "ProgramFiles(x86)"};
    for (const auto &envVar : envVarCandidates) {
        const auto value
                = QDir::fromNativeSeparators(QString::fromLocal8Bit(qgetenv(envVar)));
        const auto compilerPath = value + QStringLiteral("/LLVM/bin/") + compilerName;
        if (QFileInfo(compilerPath).exists() && !contains(compilerPaths, compilerPath))
            compilerPaths.push_back(compilerPath);
    }

    const auto msvcs = compatibleMsvcs(logger);
    const auto vcvarsallPath = findCompatibleVcsarsallBat(msvcs);
    if (vcvarsallPath.isEmpty()) {
        logger.qbsWarning()
                << Tr::tr("%1 requires installed Visual Studio 2017 or newer, but none was found.")
                        .arg(compilerName);
        return {};
    }

    std::vector<ClangClInfo> result;
    result.reserve(compilerPaths.size() + msvcs.size());

    for (const auto &path: compilerPaths)
        result.push_back({getToolchainInstallPath(path), vcvarsallPath});

    // If we didn't find custom LLVM installation, try to find if it's installed with Visual Studio
    for (const auto &msvc : msvcs) {
        const auto compilerPath = QStringLiteral("%1/VC/Tools/Llvm/bin/%2")
                .arg(msvc.installDir, compilerName);
        if (QFileInfo(compilerPath).exists()) {
            const auto vcvarsallPath = msvc.findVcvarsallBat();
            if (vcvarsallPath.isEmpty()) {
                logger.qbsWarning()
                        << Tr::tr("Found LLVM in %1, but vcvarsall.bat is missing.")
                                .arg(msvc.installDir);
            }

            result.push_back({getToolchainInstallPath(compilerPath), vcvarsallPath});
        }
    }

    return result;
}

} // namespace Internal
} // namespace qbs
