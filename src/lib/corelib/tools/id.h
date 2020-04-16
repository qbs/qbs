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

#ifndef QBS_TOOLS_ID_H
#define QBS_TOOLS_ID_H

#include "qbs_export.h"

#include <QtCore/qmetatype.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>

namespace qbs {
namespace Internal {

class QBS_AUTOTEST_EXPORT Id
{
public:
    enum { IdsPerPlugin = 10000, ReservedPlugins = 1000 };

    Id() : m_id(0) {}
    Id(int uid) : m_id(uid) {}
    Id(const char *name);
    explicit Id(const QByteArray &name);

    Id withSuffix(int suffix) const;
    Id withSuffix(const char *name) const;
    Id withPrefix(const char *name) const;

    QByteArray name() const;
    QString toString() const; // Avoid.
    QVariant toSetting() const; // Good to use.
    bool isValid() const { return m_id; }
    bool operator==(Id id) const { return m_id == id.m_id; }
    bool operator==(const char *name) const;
    bool operator!=(Id id) const { return m_id != id.m_id; }
    bool operator!=(const char *name) const { return !operator==(name); }
    bool operator<(Id id) const { return m_id < id.m_id; }
    bool operator>(Id id) const { return m_id > id.m_id; }
    bool alphabeticallyBefore(Id other) const;
    int uniqueIdentifier() const { return m_id; }
    static Id fromUniqueIdentifier(int uid) { return {uid}; }
    static Id fromSetting(const QVariant &variant); // Good to use.

private:
    // Intentionally unimplemented
    Id(const QLatin1String &);
    int m_id;
};

inline uint qHash(const Id &id) { return id.uniqueIdentifier(); }

} // namespace Internal
} // namespace qbs

Q_DECLARE_METATYPE(qbs::Internal::Id)
Q_DECLARE_METATYPE(QList<qbs::Internal::Id>)

#endif // QBS_TOOLS_ID_H
