/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
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

#include "nodetreedumper.h"

#include "artifact.h"
#include "productbuilddata.h"
#include "rulenode.h"

#include <language/language.h>
#include <tools/qbsassert.h>

#include <QIODevice>

namespace qbs {
namespace Internal {

static unsigned int indentWidth() { return 4; }

NodeTreeDumper::NodeTreeDumper(QIODevice &outDevice) : m_outDevice(outDevice)
{
}

void NodeTreeDumper::start(const QList<ResolvedProductPtr> &products)
{
    m_indentation = 0;
    foreach (const ResolvedProductPtr &p, products) {
        if (!p->buildData)
            continue;
        m_currentProduct = p;
        foreach (Artifact * const root, p->buildData->rootArtifacts())
            root->accept(this);
        m_visited.clear();
        QBS_CHECK(m_indentation == 0);
    }
}

bool NodeTreeDumper::visit(Artifact *artifact)
{
    m_outDevice.write(indentation());
    m_outDevice.write(artifact->fileName().toLocal8Bit());
    indent();
    const bool wasVisited = m_visited.contains(artifact);
    m_visited.insert(artifact);
    return !wasVisited && artifact->product == m_currentProduct;
}

void NodeTreeDumper::endVisit(Artifact *artifact)
{
    Q_UNUSED(artifact);
    doEndVisit();
}

bool NodeTreeDumper::visit(RuleNode *rule)
{
    m_outDevice.write(indentation());
    m_outDevice.write(rule->toString().toLocal8Bit());
    indent();
    return true;
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

QByteArray NodeTreeDumper::indentation() const
{
    return QByteArray(m_indentation, ' ');
}

} // namespace Internal
} // namespace qbs
