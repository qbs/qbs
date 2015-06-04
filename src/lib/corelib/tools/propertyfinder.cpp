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
#include "propertyfinder.h"

#include "qbsassert.h"

#include <QStringList>

namespace qbs {
namespace Internal {

QVariantList PropertyFinder::propertyValues(const QVariantMap &properties,
        const QString &moduleName, const QString &key)
{
    QVariant v = propertyValue(properties, moduleName, key);
    return v.toList();
}

QVariant PropertyFinder::propertyValue(const QVariantMap &properties, const QString &moduleName,
                                       const QString &key)
{
    m_moduleName = moduleName;
    m_key = key;
    m_values.clear();
    findModuleValues(properties);

    return m_values.isEmpty() ? QVariant() : m_values.first();
}

void PropertyFinder::findModuleValues(const QVariantMap &properties)
{
    const QVariantMap moduleProperties = properties.value(QLatin1String("modules")).toMap();
    const QVariantMap::const_iterator modIt = moduleProperties.find(m_moduleName);
    if (modIt != moduleProperties.constEnd()) {
        const QVariantMap moduleMap = modIt->toMap();
        const QVariant property = moduleMap.value(m_key);
        addToList(property);
    }
}

void PropertyFinder::addToList(const QVariant &value)
{
    if (!value.isNull() && !m_values.contains(value))
        m_values << value;
}

} // namespace Internal
} // namespace qbs
