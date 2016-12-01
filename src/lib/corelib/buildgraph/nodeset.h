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

#ifndef QBS_NODESET_H
#define QBS_NODESET_H

#include <QtCore/qvector.h>

#include <iterator>

namespace qbs {
namespace Internal {

class BuildGraphNode;

class PersistentPool;

/**
  * Set of build graph nodes.
  * This is faster than QSet when iterating over the container.
  */
class NodeSet
{
public:
    NodeSet &unite(const NodeSet &other);

    typedef QVector<BuildGraphNode *>::const_iterator const_iterator;
    typedef QVector<BuildGraphNode *>::iterator iterator;
    typedef BuildGraphNode * value_type;

    iterator begin() { return m_data.begin(); }
    iterator end() { return m_data.end(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator end() const { return m_data.end(); }
    const_iterator constBegin() const { return m_data.begin(); }
    const_iterator constEnd() const { return m_data.end(); }

    void insert(BuildGraphNode *node)
    {
        if (!m_data.contains(node))
            m_data << node;
    }

    void operator+=(BuildGraphNode *node)
    {
        insert(node);
    }

    NodeSet &operator<<(BuildGraphNode *node)
    {
        insert(node);
        return *this;
    }

    void remove(BuildGraphNode *node);

    bool contains(BuildGraphNode *node) const
    {
        return m_data.contains(node);
    }

    void clear()
    {
        m_data.clear();
    }

    bool isEmpty() const
    {
        return m_data.empty();
    }

    int count() const
    {
        return m_data.size();
    }

    void reserve(int size)
    {
        m_data.reserve(size);
    }

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

private:
    QVector<BuildGraphNode *> m_data;
};

template <class T>
class TypeFilter
{
    const NodeSet &m_nodes;
public:
    TypeFilter(const NodeSet &nodes)
        : m_nodes(nodes)
    {
    }

    class const_iterator : public std::iterator<std::forward_iterator_tag, T *>
    {
        const NodeSet &m_nodes;
        NodeSet::const_iterator m_it;
    public:
        const_iterator(const NodeSet &nodes, const NodeSet::const_iterator &it)
            : m_nodes(nodes), m_it(it)
        {
            while (m_it != m_nodes.constEnd() && dynamic_cast<T *>(*m_it) == 0)
                ++m_it;
        }

        bool operator==(const const_iterator &rhs)
        {
            return m_it == rhs.m_it;
        }

        bool operator!=(const const_iterator &rhs)
        {
            return !(*this == rhs);
        }

        const_iterator &operator++()
        {
            for (;;) {
                ++m_it;
                if (m_it == m_nodes.constEnd() || dynamic_cast<T *>(*m_it))
                    return *this;
            }
        }

        T *operator*() const
        {
            return static_cast<T *>(*m_it);
        }
    };

    const_iterator begin() const
    {
        return const_iterator(m_nodes, m_nodes.constBegin());
    }

    const_iterator end() const
    {
        return const_iterator(m_nodes, m_nodes.constEnd());
    }
};

template <class T>
const TypeFilter<T> filterByType(const NodeSet &nodes)
{
    return TypeFilter<T>(nodes);
}

} // namespace Internal
} // namespace qbs

#endif // QBS_NODESET_H
