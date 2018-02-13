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

#ifndef QBS_PROPERTYMAPINTERNAL_H
#define QBS_PROPERTYMAPINTERNAL_H

#include "forward_decls.h"
#include <tools/persistence.h>
#include <tools/qbs_export.h>
#include <QtCore/qvariant.h>

namespace qbs {
namespace Internal {

class QBS_AUTOTEST_EXPORT PropertyMapInternal
{
public:
    static PropertyMapPtr create() { return PropertyMapPtr(new PropertyMapInternal); }
    PropertyMapPtr clone() const { return PropertyMapPtr(new PropertyMapInternal(*this)); }

    const QVariantMap &value() const { return m_value; }
    QVariant moduleProperty(const QString &moduleName,
                            const QString &key, bool *isPresent = nullptr) const;
    QVariant qbsPropertyValue(const QString &key) const; // Convenience function.
    QVariant property(const QStringList &name) const;
    void setValue(const QVariantMap &value);

    template<PersistentPool::OpType opType> void completeSerializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(m_value);
    }

private:
    friend bool operator==(const PropertyMapInternal &lhs, const PropertyMapInternal &rhs);

    PropertyMapInternal();
    PropertyMapInternal(const PropertyMapInternal &other);

    QVariantMap m_value;
};

inline bool operator==(const PropertyMapInternal &lhs, const PropertyMapInternal &rhs)
{
    return lhs.m_value == rhs.m_value;
}

QVariant QBS_AUTOTEST_EXPORT moduleProperty(const QVariantMap &properties,
                                            const QString &moduleName,
                                            const QString &key, bool *isPresent = nullptr);

} // namespace Internal
} // namespace qbs

#endif // QBS_PROPERTYMAPINTERNAL_H
