/****************************************************************************
**
** Copyright (C) 2025 Ivan Komissarov (abbapoh@gmail.com).
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

#pragma once

#include <vector>
#include <QString>

class QFile;

namespace qbs {
namespace Internal {

struct DotGraphNode
{
    size_t id{0};
    QString label;
    bool enabled{true};

    enum class Type { Product, Project, Module };
    Type type;

    static QByteArray typeToString(Type type)
    {
        if (type == Type::Project)
            return QByteArray("ellipse");
        if (type == Type::Product)
            return QByteArray("box");
        if (type == Type::Module)
            return QByteArray("diamond");
        return QByteArray("box");
    }

    QByteArray toString() const
    {
        QByteArray result;
        result += QByteArray::number(quintptr(id)) + " [";
        result += "shape=" + typeToString(type);
        if (!enabled) {
            result += " fontcolor=grey";
            result += " color=grey";
        }
        result += " label=\"" + label.toUtf8() + "\"";
        result += "]";
        return result;
    }
};

struct Relation
{
    size_t from{0};
    size_t to{0};

    QByteArray toString() const
    {
        return QByteArray::number(quintptr(from)) + " -> " + QByteArray::number(quintptr(to));
    }
};

class DotGraph
{
public:
    void write(QFile &file);
    void write(const QString &filePath);

    size_t addNode(DotGraphNode &&node)
    {
        m_nodes.push_back(std::move(node));
        return m_nodes.back().id;
    }
    void addRelation(Relation &&relation) { m_relations.push_back(std::move(relation)); }
    size_t createNodeId() { return m_nodeIds++; }

private:
    std::vector<DotGraphNode> m_nodes;
    std::vector<Relation> m_relations;
    size_t m_nodeIds = 1;
};

} // namespace Internal
} // namespace qbs
