/****************************************************************************
**
** Copyright (C) 2023 Danya Patrushev <danyapat@yandex.ru>
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

#include "emscriptenprobe.h"

#include "../shared/logging/consolelogger.h"
#include "probe.h"

#include <logging/translator.h>
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/settings.h>

#include <QDir>
#include <QFileInfo>
#include <QProcess>

using namespace qbs;
using Internal::HostOsInfo;
using Internal::Tr;

namespace {

qbs::Profile writeProfile(
    const QString &profileName, const QFileInfo &compiler, qbs::Settings *settings)
{
    qbs::Profile profile(profileName, settings);
    profile.setValue(QStringLiteral("qbs.architecture"), QStringLiteral("wasm"));
    profile.setValue(QStringLiteral("qbs.toolchainType"), QStringLiteral("emscripten"));
    profile.setValue(QStringLiteral("qbs.targetPlatform"), QStringLiteral("wasm-emscripten"));
    profile.setValue(QStringLiteral("cpp.toolchainInstallPath"), compiler.absolutePath());

    return profile;
}

} //namespace

bool isEmscriptenCompiler(const QString &compilerName)
{
    return compilerName.startsWith(QLatin1String("emcc"))
           || compilerName.startsWith(QLatin1String("em++"));
}

qbs::Profile createEmscriptenProfile(
    const QFileInfo &compiler, qbs::Settings *settings, const QString &profileName)
{
    qbs::Profile profile = writeProfile(profileName, compiler, settings);

    qbsInfo() << Tr::tr("Profile '%1' created for '%2'.")
                     .arg(profile.name(), compiler.absoluteFilePath());
    return profile;
}

void emscriptenProbe(qbs::Settings *settings, std::vector<qbs::Profile> &profiles)
{
    qbsInfo() << Tr::tr("Trying to detect emscripten toolchain...");

    const QString emcc = HostOsInfo::isWindowsHost() ? QStringLiteral("emcc.bat")
                                                     : QStringLiteral("emcc");
    const QString compilerPath = findExecutable(emcc);

    if (compilerPath.isEmpty()) {
        qbsInfo() << Tr::tr("No emscripten toolchain found.");
        return;
    }

    const qbs::Profile profile = createEmscriptenProfile(
        QFileInfo(compilerPath), settings, QLatin1String("wasm"));
    profiles.push_back(profile);
}
