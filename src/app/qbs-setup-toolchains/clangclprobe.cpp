/****************************************************************************
**
** Copyright (C) 2019 Ivan Komissarov (abbapoh@gmail.com)
** Contact: http://www.qt.io/licensing
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

#include "clangclprobe.h"
#include "msvcprobe.h"
#include "probe.h"

#include "../shared/logging/consolelogger.h"

#include <logging/translator.h>
#include <tools/clangclinfo.h>
#include <tools/hostosinfo.h>
#include <tools/msvcinfo.h>
#include <tools/profile.h>
#include <tools/qttools.h>
#include <tools/settings.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>

using qbs::Settings;
using qbs::Profile;
using qbs::Internal::ClangClInfo;
using qbs::Internal::HostOsInfo;

using qbs::Internal::Tr;

namespace {

Profile createProfileHelper(
        Settings *settings,
        const QString &profileName,
        const QString &toolchainInstallPath,
        const QString &vcvarsallPath,
        const QString &architecture)
{
    Profile profile(profileName, settings);
    profile.removeProfile();
    profile.setValue(QStringLiteral("qbs.architecture"), architecture);
    profile.setValue(QStringLiteral("qbs.toolchainType"), QStringLiteral("clang-cl"));
    profile.setValue(QStringLiteral("cpp.toolchainInstallPath"), toolchainInstallPath);
    profile.setValue(QStringLiteral("cpp.vcvarsallPath"), vcvarsallPath);
    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.")
            .arg(profile.name(), QDir::toNativeSeparators(toolchainInstallPath));
    return profile;
}

QString findClangCl()
{
    const auto compilerName = HostOsInfo::appendExecutableSuffix(QStringLiteral("clang-cl"));
    const auto compilerFromPath = findExecutable(compilerName);
    if (!compilerFromPath.isEmpty())
        return compilerFromPath;

    return {};
}

} // namespace

void createClangClProfile(const QFileInfo &compiler, Settings *settings,
                          const QString &profileName)
{
    const auto clangCl = ClangClInfo::fromCompilerFilePath(
            compiler.filePath(), ConsoleLogger::instance());
    if (clangCl.isEmpty())
        return;
    const auto hostArch = QString::fromStdString(HostOsInfo::hostOSArchitecture());
    createProfileHelper(
            settings, profileName, clangCl.toolchainInstallPath, clangCl.vcvarsallPath, hostArch);
}

/*!
  \brief Creates a clang-cl profile based on auto-detected vsversion.
  \internal
*/
void clangClProbe(Settings *settings, std::vector<Profile> &profiles)
{
    const auto compilerName = QStringLiteral("clang-cl");
    qbsInfo() << Tr::tr("Trying to detect %1...").arg(compilerName);
    const auto clangCls = ClangClInfo::installedCompilers(
            {findClangCl()}, ConsoleLogger::instance());
    if (clangCls.empty()) {
        qbsInfo() << Tr::tr("%1 was not found.").arg(compilerName);
        return;
    }

    const auto clangCl = clangCls.front();
    const QString architectures[] = {
        QStringLiteral("x86_64"),
        QStringLiteral("x86")
    };
    for (const auto &arch: architectures) {
        const auto profileName = QStringLiteral("clang-cl-%1").arg(arch);
        auto profile = createProfileHelper(
                settings, profileName, clangCl.toolchainInstallPath, clangCl.vcvarsallPath, arch);
        profiles.push_back(std::move(profile));
    }
}
