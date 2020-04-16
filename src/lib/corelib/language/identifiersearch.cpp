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

#include "identifiersearch.h"
#include <parser/qmljsast_p.h>

namespace qbs {
namespace Internal {

IdentifierSearch::IdentifierSearch() = default;

void IdentifierSearch::start(QbsQmlJS::AST::Node *node)
{
    for (auto it = m_requests.cbegin(); it != m_requests.cend(); ++it)
        *it.value() = false;
    m_numberOfFoundIds = 0;
    node->accept(this);
}

void IdentifierSearch::add(const QString &name, bool *found)
{
    m_requests.insert(name, found);
}

bool IdentifierSearch::preVisit(QbsQmlJS::AST::Node *)
{
    return m_numberOfFoundIds < m_requests.size();
}

bool IdentifierSearch::visit(QbsQmlJS::AST::IdentifierExpression *e)
{
    bool *found = m_requests.value(e->name.toString());
    if (found && !*found) {
        *found = true;
        m_numberOfFoundIds++;
    }
    return m_numberOfFoundIds < m_requests.size();
}

} // namespace Internal
} // namespace qbs
