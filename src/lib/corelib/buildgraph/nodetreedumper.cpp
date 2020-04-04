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

#include "nodetreedumper.h"

#include "artifact.h"
#include "productbuilddata.h"
#include "rulenode.h"

#include <language/language.h>
#include <tools/qbsassert.h>

#include <QtCore/qiodevice.h>

namespace qbs {
namespace Internal {

static int indentWidth() { return 4; }

NodeTreeDumper::NodeTreeDumper(QIODevice &outDevice) : m_outDevice(outDevice)
{
}

void NodeTreeDumper::start(const QVector<ResolvedProductPtr> &products)
{
    m_indentation = 0;
    for (const ResolvedProductPtr &p : products) {
        if (!p->buildData)
            continue;
        m_currentProduct = p;
        for (Artifact * const root : p->buildData->rootArtifacts())
            root->accept(this);
        m_visited.clear();
        QBS_CHECK(m_indentation == 0);
    }
}

bool NodeTreeDumper::visit(Artifact *artifact)
{
    return doVisit(artifact, artifact->fileName());
}

void NodeTreeDumper::endVisit(Artifact *artifact)
{
    Q_UNUSED(artifact);
    doEndVisit();
}

bool NodeTreeDumper::visit(RuleNode *rule)
{
    return doVisit(rule, rule->toString());
}

void NodeTreeDumper::endVisit(RuleNode *rule)
{
    Q_UNUSED(rule);
    doEndVisit();
}

void NodeTreeDumper::doEndVisit()
{
    unindent();
}

void NodeTreeDumper::indent()
{
    m_outDevice.write("\n");
    m_indentation += indentWidth();
}

void NodeTreeDumper::unindent()
{
    m_indentation -= indentWidth();
}

bool NodeTreeDumper::doVisit(BuildGraphNode *node, const QString &nodeRepr)
{
    m_outDevice.write(indentation());
    m_outDevice.write(nodeRepr.toLocal8Bit());
    indent();
    const bool wasVisited = !m_visited.insert(node).second;
    return !wasVisited && node->product == m_currentProduct;
}

QByteArray NodeTreeDumper::indentation() const
{
    return QByteArray(m_indentation, ' ');
}

} // namespace Internal
} // namespace qbs
