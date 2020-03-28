/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
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

#ifndef IMSBUILDNODE_H
#define IMSBUILDNODE_H

#include <tools/stlutils.h>

#include <memory>
#include <vector>

namespace qbs {

class IMSBuildNodeVisitor;

class IMSBuildNode
{
public:
    virtual ~IMSBuildNode();
    virtual void accept(IMSBuildNodeVisitor *visitor) const = 0;
};

template<typename... Types>
class MSBuildNode : public IMSBuildNode
{
public:
    template<class T>
    T *appendChild(std::unique_ptr<T> child)
    {
        static_assert(Internal::is_any_of_types<T, Types...>, "Type is not in the allowed list");
        const auto p = child.get();
        m_children.push_back(std::move(child));
        return p;
    }

    template<class T, class... Args>
    T *makeChild(Args &&...args)
    {
        static_assert(Internal::is_any_of_types<T, Types...>, "Type is not in the allowed list");
        return appendChild(std::make_unique<T>(std::forward<Args>(args)...));
    }

    template<class T>
    std::unique_ptr<T> takeChild(T *ptr)
    {
        static_assert(Internal::is_any_of_types<T, Types...>, "Type is not in the allowed list");
        const auto pred = [ptr](const auto &node) { return node.get() == ptr; };
        const auto it = std::find_if(m_children.begin(), m_children.end(), pred);
        if (it == m_children.end())
            return {};
        std::unique_ptr<T> result(static_cast<T *>(it->release()));
        m_children.erase(it);
        return result;
    }

protected:
    const std::vector<std::unique_ptr<IMSBuildNode>> &children() const { return m_children; }
    void acceptChildren(IMSBuildNodeVisitor *visitor) const
    {
        for (const auto &child : m_children)
            child->accept(visitor);
    }

private:
    std::vector<std::unique_ptr<IMSBuildNode>> m_children;
};

} // namespace qbs

#endif // IMSBUILDNODE_H
