/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#ifndef QBS_ITEMDECLARATION_H
#define QBS_ITEMDECLARATION_H

#include "deprecationinfo.h"
#include "itemtype.h"
#include "propertydeclaration.h"

#include <QSet>
#include <QString>

namespace qbs {
namespace Internal {

class ItemDeclaration
{
public:
    ItemDeclaration(ItemType type = ItemType::Unknown);

    ItemType type() const { return m_type; }

    typedef QList<PropertyDeclaration> Properties;
    void setProperties(const Properties &props) { m_properties = props; }
    const Properties &properties() const { return m_properties; }

    void setDeprecationInfo(const DeprecationInfo &di) { m_deprecationInfo = di; }
    DeprecationInfo deprecationInfo() const { return m_deprecationInfo; }

    ItemDeclaration &operator<<(const PropertyDeclaration &decl);

    typedef QSet<ItemType> TypeNames;
    void setAllowedChildTypes(const TypeNames &typeNames) { m_allowedChildTypes = typeNames; }
    const TypeNames &allowedChildTypes() const { return m_allowedChildTypes; }
    bool isChildTypeAllowed(ItemType type) const;

private:
    ItemType m_type;
    Properties m_properties;
    TypeNames m_allowedChildTypes;
    DeprecationInfo m_deprecationInfo;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_ITEMDECLARATION_H
