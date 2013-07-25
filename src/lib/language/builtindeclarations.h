/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QBS_BUILTINDECLARATIONS_H
#define QBS_BUILTINDECLARATIONS_H

#include "propertydeclaration.h"

#include <QByteArray>
#include <QMap>

namespace qbs {
namespace Internal {

class Item;

class BuiltinDeclarations
{
public:
    BuiltinDeclarations();

    QString languageVersion() const;
    QByteArray qmlTypeInfo() const;
    QList<PropertyDeclaration> declarationsForType(const QString &typeName) const;
    void setupItemForBuiltinType(qbs::Internal::Item *item) const;

private:
    void addArtifactItem();
    void addDependsItem();
    void addExportItem();
    void addFileTaggerItem();
    void addGroupItem();
    void addModuleItem();
    void addProbeItem();
    void addProductItem();
    void addProjectItem();
    void addPropertyOptionsItem();
    void addRuleItem();
    void addSubprojectItem();
    void addTransformerItem();

    QString m_languageVersion;
    QMap<QString, QList<PropertyDeclaration> > m_builtins;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_BUILTINDECLARATIONS_H
