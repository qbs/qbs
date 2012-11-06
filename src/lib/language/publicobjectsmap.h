/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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
#ifndef PUBLICOBJECTSMAP_H
#define PUBLICOBJECTSMAP_H

#include "language.h"
#include "publictypes.h"

#include <QtGlobal>
#include <QHash>

namespace qbs {

class PublicObjectsMap
{
public:
    void insertGroup(Group::Id id, const ResolvedGroup::Ptr &group) { m_groups.insert(id, group); }
    void insertProduct(Product::Id id, const ResolvedProduct::Ptr &product) {
        m_products.insert(id, product);
    }
    void insertProject(Project::Id id, const ResolvedProject::Ptr &project) {
        m_projects.insert(id, project);
    }

    ResolvedGroup::Ptr group(Group::Id id) const { return m_groups.value(id); }
    ResolvedProduct::Ptr product(Product::Id id) const { return m_products.value(id); }
    ResolvedProject::Ptr project(Project::Id id) const { return m_projects.value(id); }

private:
    QHash<Group::Id, ResolvedGroup::Ptr> m_groups;
    QHash<Product::Id, ResolvedProduct::Ptr> m_products;
    QHash<Project::Id, ResolvedProject::Ptr> m_projects;
};

} // namespace qbs

#endif // PUBLICOBJECTSMAP_H
