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

#ifndef QBSQTTOOLS_H
#define QBSQTTOOLS_H

#include <QtCore/qhash.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qtextstream.h>

#include <functional>

QT_BEGIN_NAMESPACE
class QProcessEnvironment;
QT_END_NAMESPACE

#if (QT_VERSION < QT_VERSION_CHECK(5, 14, 0))
#define QBS_SKIP_EMPTY_PARTS QString::SkipEmptyParts
#else
#define QBS_SKIP_EMPTY_PARTS Qt::SkipEmptyParts
#endif

namespace std {
#if (QT_VERSION < QT_VERSION_CHECK(5, 14, 0))
template<> struct hash<QString> {
    std::size_t operator()(const QString &s) const { return qHash(s); }
};
#endif

template<typename T1, typename T2> struct hash<std::pair<T1, T2>>
{
    size_t operator()(const pair<T1, T2> &x) const
    {
        return std::hash<T1>()(x.first) ^ std::hash<T2>()(x.second);
    }
};
} // namespace std

QT_BEGIN_NAMESPACE

uint qHash(const QStringList &list);
uint qHash(const QProcessEnvironment &env);

#if (QT_VERSION < QT_VERSION_CHECK(5, 14, 0))
namespace Qt {
inline QTextStream &endl(QTextStream &stream) { return stream << QT_PREPEND_NAMESPACE(endl); }
} // namespace Qt
#endif

QT_END_NAMESPACE

namespace qbs {

template <class T>
QSet<T> toSet(const QList<T> &list)
{
#if (QT_VERSION < QT_VERSION_CHECK(5, 14, 0))
    return list.toSet();
#else
    return QSet<T>(list.begin(), list.end());
#endif
}

template<class T>
QList<T> toList(const QSet<T> &set)
{
#if (QT_VERSION < QT_VERSION_CHECK(5, 14, 0))
    return set.toList();
#else
    return QList<T>(set.begin(), set.end());
#endif
}

template<typename K, typename V>
QHash<K, V> &unite(QHash<K, V> &h, const QHash<K, V> &other)
{
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
    return h.unite(other);
#else
    h.insert(other);
    return h;
#endif
}

} // namespace qbs

#endif // QBSQTTOOLS_H
