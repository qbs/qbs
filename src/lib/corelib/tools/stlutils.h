/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef QBS_STLUTILS_H
#define QBS_STLUTILS_H

#include <algorithm>
#include <iterator>

namespace qbs {
namespace Internal {

template <class C>
C sorted(const C &container)
{
    C result = container;
    std::sort(std::begin(result), std::end(result));
    return result;
}

template <class C, class T>
bool contains(const C &container, const T &v)
{
    const auto &end = std::cend(container);
    return std::find(std::cbegin(container), end, v) != end;
}

template <class T, size_t N, class U>
bool contains(const T (&container)[N], const U &v)
{
    const auto &end = std::cend(container);
    return std::find(std::cbegin(container), end, v) != end;
}

template <class C>
bool containsKey(const C &container, const typename C::key_type &v)
{
    const auto &end = container.cend();
    return container.find(v) != end;
}

template <typename C>
bool removeOne(C &container, const typename C::value_type &v)
{
    auto end = std::end(container);
    auto it = std::find(std::begin(container), end, v);
    if (it == end)
        return false;
    container.erase(it);
    return true;
}

template <typename C>
void removeAll(C &container, const typename C::value_type &v)
{
    container.erase(std::remove(std::begin(container), std::end(container), v),
                    std::end(container));
}

template <class Container, class UnaryPredicate>
bool any_of(const Container &container, const UnaryPredicate &predicate)
{
    return std::any_of(std::begin(container), std::end(container), predicate);
}

template <class Container, class UnaryPredicate>
bool none_of(const Container &container, const UnaryPredicate &predicate)
{
    return std::none_of(std::begin(container), std::end(container), predicate);
}

template <class C>
C &operator<<(C &container, const typename C::value_type &v)
{
    container.push_back(v);
    return container;
}

template <class C>
C &operator<<(C &container, const C &other)
{
    container.insert(container.end(), other.cbegin(), other.cend());
    return container;
}

} // namespace Internal
} // namespace qbs

#endif // QBS_STLUTILS_H
