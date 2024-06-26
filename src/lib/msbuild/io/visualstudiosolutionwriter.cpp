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
    out << "Microsoft Visual Studio Solution File, Format Version "
        << solution->versionInfo().solutionVersion().toStdString()
        << "\n# Visual Studio "
        << solution->versionInfo().version().majorVersion()
        << "\n";

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

        out << "Project(\""
            << project->projectTypeGuid().toString().toStdString()
            << "\") = \""
            << QFileInfo(QString::fromStdString(projectFilePath)).baseName().toStdString()
            << "\", \""
            << projectFilePath
            << "\", \""
            << project->guid().toString().toStdString()
            << "\"\n";

        out << "EndProject\n";
    }

    const auto folderProjects = solution->folderProjects();
    for (const auto &project : folderProjects) {
        out << "Project(\""
            << project->projectTypeGuid().toString().toStdString()
            << "\") = \""
            << project->name().toStdString()
            << "\", \""
            << project->name().toStdString()
            << "\", \""
            << project->guid().toString().toStdString()
            << "\"\n";

        out << "EndProject\n";
    }

    out << "Global\n";

    const auto globalSections = solution->globalSections();
    for (const auto &globalSection : globalSections) {
        out << "\tGlobalSection("
            << globalSection->name().toStdString()
            << ") = "
            << (globalSection->isPost() ? "postSolution" : "preSolution")
            << "\n";
        for (const auto &property : globalSection->properties())
            out << "\t\t"
                << property.first.toStdString()
                << " = "
                << property.second.toStdString()
                << "\n";

        out << "\tEndGlobalSection\n";
    }

    out << "EndGlobal\n";

    return out.good();
}

} // namespace qbs
