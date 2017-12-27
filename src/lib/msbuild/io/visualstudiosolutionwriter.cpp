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

#include "visualstudiosolutionwriter.h"

#include "../solution/visualstudiosolutionfileproject.h"
#include "../solution/visualstudiosolutionfolderproject.h"
#include "../solution/visualstudiosolutionglobalsection.h"
#include "../solution/visualstudiosolution.h"

#include <tools/hostosinfo.h>
#include <tools/pathutils.h>
#include <tools/stlutils.h>
#include <tools/visualstudioversioninfo.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/quuid.h>

#include <vector>

namespace qbs {

using namespace Internal;

class VisualStudioSolutionWriterPrivate
{
public:
    std::ostream *device = nullptr;
    std::string baseDir;
};

VisualStudioSolutionWriter::VisualStudioSolutionWriter(std::ostream *device)
    : d(new VisualStudioSolutionWriterPrivate)
{
    d->device = device;
}

VisualStudioSolutionWriter::~VisualStudioSolutionWriter() = default;

std::string VisualStudioSolutionWriter::projectBaseDirectory() const
{
    return d->baseDir;
}

void VisualStudioSolutionWriter::setProjectBaseDirectory(const std::string &dir)
{
    d->baseDir = dir;
}

bool VisualStudioSolutionWriter::write(const VisualStudioSolution *solution)
{
    auto &out = *d->device;
    out << u8"Microsoft Visual Studio Solution File, Format Version "
        << solution->versionInfo().solutionVersion().toStdString()
        << u8"\n# Visual Studio "
        << solution->versionInfo().version().majorVersion()
        << u8"\n";

    const auto fileProjects = solution->fileProjects();
    for (const auto &project : fileProjects) {
        auto projectFilePath = project->filePath().toStdString();

        // Try to make the project file path relative to the
        // solution file path if we're writing to a file device
        if (!d->baseDir.empty()) {
            const QDir solutionDir(QString::fromStdString(d->baseDir));
            projectFilePath = Internal::PathUtils::toNativeSeparators(
                        solutionDir.relativeFilePath(QString::fromStdString(projectFilePath)),
                        Internal::HostOsInfo::HostOsWindows).toStdString();
        }

        out << u8"Project(\""
            << project->projectTypeGuid().toString().toStdString()
            << u8"\") = \""
            << QFileInfo(QString::fromStdString(projectFilePath)).baseName().toStdString()
            << u8"\", \""
            << projectFilePath
            << u8"\", \""
            << project->guid().toString().toStdString()
            << u8"\"\n";

        const auto dependencies = solution->dependencies(project);
        if (!dependencies.empty()) {
            out << u8"\tProjectSection(ProjectDependencies) = postProject\n";

            for (const auto &dependency : dependencies)
                out << u8"\t\t"
                    << dependency->guid().toString().toStdString()
                    << u8" = "
                    << dependency->guid().toString().toStdString()
                    << u8"\n";

            out << u8"\tEndProjectSection\n";
        }

        out << u8"EndProject\n";
    }

    const auto folderProjects = solution->folderProjects();
    for (const auto &project : folderProjects) {
        out << u8"Project(\""
            << project->projectTypeGuid().toString().toStdString()
            << u8"\") = \""
            << project->name().toStdString()
            << u8"\", \""
            << project->name().toStdString()
            << u8"\", \""
            << project->guid().toString().toStdString()
            << u8"\"\n";

        out << u8"EndProject\n";
    }

    out << u8"Global\n";

    const auto globalSections = solution->globalSections();
    for (const auto &globalSection : globalSections) {
        out << u8"\tGlobalSection("
            << globalSection->name().toStdString()
            << u8") = "
            << (globalSection->isPost() ? u8"postSolution" : u8"preSolution")
            << u8"\n";
        for (const auto &property : globalSection->properties())
            out << u8"\t\t"
                << property.first.toStdString()
                << u8" = "
                << property.second.toStdString()
                << u8"\n";

        out << u8"\tEndGlobalSection\n";
    }

    out << u8"EndGlobal\n";

    return out.good();
}

} // namespace qbs
