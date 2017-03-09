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
#include <tools/visualstudioversioninfo.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qtextstream.h>
#include <QtCore/quuid.h>

#include <vector>

namespace qbs {

class VisualStudioSolutionWriterPrivate
{
public:
    QIODevice *device;
    QString baseDir;
};

VisualStudioSolutionWriter::VisualStudioSolutionWriter(QIODevice *device)
    : d(new VisualStudioSolutionWriterPrivate)
{
    d->device = device;
}

VisualStudioSolutionWriter::~VisualStudioSolutionWriter()
{
}

QString VisualStudioSolutionWriter::projectBaseDirectory() const
{
    return d->baseDir;
}

void VisualStudioSolutionWriter::setProjectBaseDirectory(const QString &dir)
{
    d->baseDir = dir;
}

bool VisualStudioSolutionWriter::write(const VisualStudioSolution *solution)
{
    QTextStream out(d->device);
    out << QString(QLatin1String("Microsoft Visual Studio Solution File, "
                                 "Format Version %1\n"
                                 "# Visual Studio %2\n"))
           .arg(solution->versionInfo().solutionVersion())
           .arg(solution->versionInfo().version().majorVersion());

    for (const auto &project : solution->fileProjects()) {
        auto projectFilePath = project->filePath();

        // Try to make the project file path relative to the
        // solution file path if we're writing to a file device
        if (!d->baseDir.isEmpty()) {
            const QDir solutionDir(d->baseDir);
            projectFilePath = Internal::PathUtils::toNativeSeparators(
                        solutionDir.relativeFilePath(projectFilePath),
                        Internal::HostOsInfo::HostOsWindows);
        }

        out << QStringLiteral("Project(\"%1\") = \"%2\", \"%3\", \"%4\"\n")
               .arg(project->projectTypeGuid().toString())
               .arg(QFileInfo(projectFilePath).baseName())
               .arg(projectFilePath)
               .arg(project->guid().toString());

        const auto dependencies = solution->dependencies(project);
        if (!dependencies.isEmpty()) {
            out << "\tProjectSection(ProjectDependencies) = postProject\n";

            for (const auto &dependency : dependencies)
                out << QStringLiteral("\t\t%1 = %1\n").arg(dependency->guid().toString());

            out << "\tEndProjectSection\n";
        }

        out << "EndProject\n";
    }

    for (const auto &project : solution->folderProjects()) {
        out << QStringLiteral("Project(\"%1\") = \"%2\", \"%3\", \"%4\"\n")
               .arg(project->projectTypeGuid().toString())
               .arg(project->name())
               .arg(project->name())
               .arg(project->guid().toString());
        out << QStringLiteral("EndProject\n");
    }

    out << "Global\n";

    for (const auto &globalSection : solution->globalSections()) {
        out << QStringLiteral("\tGlobalSection(%1) = %2\n")
               .arg(globalSection->name())
               .arg(globalSection->isPost()
                    ? QStringLiteral("postSolution")
                    : QStringLiteral("preSolution"));
        for (const auto &property : globalSection->properties())
            out << QStringLiteral("\t\t%1 = %2\n").arg(property.first).arg(property.second);
        out << "\tEndGlobalSection\n";
    }

    out << "EndGlobal\n";

    return out.status() == QTextStream::Ok;
}

} // namespace qbs
