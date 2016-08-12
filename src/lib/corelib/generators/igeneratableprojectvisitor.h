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

#ifndef IGENERATABLEPROJECTVISITOR_H
#define IGENERATABLEPROJECTVISITOR_H

#include "generatordata.h"

namespace qbs {

class IGeneratableProjectVisitor {
public:
    // Collapsed configurations
    virtual void visitProject(const GeneratableProject &project) {
        Q_UNUSED(project);
    }

    virtual void visitProjectData(const GeneratableProject &project,
                                  const GeneratableProjectData &parentProjectData,
                                  const GeneratableProjectData &projectData) {
        Q_UNUSED(project);
        Q_UNUSED(parentProjectData);
        Q_UNUSED(projectData);
    }

    virtual void visitProjectData(const GeneratableProject &project,
                                  const GeneratableProjectData &projectData) {
        Q_UNUSED(project);
        Q_UNUSED(projectData);
    }

    virtual void visitProduct(const GeneratableProject &project,
                              const GeneratableProjectData &projectData,
                              const GeneratableProductData &productData) {
        Q_UNUSED(project);
        Q_UNUSED(projectData);
        Q_UNUSED(productData);
    }

    // Expanded configurations
    virtual void visitProject(const qbs::Project &project,
                              const QString &configuration) {
        Q_UNUSED(project);
        Q_UNUSED(configuration);
    }

    virtual void visitProjectData(const qbs::ProjectData &parentProjectData,
                                  const qbs::ProjectData &projectData,
                                  const QString &configuration) {
        Q_UNUSED(parentProjectData);
        Q_UNUSED(projectData);
        Q_UNUSED(configuration);
    }

    virtual void visitProjectData(const qbs::ProjectData &projectData,
                                  const QString &configuration) {
        Q_UNUSED(projectData);
        Q_UNUSED(configuration);
    }

    virtual void visitProduct(const qbs::ProductData &productData,
                              const QString &configuration) {
        Q_UNUSED(productData);
        Q_UNUSED(configuration);
    }
};

} // namespace qbs

#endif // IGENERATABLEPROJECTVISITOR_H
