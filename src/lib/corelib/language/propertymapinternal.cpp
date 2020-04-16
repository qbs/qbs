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

#include "propertymapinternal.h"

#include <tools/jsliterals.h>
#include <tools/scripttools.h>
#include <tools/stringconstants.h>

namespace qbs {
namespace Internal {

/*!
 * \class PropertyMapInternal
 * \brief The \c PropertyMapInternal class contains a set of properties and their values.
 * An instance of this class is attached to every \c ResolvedProduct.
 * \c ResolvedGroups inherit their properties from the respective \c ResolvedProduct, \c SourceArtifacts
 * inherit theirs from the respective \c ResolvedGroup. \c ResolvedGroups can override the value of an
 * inherited property, \c SourceArtifacts cannot. If a property value is overridden, a new
 * \c PropertyMapInternal object is allocated, otherwise the pointer is shared.
 * \sa ResolvedGroup
 * \sa ResolvedProduct
 * \sa SourceArtifact
 */
PropertyMapInternal::PropertyMapInternal() = default;

PropertyMapInternal::PropertyMapInternal(const PropertyMapInternal &other) = default;

QVariant PropertyMapInternal::moduleProperty(const QString &moduleName, const QString &key,
                                             bool *isPresent) const
{
    return ::qbs::Internal::moduleProperty(m_value, moduleName, key, isPresent);
}

QVariant PropertyMapInternal::qbsPropertyValue(const QString &key) const
{
    return moduleProperty(StringConstants::qbsModule(), key);
}

QVariant PropertyMapInternal::property(const QStringList &name) const
{
    return getConfigProperty(m_value, name);
}

void PropertyMapInternal::setValue(const QVariantMap &map)
{
    m_value = map;
}

QVariant moduleProperty(const QVariantMap &properties, const QString &moduleName,
                        const QString &key, bool *isPresent)
{
    const auto moduleIt = properties.find(moduleName);
    if (moduleIt == properties.end()) {
        if (isPresent)
            *isPresent = false;
        return {};
    }
    const QVariantMap &moduleMap = moduleIt.value().toMap();
    const auto propertyIt = moduleMap.find(key);
    if (propertyIt == moduleMap.end()) {
        if (isPresent)
            *isPresent = false;
        return {};
    }
    if (isPresent)
        *isPresent = true;
    return propertyIt.value();
}

} // namespace Internal
} // namespace qbs
