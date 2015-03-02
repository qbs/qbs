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

#ifndef QBS_TOOLS_ID_H
#define QBS_TOOLS_ID_H

#include <QMetaType>
#include <QString>
#include <QVariant>

namespace qbs {
namespace Internal {

class Id
{
public:
    enum { IdsPerPlugin = 10000, ReservedPlugins = 1000 };

    Id() : m_id(0) {}
    Id(int uid) : m_id(uid) {}
    Id(const char *name);
//    explicit Id(const QString &name);
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
    static Id fromUniqueIdentifier(int uid) { return Id(uid); }
    static Id fromString(const QString &str); // FIXME: avoid.
    static Id fromName(const QByteArray &ba); // FIXME: avoid.
    static Id fromSetting(const QVariant &variant); // Good to use.
    static void registerId(int uid, const char *name);

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
