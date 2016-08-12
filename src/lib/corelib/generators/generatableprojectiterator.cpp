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

#include "generatableprojectiterator.h"

namespace qbs {

GeneratableProjectIterator::GeneratableProjectIterator(const GeneratableProject &project)
    : project(project)
{
}

void GeneratableProjectIterator::accept(IGeneratableProjectVisitor *visitor)
{
    visitor->visitProject(project);
    QMapIterator<QString, qbs::Project> it(project.projects);
    while (it.hasNext()) {
        it.next();
        visitor->visitProject(it.value(), it.key());
    }

    accept(project, GeneratableProjectData(), project, visitor);
}

void GeneratableProjectIterator::accept(const GeneratableProject &project,
                                        const GeneratableProjectData &parentProjectData,
                                        const GeneratableProjectData &projectData,
                                        IGeneratableProjectVisitor *visitor)
{
    visitor->visitProjectData(project, parentProjectData, projectData);
    visitor->visitProjectData(project, projectData);
    QMapIterator<QString, qbs::ProjectData> it(projectData.data);
    while (it.hasNext()) {
        it.next();
        visitor->visitProjectData(parentProjectData.data.value(it.key()), it.value(), it.key());
        visitor->visitProjectData(it.value(), it.key());
    }

    for (const auto &subProject : projectData.subProjects) {
        accept(project, projectData, subProject, visitor);
    }

    for (const auto &productDataMap : projectData.products) {
        visitor->visitProduct(project, projectData, productDataMap);
        QMapIterator<QString, qbs::ProductData> it(productDataMap.data);
        while (it.hasNext()) {
            it.next();
            visitor->visitProduct(it.value(), it.key());
        }
    }
}

} // namespace qbs
