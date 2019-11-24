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

#ifndef IMSBUILDNODEVISITOR_H
#define IMSBUILDNODEVISITOR_H

namespace qbs {

class MSBuildImport;
class MSBuildImportGroup;
class MSBuildItem;
class MSBuildItemDefinitionGroup;
class MSBuildItemGroup;
class MSBuildItemMetadata;
class MSBuildProject;
class MSBuildProperty;
class MSBuildPropertyGroup;

class IMSBuildNodeVisitor
{
public:
    virtual ~IMSBuildNodeVisitor() = default;

    virtual void visitStart(const MSBuildImport *import) = 0;
    virtual void visitEnd(const MSBuildImport *import) = 0;

    virtual void visitStart(const MSBuildImportGroup *importGroup) = 0;
    virtual void visitEnd(const MSBuildImportGroup *importGroup) = 0;

    virtual void visitStart(const MSBuildItem *item) = 0;
    virtual void visitEnd(const MSBuildItem *item) = 0;

    virtual void visitStart(const MSBuildItemDefinitionGroup *itemDefinitionGroup) = 0;
    virtual void visitEnd(const MSBuildItemDefinitionGroup *itemDefinitionGroup) = 0;

    virtual void visitStart(const MSBuildItemGroup *itemGroup) = 0;
    virtual void visitEnd(const MSBuildItemGroup *itemGroup) = 0;

    virtual void visitStart(const MSBuildItemMetadata *itemMetadata) = 0;
    virtual void visitEnd(const MSBuildItemMetadata *itemMetadata) = 0;

    virtual void visitStart(const MSBuildProject *project) = 0;
    virtual void visitEnd(const MSBuildProject *project) = 0;

    virtual void visitStart(const MSBuildProperty *property) = 0;
    virtual void visitEnd(const MSBuildProperty *property) = 0;

    virtual void visitStart(const MSBuildPropertyGroup *propertyGroup) = 0;
    virtual void visitEnd(const MSBuildPropertyGroup *propertyGroup) = 0;
};

} // namespace qbs

#endif // IMSBUILDNODEVISITOR_H
