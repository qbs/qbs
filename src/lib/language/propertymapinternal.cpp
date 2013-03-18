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

#include "propertymapinternal.h"
#include <tools/propertyfinder.h>
#include <tools/scripttools.h>

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
PropertyMapInternal::PropertyMapInternal()
{
}

PropertyMapInternal::PropertyMapInternal(const PropertyMapInternal &other)
    : PersistentObject(other), m_value(other.m_value)
{
}

QVariant PropertyMapInternal::qbsPropertyValue(const QString &key)
{
    return PropertyFinder().propertyValue(value(), QLatin1String("qbs"), key);
}

void PropertyMapInternal::setValue(const QVariantMap &map)
{
    m_value = map;
}

static QString toJSLiteral(const QVariantMap &vm, int level = 0)
{
    QString indent;
    for (int i = 0; i < level; ++i)
        indent += QLatin1String("    ");
    QString str;
    for (QVariantMap::const_iterator it = vm.begin(); it != vm.end(); ++it) {
        if (it.value().type() == QVariant::Map) {
            str += indent + it.key() + QLatin1String(": {\n");
            str += toJSLiteral(it.value().toMap(), level + 1);
            str += indent + QLatin1String("}\n");
        } else {
            str += indent + it.key() + QLatin1String(": ") + toJSLiteral(it.value())
                    + QLatin1Char('\n');
        }
    }
    return str;
}

QString PropertyMapInternal::toJSLiteral() const
{
    return qbs::Internal::toJSLiteral(m_value);
}

void PropertyMapInternal::load(PersistentPool &pool)
{
    pool.stream() >> m_value;
}

void PropertyMapInternal::store(PersistentPool &pool) const
{
    pool.stream() << m_value;
}

} // namespace Internal
} // namespace qbs
