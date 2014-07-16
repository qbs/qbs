/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2015 Jake Petroules.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#ifndef GENERATORPLUGIN_H
#define GENERATORPLUGIN_H

#include <api/project.h>
#include <QList>
#include <QString>

namespace qbs {

/*!
 * \class ProjectGenerator
 * \brief The \c ProjectGenerator class is an abstract base class for generators which generate
 * arbitrary output given a resolved Qbs project.
 */

class ProjectGenerator
{
public:
    virtual ~ProjectGenerator()
    {
    }

    /*!
     * Returns the name of the generator used to create the external build system files.
     */
    virtual QString generatorName() const = 0;

    virtual void generate(const InstallOptions &installOptions) = 0;

    QList<Project> projects() const
    {
        return m_projects;
    }

    void addProject(const Project &project)
    {
        m_projects << project;
    }

    void addProjects(const QList<Project> &projects)
    {
        m_projects << projects;
    }

    void removeProject(const Project &project)
    {
        m_projects.removeOne(project);
    }

    void clearProjects()
    {
        m_projects.clear();
    }

protected:
    ProjectGenerator()
    {
    }

private:
    QList<Project> m_projects;
};

} // namespace qbs

#ifdef __cplusplus
extern "C" {
#endif

typedef qbs::ProjectGenerator **(*getGenerators_f)();

#ifdef __cplusplus
}
#endif

#endif // GENERATORPLUGIN_H
