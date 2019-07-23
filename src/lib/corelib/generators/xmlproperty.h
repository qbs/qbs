/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
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

#ifndef GENERATORS_XML_PROPERTY_H
#define GENERATORS_XML_PROPERTY_H

#include <tools/qbs_export.h>

#include <QtCore/qvariant.h>

#include <memory>

namespace qbs {
namespace gen {
namespace xml {

class INodeVisitor;

class QBS_EXPORT Property
{
    Q_DISABLE_COPY(Property)
public:
    Property() = default;
    explicit Property(QByteArray name, QVariant value);
    virtual ~Property() = default;

    QByteArray name() const { return m_name; }
    void setName(QByteArray name) { m_name = std::move(name); }

    QVariant value() const { return m_value; }
    void setValue(QVariant value) { m_value = std::move(value); }

    template<class T>
    T *appendChild(std::unique_ptr<T> child) {
        const auto p = child.get();
        m_children.push_back(std::move(child));
        return p;
    }

    template<class T, class... Args>
    T *appendChild(Args&&... args) {
        return appendChild(std::make_unique<T>(std::forward<Args>(args)...));
    }

    virtual void accept(INodeVisitor *visitor) const;

protected:
    const std::vector<std::unique_ptr<Property>> &children() const
    { return m_children; }

private:
    QByteArray m_name;
    QVariant m_value;
    std::vector<std::unique_ptr<Property>> m_children;
};

} // namespace xml
} // namespace gen
} // namespace qbs

#endif // GENERATORS_XML_PROPERTY_H
