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
#include "propertyfinder.h"

#include "qbsassert.h"

namespace qbs {
namespace Internal {

QVariantList PropertyFinder::propertyValues(const QVariantMap &properties,
        const QString &moduleName, const QString &key, MergeType mergeType)
{
    m_moduleName = moduleName;
    m_key = key;
    m_findOnlyOne = false;
    m_values.clear();
    findModuleValues(properties);
    if (mergeType == DoMergeLists)
        mergeLists(&m_values);
    return m_values;
}

QVariant PropertyFinder::propertyValue(const QVariantMap &properties, const QString &moduleName,
                                       const QString &key)
{
    m_moduleName = moduleName;
    m_key = key;
    m_findOnlyOne = true;
    m_values.clear();
    findModuleValues(properties);

    QBS_ASSERT(m_values.count() <= 1, return QVariant());
    return m_values.isEmpty() ? QVariant() : m_values.first();
}

void PropertyFinder::findModuleValues(const QVariantMap &properties)
{
    QVariantMap moduleProperties = properties.value(QLatin1String("modules")).toMap();

    // Direct hits come first.
    const QVariantMap::Iterator modIt = moduleProperties.find(m_moduleName);
    if (modIt != moduleProperties.end()) {
        const QVariantMap moduleMap = modIt->toMap();
        const QVariant property = moduleMap.value(m_key);
        addToList(property);
        moduleProperties.erase(modIt);
    }

    // These are the non-matching modules.
    for (QVariantMap::ConstIterator it = moduleProperties.constBegin();
         it != moduleProperties.constEnd() && (m_values.isEmpty() || !m_findOnlyOne); ++it) {
        findModuleValues(it->toMap());
    }
}

void PropertyFinder::addToList(const QVariant &value)
{
    // Note: This means that manually setting a property to "null" will not lead to a "hit".
    if (!value.isNull() && !m_values.contains(value))
        m_values << value;
}

void PropertyFinder::mergeLists(QVariantList *values)
{
    QVariantList::iterator it = values->begin();
    while (it != values->end()) {
        if (it->canConvert<QVariantList>()) {
            QVariantList sublist = it->toList();
            mergeLists(&sublist);
            it = values->erase(it);
            for (int k = sublist.count(); --k >= 0;)
                it = values->insert(it, sublist.at(k));
        } else {
            ++it;
        }
    }
}

} // namespace Internal
} // namespace qbs
