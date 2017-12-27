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

#include "visualstudiosolution.h"

#include "visualstudiosolutionfileproject.h"
#include "visualstudiosolutionfolderproject.h"

#include <tools/visualstudioversioninfo.h>

#include <QtCore/qmap.h>

namespace qbs {

class VisualStudioSolutionPrivate
{
public:
    VisualStudioSolutionPrivate(const Internal::VisualStudioVersionInfo &versionInfo)
        : versionInfo(versionInfo) { }
    const Internal::VisualStudioVersionInfo versionInfo;
    QList<IVisualStudioSolutionProject *> projects;
    QMap<VisualStudioSolutionFileProject *, QList<VisualStudioSolutionFileProject *>> dependencies;
    QList<VisualStudioSolutionGlobalSection *> globalSections;
};

VisualStudioSolution::VisualStudioSolution(const Internal::VisualStudioVersionInfo &versionInfo,
                                           QObject *parent)
    : QObject(parent)
    , d(new VisualStudioSolutionPrivate(versionInfo))
{
}

VisualStudioSolution::~VisualStudioSolution() = default;

Internal::VisualStudioVersionInfo VisualStudioSolution::versionInfo() const
{
    return d->versionInfo;
}

QList<IVisualStudioSolutionProject *> VisualStudioSolution::projects() const
{
    return d->projects;
}

QList<VisualStudioSolutionFileProject *> VisualStudioSolution::fileProjects() const
{
    QList<VisualStudioSolutionFileProject *> list;
    for (const auto &project : qAsConst(d->projects))
        if (auto fileProject = qobject_cast<VisualStudioSolutionFileProject *>(project))
            list.push_back(fileProject);
    return list;
}

QList<VisualStudioSolutionFolderProject *> VisualStudioSolution::folderProjects() const
{
    QList<VisualStudioSolutionFolderProject *> list;
    for (const auto &project : qAsConst(d->projects))
        if (auto folderProject = qobject_cast<VisualStudioSolutionFolderProject *>(project))
            list.push_back(folderProject);
    return list;
}

void VisualStudioSolution::appendProject(IVisualStudioSolutionProject *project)
{
    d->projects.push_back(project);
}

QList<VisualStudioSolutionFileProject *> VisualStudioSolution::dependencies(
        VisualStudioSolutionFileProject *project) const
{
    return d->dependencies.value(project);
}

void VisualStudioSolution::addDependency(VisualStudioSolutionFileProject *project,
                                         VisualStudioSolutionFileProject *dependency)
{
    d->dependencies[project].push_back(dependency);
}

QList<VisualStudioSolutionGlobalSection *> VisualStudioSolution::globalSections() const
{
    return d->globalSections;
}

void VisualStudioSolution::appendGlobalSection(VisualStudioSolutionGlobalSection *globalSection)
{
    d->globalSections.push_back(globalSection);
}

} // namespace qbs
