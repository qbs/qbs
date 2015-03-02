/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
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
#ifndef QBS_PROPERTY_H
#define QBS_PROPERTY_H

#include <QSet>
#include <QString>
#include <QVariant>

namespace qbs {
namespace Internal {

class Property
{
public:
    enum Kind
    {
        PropertyInModule,
        PropertyInProduct,
        PropertyInProject
    };

    Property()
        : kind(PropertyInModule)
    {
    }

    Property(const QString &m, const QString &p, const QVariant &v, Kind k = PropertyInModule)
        : moduleName(m), propertyName(p), value(v), kind(k)
    {
    }

    QString moduleName;
    QString propertyName;
    QVariant value;
    Kind kind;
};

inline bool operator==(const Property &p1, const Property &p2)
{
    return p1.moduleName == p2.moduleName && p1.propertyName == p2.propertyName;
}

inline uint qHash(const Property &p)
{
    return QT_PREPEND_NAMESPACE(qHash)(p.moduleName + p.propertyName);
}

typedef QSet<Property> PropertySet;

} // namespace Internal
} // namespace qbs

#endif // Include guard
