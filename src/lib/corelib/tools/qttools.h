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

#include <tools/qbsassert.h>
#include <tools/porting.h>
#include <tools/stlutils.h>

#include <QtCore/qhash.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qtextstream.h>
#include <QtCore/qvariant.h>

#include <functional>

QT_BEGIN_NAMESPACE
class QProcessEnvironment;
QT_END_NAMESPACE

namespace std {
template<typename T1, typename T2> struct hash<std::pair<T1, T2>>
{
    size_t operator()(const pair<T1, T2> &x) const
    {
        return std::hash<T1>()(x.first) ^ std::hash<T2>()(x.second);
    }
};

template <typename... Ts>
struct hash<std::tuple<Ts...>>
{
private:
    template<std::size_t... Ns>
    static size_t helper(std::index_sequence<Ns...>, const std::tuple<Ts...> &tuple) noexcept
    {
        size_t seed = 0;
        (qbs::Internal::hashCombineHelper(seed, std::get<Ns>(tuple)), ...);
        return seed;
    }

public:
    size_t operator()(const std::tuple<Ts...> & tuple) const noexcept
    {
        return helper(std::make_index_sequence<sizeof...(Ts)>(), tuple);
    }
};


template<> struct hash<QStringList>
{
    std::size_t operator()(const QStringList &s) const noexcept
    {
        return qbs::Internal::hashRange(s);
    }
};

template<> struct hash<QVariant>
{
    size_t operator()(const QVariant &v) const noexcept;
};

template<> struct hash<QVariantList>
{
    size_t operator()(const QVariantList &v) const noexcept
    {
        return qbs::Internal::hashRange(v);
    }
};

template<> struct hash<QVariantMap>
{
    size_t operator()(const QVariantMap &v) const noexcept
    {
        return qbs::Internal::hashRange(v);
    }
};

template<> struct hash<QVariantHash>
{
    size_t operator()(const QVariantHash &v) const noexcept
    {
        return qbs::Internal::hashRange(v);
    }
};

} // namespace std

QT_BEGIN_NAMESPACE

qbs::QHashValueType qHash(const QStringList &list);
qbs::QHashValueType qHash(const QProcessEnvironment &env);

template<typename... Args>
qbs::QHashValueType qHash(const std::tuple<Args...> &tuple)
{
    return std::hash<std::tuple<Args...>>()(tuple) % std::numeric_limits<uint>::max();
}

inline qbs::QHashValueType qHash(const QVariant &v)
{
    return std::hash<QVariant>()(v) % std::numeric_limits<uint>::max();
}

inline qbs::QHashValueType qHash(const QVariantMap &v)
{
    return std::hash<QVariantMap>()(v) % std::numeric_limits<uint>::max();
}

inline qbs::QHashValueType qHash(const QVariantHash &v)
{
    return std::hash<QVariantHash>()(v) % std::numeric_limits<uint>::max();
}

QT_END_NAMESPACE

namespace qbs {

template <class T>
QSet<T> toSet(const QList<T> &list)
{
    return QSet<T>(list.begin(), list.end());
}

template<class T>
QList<T> toList(const QSet<T> &set)
{
    return QList<T>(set.begin(), set.end());
}

template<typename K, typename V>
QHash<K, V> &unite(QHash<K, V> &h, const QHash<K, V> &other)
{
    h.insert(other);
    return h;
}

inline void setupDefaultCodec(QTextStream &stream)
{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    stream.setCodec("UTF-8");
#else
    Q_UNUSED(stream);
#endif
}

inline bool qVariantCanConvert(const QVariant &variant, int typeId)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return variant.canConvert(QMetaType(typeId));
#else
    return variant.canConvert(typeId); // deprecated in Qt6
#endif
}

inline bool qVariantConvert(QVariant &variant, int typeId)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    return variant.convert(QMetaType(typeId));
#else
    return variant.convert(typeId); // deprecated in Qt6
#endif
}

} // namespace qbs

#endif // QBSQTTOOLS_H
