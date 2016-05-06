/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "property.h"

#include <tools/persistence.h>

namespace qbs {
namespace Internal {

void storePropertySet(PersistentPool &pool, const PropertySet &propertySet)
{
    pool.stream() << propertySet.count();
    foreach (const Property &p, propertySet) {
        pool.storeString(p.moduleName);
        pool.storeString(p.propertyName);
        pool.stream() << p.value << static_cast<int>(p.kind);
    }
}

PropertySet restorePropertySet(PersistentPool &pool)
{
    int count;
    pool.stream() >> count;
    PropertySet propertySet;
    propertySet.reserve(count);
    while (--count >= 0) {
        Property p;
        p.moduleName = pool.idLoadString();
        p.propertyName = pool.idLoadString();
        int k;
        pool.stream() >> p.value >> k;
        p.kind = static_cast<Property::Kind>(k);
        propertySet += p;
    }
    return propertySet;
}

void storePropertyHash(PersistentPool &pool, const PropertyHash &propertyHash)
{
    pool.stream() << propertyHash.count();
    for (auto it = propertyHash.constBegin(); it != propertyHash.constEnd(); ++it) {
        pool.storeString(it.key());
        const PropertySet &properties = it.value();
        pool.stream() << properties.count();
        foreach (const Property &p, properties) {
            pool.storeString(p.moduleName);
            pool.storeString(p.propertyName);
            pool.stream() << p.value; // kind is always PropertyInModule
        }
    }
}

PropertyHash restorePropertyHash(PersistentPool &pool)
{
    int count;
    pool.stream() >> count;
    PropertyHash propertyHash;
    propertyHash.reserve(count);
    while (--count >= 0) {
        const QString artifactName = pool.idLoadString();
        int listCount;
        pool.stream() >> listCount;
        PropertySet list;
        list.reserve(listCount);
        while (--listCount >= 0) {
            Property p;
            p.moduleName = pool.idLoadString();
            p.propertyName = pool.idLoadString();
            pool.stream() >> p.value;
            p.kind = Property::PropertyInModule;
            list += p;
        }
        propertyHash.insert(artifactName, list);
    }
    return propertyHash;
}

} // namespace Internal
} // namespace qbs
