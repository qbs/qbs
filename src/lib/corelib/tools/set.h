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

#ifndef QBS_SET_H
#define QBS_SET_H

#include <tools/dynamictypecheck.h>
#include <tools/persistence.h>

#ifdef QT_CORE_LIB
#include <QtCore/qstringlist.h>
#include <QtCore/qvector.h>
#endif

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <set>
#include <type_traits>

namespace qbs {
namespace Internal {

template<typename T> class Set;
template<typename T> Set<T> operator&(const Set<T> &set1, const Set<T> &set2);
template<typename T> Set<T> operator-(const Set<T> &set1, const Set<T> &set2);

namespace helper {
template<typename T> struct SortAfterLoad { static const bool required = false; };
template<typename T> struct SortAfterLoad<T *> { static const bool required = true; };
template<typename T> struct SortAfterLoad<std::shared_ptr<T>> { static const bool required = true; };
}

template<typename T> class Set
{
public:
    using const_iterator = typename std::vector<T>::const_iterator;
    using iterator = typename std::vector<T>::iterator;
    using reverse_iterator = typename std::vector<T>::reverse_iterator;
    using const_reverse_iterator = typename std::vector<T>::const_reverse_iterator;
    using size_type = typename std::vector<T>::size_type;
    using value_type = T;
    using difference_type = typename std::vector<T>::difference_type;
    using pointer = typename std::vector<T>::pointer;
    using const_pointer = typename std::vector<T>::const_pointer;
    using reference = typename std::vector<T>::reference;
    using const_reference = typename std::vector<T>::const_reference;

    iterator begin() { return m_data.begin(); }
    iterator end() { return m_data.end(); }
    reverse_iterator rbegin() { return m_data.rbegin(); }
    reverse_iterator rend() { return m_data.rend(); }
    const_reverse_iterator rbegin() const { return m_data.rbegin(); }
    const_reverse_iterator rend() const { return m_data.rend(); }
    const_reverse_iterator crbegin() const { return m_data.crbegin(); }
    const_reverse_iterator crend() const { return m_data.crend(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator end() const { return m_data.end(); }
    const_iterator cbegin() const { return m_data.cbegin(); }
    const_iterator cend() const { return m_data.cend(); }
    const_iterator constBegin() const { return m_data.cbegin(); }
    const_iterator constEnd() const { return m_data.cend(); }

    Set() = default;
    Set(const std::initializer_list<T> &list);

    Set &unite(const Set &other);
    Set &operator+=(const Set &other) { return unite(other); }
    Set &operator|=(const Set &other) { return unite(other); }

    Set &subtract(const Set &other);
    Set &operator-=(const Set &other) { return subtract(other); }

    Set &intersect(const Set &other);
    Set &operator&=(const Set &other) { return intersect(other); }
    Set &operator&=(const T &v) { return intersect(Set{ v }); }

    iterator find(const T &v) { return std::find(m_data.begin(), m_data.end(), v); }
    const_iterator find(const T &v) const { return std::find(m_data.cbegin(), m_data.cend(), v); }
    std::pair<iterator, bool> insert(const T &v);
    Set &operator+=(const T &v) { insert(v); return *this; }
    Set &operator|=(const T &v) { return operator+=(v); }
    Set &operator<<(const T &v) { return operator+=(v); }

    bool contains(const T &v) const { return std::binary_search(cbegin(), cend(), v); }
    bool contains(const Set<T> &other) const;
    bool empty() const { return m_data.empty(); }
    size_type size() const { return m_data.size(); }
    size_type capacity() const { return m_data.capacity(); }
    bool intersects(const Set<T> &other) const;

    bool remove(const T &v);
    void operator-=(const T &v) { remove(v); }
    iterator erase(iterator it) { return m_data.erase(it); }
    iterator erase(iterator first, iterator last) { return m_data.erase(first, last); }

    void clear() { m_data.clear(); }
    void reserve(size_type size) { m_data.reserve(size); }

    void swap(Set<T> &other) { m_data.swap(other.m_data); }

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

#ifdef QT_CORE_LIB
    QStringList toStringList() const;
    QString toString(const T& value) const { return value.toString(); }
    QString toString() const;

    static Set<T> fromList(const QList<T> &list);
    QList<T> toList() const;
#endif

    static Set<T> fromStdVector(const std::vector<T> &vector);
    static Set<T> fromStdSet(const std::set<T> &set);
    std::set<T> toStdSet() const;

    template<typename U> static Set<T> filtered(const Set<U> &s);

    bool operator==(const Set &other) const { return m_data == other.m_data; }
    bool operator!=(const Set &other) const { return m_data != other.m_data; }

private:
    friend Set<T> operator&<>(const Set<T> &set1, const Set<T> &set2);
    friend Set<T> operator-<>(const Set<T> &set1, const Set<T> &set2);

    void sort() { std::sort(m_data.begin(), m_data.end()); }
    T loadElem(PersistentPool &pool) { return pool.load<T>(); }
    void storeElem(PersistentPool &pool, const T &v) const { pool.store(v); }
    bool sortAfterLoadRequired() const { return helper::SortAfterLoad<T>::required; }
    iterator asMutableIterator(const_iterator cit);

    std::vector<T> m_data;
};

template<typename T> Set<T>::Set(const std::initializer_list<T> &list) : m_data(list)
{
    sort();
    const auto last = std::unique(m_data.begin(), m_data.end());
    m_data.erase(last, m_data.end());
}

template<typename T> Set<T> &Set<T>::intersect(const Set<T> &other)
{
    auto it = begin();
    auto otherIt = other.cbegin();
    while (it != end()) {
        if (otherIt == other.cend()) {
            m_data.erase(it, end());
            break;
        }
        if (*it < *otherIt) {
            it = erase(it);
            continue;
        }
        if (!(*otherIt < *it))
            ++it;
        ++otherIt;
    }
    return *this;
}

template<typename T> std::pair<typename Set<T>::iterator, bool> Set<T>::insert(const T &v)
{
    const auto it = std::lower_bound(m_data.begin(), m_data.end(), v);
    if (it == m_data.end() || v < *it)
        return std::make_pair(m_data.insert(it, v), true);
    return std::make_pair(it, false);
}

template<typename T> bool Set<T>::contains(const Set<T> &other) const
{
    auto it = cbegin();
    auto otherIt = other.cbegin();
    while (otherIt != other.cend()) {
        if (it == cend() || *otherIt < *it)
            return false;
        if (!(*it < *otherIt))
            ++otherIt;
        ++it;
    }
    return true;
}

template<typename T> bool Set<T>::intersects(const Set<T> &other) const
{
    auto it = cbegin();
    auto itOther = other.cbegin();
    while (it != cend() && itOther != other.cend()) {
        if (*it < *itOther)
            ++it;
        else if (*itOther < *it)
            ++itOther;
        else
            return true;
    }
    return false;
}

template<typename T> Set<T> &Set<T>::unite(const Set<T> &other)
{
    if (other.empty())
        return *this;
    if (empty()) {
        m_data = other.m_data;
        return *this;
    }
    auto lowerBound = m_data.begin();
    for (auto otherIt = other.cbegin(); otherIt != other.cend(); ++otherIt) {
        lowerBound = std::lower_bound(lowerBound, m_data.end(), *otherIt);
        if (lowerBound == m_data.end()) {
            m_data.reserve(size() + std::distance(otherIt, other.cend()));
            std::copy(otherIt, other.cend(), std::back_inserter(m_data));
            return *this;
        }
        if (*otherIt < *lowerBound)
            lowerBound = m_data.insert(lowerBound, *otherIt);
    }
    return *this;
}

template<typename T> bool Set<T>::remove(const T &v)
{
    const auto it = std::lower_bound(m_data.cbegin(), m_data.cend(), v);
    if (it != m_data.cend() && !(v < *it)) {
        m_data.erase(asMutableIterator(it));
        return true;
    }
    return false;
}

template<typename T> void Set<T>::load(PersistentPool &pool)
{
    clear();
    int i = pool.load<int>();
    reserve(i);
    for (; --i >= 0;)
        m_data.push_back(loadElem(pool));
    if (sortAfterLoadRequired())
        sort();
}

template<typename T> void Set<T>::store(PersistentPool &pool) const
{
    pool.store(static_cast<int>(size()));
    std::for_each(m_data.cbegin(), m_data.cend(),
                  std::bind(&Set<T>::storeElem, this, std::ref(pool), std::placeholders::_1));
}

#ifdef QT_CORE_LIB
template<typename T> QStringList Set<T>::toStringList() const
{
    QStringList sl;
    sl.reserve(int(size()));
    std::transform(cbegin(), cend(), std::back_inserter(sl),
                   [this](const T &e) { return toString(e); });
    return sl;
}

template<typename T> QString Set<T>::toString() const
{
    return QLatin1Char('[') + toStringList().join(QLatin1String(", ")) + QLatin1Char(']');
}

template<> inline QString Set<QString>::toString(const QString &value) const { return value; }

template<typename T> Set<T> Set<T>::fromList(const QList<T> &list)
{
    Set<T> s;
    std::copy(list.cbegin(), list.cend(), std::back_inserter(s.m_data));
    s.sort();
    return s;
}

template<typename T> QList<T> Set<T>::toList() const
{
    QList<T> list;
    std::copy(m_data.cbegin(), m_data.cend(), std::back_inserter(list));
    return list;
}
#endif

template<typename T> Set<T> Set<T>::fromStdVector(const std::vector<T> &vector)
{
    Set<T> s;
    std::copy(vector.cbegin(), vector.cend(), std::back_inserter(s.m_data));
    s.sort();
    return s;
}

template<typename T> Set<T> Set<T>::fromStdSet(const std::set<T> &set)
{
    Set<T> s;
    std::copy(set.cbegin(), set.cend(), std::back_inserter(s.m_data));
    return s;
}

template<typename T> std::set<T> Set<T>::toStdSet() const
{
    std::set<T> set;
    for (auto it = cbegin(); it != cend(); ++it)
        set.insert(*it);
    return set;
}

template<typename T>
typename Set<T>::iterator Set<T>::asMutableIterator(typename Set<T>::const_iterator cit)
{
    const auto offset = std::distance(cbegin(), cit);
    return begin() + offset;
}

template<typename T> template<typename U> Set<T> Set<T>::filtered(const Set<U> &s)
{
    static_assert(std::is_pointer<T>::value, "Set::filtered() assumes pointer types");
    static_assert(std::is_pointer<U>::value, "Set::filtered() assumes pointer types");
    Set<T> filteredSet;
    for (auto &u : s) {
        if (hasDynamicType<std::remove_pointer_t<T>>(u))
            filteredSet.m_data.push_back(static_cast<T>(u));
    }
    return filteredSet;
}

template<typename T> Set<T> &Set<T>::subtract(const Set<T> &other)
{
    if (empty() || other.empty())
        return *this;
    auto lowerBound = m_data.begin();
    for (auto otherIt = other.cbegin(); otherIt != other.cend(); ++otherIt) {
        lowerBound = std::lower_bound(lowerBound, m_data.end(), *otherIt);
        if (lowerBound == m_data.end())
            return *this;
        if (!(*otherIt < *lowerBound))
            lowerBound = m_data.erase(lowerBound);
    }
    return *this;
}

template<typename T> Set<T> operator+(const Set<T> &set1, const Set<T> &set2)
{
    Set<T> result = set1;
    return result += set2;
}

template<typename T> Set<T> operator|(const Set<T> &set1, const Set<T> &set2)
{
    return set1 + set2;
}

template<typename T> Set<T> operator-(const Set<T> &set1, const Set<T> &set2)
{
    if (set1.empty() || set2.empty())
        return set1;
    Set<T> result;
    auto it1 = set1.cbegin();
    auto it2 = set2.cbegin();
    while (it1 != set1.cend()) {
        if (it2 == set2.cend()) {
            std::copy(it1, set1.cend(), std::back_inserter(result.m_data));
            break;
        }
        if (*it1 < *it2) {
            result.m_data.push_back(*it1++);
        } else if (*it2 < *it1) {
            ++it2;
        } else {
            ++it1;
            ++it2;
        }
    }
    return result;
}

template<typename T> Set<T> operator&(const Set<T> &set1, const Set<T> &set2)
{
    Set<T> result;
    auto it1 = set1.cbegin();
    auto it2 = set2.cbegin();
    while (it1 != set1.cend() && it2 != set2.cend()) {
        if (*it1 < *it2) {
            ++it1;
            continue;
        }
        if (*it2 < *it1) {
            ++it2;
            continue;
        }
        result.m_data.push_back(*it1);
        ++it1;
        ++it2;
    }
    return result;
}

} // namespace Internal
} // namespace qbs

#endif // Include guard
