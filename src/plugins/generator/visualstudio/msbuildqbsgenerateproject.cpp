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

#include "msbuildqbsgenerateproject.h"

#include "msbuild/msbuildimport.h"
#include "msbuild/msbuildproperty.h"
#include "msbuild/msbuildpropertygroup.h"

#include <tools/hostosinfo.h>
#include <tools/shellutils.h>
#include <QtCore/quuid.h>

namespace qbs {

MSBuildQbsGenerateProject::MSBuildQbsGenerateProject(
        const GeneratableProject &project,
        const Internal::VisualStudioVersionInfo &versionInfo,
        VisualStudioGenerator *parent)
    : MSBuildTargetProject(project, versionInfo, parent)
{
    const auto cppDefaultProps = new MSBuildImport(this);
    cppDefaultProps->setProject(QStringLiteral("$(VCTargetsPath)\\Microsoft.Cpp.Default.props"));

    const auto group = new MSBuildPropertyGroup(this);
    group->setLabel(QStringLiteral("Configuration"));
    group->appendProperty(QStringLiteral("PlatformToolset"),
                          versionInfo.platformToolsetVersion());
    group->appendProperty(QStringLiteral("ConfigurationType"),
                          QStringLiteral("Makefile"));
    const auto params = Internal::shellQuote(project.commandLine(),
                                             Internal::HostOsInfo::HostOsWindows);
    group->appendProperty(QStringLiteral("NMakeBuildCommandLine"),
                          QStringLiteral("$(QbsGenerateCommandLine) ") + params);

    const auto cppProps = new MSBuildImport(this);
    cppProps->setProject(QStringLiteral("$(VCTargetsPath)\\Microsoft.Cpp.props"));

    const auto import = new MSBuildImport(this);
    import->setProject(QStringLiteral("$(VCTargetsPath)\\Microsoft.Cpp.targets"));
}

} // namespace qbs
