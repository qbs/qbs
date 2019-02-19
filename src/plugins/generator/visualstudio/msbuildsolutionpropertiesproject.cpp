/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "msbuildsolutionpropertiesproject.h"

#include "msbuild/msbuildpropertygroup.h"

#include <tools/pathutils.h>

#include <QtCore/qfileinfo.h>

namespace qbs {

MSBuildSolutionPropertiesProject::MSBuildSolutionPropertiesProject(
        const Internal::VisualStudioVersionInfo &versionInfo,
        const GeneratableProject &project,
        const QFileInfo &qbsExecutable,
        const QString &qbsSettingsDir)
{
    setDefaultTargets(QStringLiteral("Build"));
    setToolsVersion(versionInfo.toolsVersion());

    const auto group = new MSBuildPropertyGroup(this);
    group->setLabel(QStringLiteral("UserMacros"));

    static const auto win = Internal::HostOsInfo::HostOsWindows;

    group->appendProperty(QStringLiteral("QbsExecutableDir"),
                          Internal::PathUtils::toNativeSeparators(qbsExecutable.path(), win)
                          + Internal::HostOsInfo::pathSeparator(win));
    group->appendProperty(QStringLiteral("QbsProjectDir"),
                          Internal::PathUtils::toNativeSeparators(project.filePath().path(), win)
                          + Internal::HostOsInfo::pathSeparator(win));

    if (!qbsSettingsDir.isEmpty()) {
        group->appendProperty(QStringLiteral("QbsSettingsDir"),
                              Internal::PathUtils::toNativeSeparators(qbsSettingsDir, win)
                              + Internal::HostOsInfo::pathSeparator(win) + QLatin1Char('.'));
    }
}

} // namespace qbs
