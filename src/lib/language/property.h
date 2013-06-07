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
#ifndef QBS_PROPERTY_H
#define QBS_PROPERTY_H

#include <tools/qbsassert.h>

#include <QSet>
#include <QString>
#include <QVariant>

class Property
{
public:
    enum Kind
    {
        PropertyInModule,
        PropertyInProduct
    };

    Property()
        : kind(PropertyInModule)
    {
    }

    Property(const QString &m, const QString &p, const QVariant &v, Kind k = PropertyInModule)
        : moduleName(m), propertyName(p), value(v), kind(k)
    {
        QBS_CHECK(!moduleName.contains(QLatin1Char('.')));
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

typedef QSet<Property> PropertyList;

#endif // Include guard
