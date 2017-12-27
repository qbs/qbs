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

#include "msbuildproject.h"

#include "imsbuildnodevisitor.h"
#include "msbuildimport.h"
#include "msbuildimportgroup.h"
#include "msbuilditemdefinitiongroup.h"
#include "msbuilditemgroup.h"
#include "msbuildpropertygroup.h"

namespace qbs {

class MSBuildProjectPrivate
{
public:
    QString defaultTargets;
    QString toolsVersion;
};

MSBuildProject::MSBuildProject(QObject *parent)
    : QObject(parent)
    , d(new MSBuildProjectPrivate)
{
}

MSBuildProject::~MSBuildProject() = default;

QString MSBuildProject::defaultTargets() const
{
    return d->defaultTargets;
}

void MSBuildProject::setDefaultTargets(const QString &defaultTargets)
{
    d->defaultTargets = defaultTargets;
}

QString MSBuildProject::toolsVersion() const
{
    return d->toolsVersion;
}

void MSBuildProject::setToolsVersion(const QString &toolsVersion)
{
    d->toolsVersion = toolsVersion;
}

void MSBuildProject::accept(IMSBuildNodeVisitor *visitor) const
{
    visitor->visitStart(this);

    for (const auto &child : children()) {
        if (const auto node = qobject_cast<MSBuildImport *>(child))
            node->accept(visitor);
        else if (const auto node = qobject_cast<MSBuildImportGroup *>(child))
            node->accept(visitor);
        else if (const auto node = qobject_cast<MSBuildItemDefinitionGroup *>(child))
            node->accept(visitor);
        else if (const auto node = qobject_cast<MSBuildItemGroup *>(child))
            node->accept(visitor);
        else if (const auto node = qobject_cast<MSBuildPropertyGroup *>(child))
            node->accept(visitor);
    }

    visitor->visitEnd(this);
}

} // namespace qbs
