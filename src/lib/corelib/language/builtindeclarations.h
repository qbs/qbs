/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QBS_BUILTINDECLARATIONS_H
#define QBS_BUILTINDECLARATIONS_H

#include "itemdeclaration.h"
#include "itemtype.h"

#include <tools/codelocation.h>
#include <tools/version.h>

#include <QtCore/qhash.h>
#include <QtCore/qmap.h>
#include <QtCore/qstring.h>

namespace qbs {
namespace Internal {

class BuiltinDeclarations
{
public:
    static const BuiltinDeclarations &instance();

    Version languageVersion() const;
    QStringList allTypeNames() const;
    ItemDeclaration declarationsForType(ItemType type) const;
    ItemType typeForName(const QString &typeName,
                         const CodeLocation &location = CodeLocation()) const;
    QString nameForType(ItemType itemType) const;
    QStringList argumentNamesForScriptFunction(ItemType itemType, const QString &scriptName) const;

protected:
    BuiltinDeclarations();

private:
    void insert(const ItemDeclaration &decl);
    void addArtifactItem();
    void addDependsItem();
    void addExportItem();
    void addFileTaggerItem();
    void addGroupItem();
    void addJobLimitItem();
    void addModuleItem();
    void addModuleProviderItem();
    static ItemDeclaration moduleLikeItem(ItemType type);
    void addProbeItem();
    void addProductItem();
    void addProfileItem();
    void addProjectItem();
    void addPropertiesItem();
    void addPropertyOptionsItem();
    void addRuleItem();
    void addSubprojectItem();
    void addTransformerItem();
    void addScannerItem();

    const Version m_languageVersion;
    QMap<ItemType, ItemDeclaration> m_builtins;
    const QHash<QString, ItemType> m_typeMap;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_BUILTINDECLARATIONS_H
