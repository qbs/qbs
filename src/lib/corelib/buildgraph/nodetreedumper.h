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
#ifndef QBS_NODETREEDUMPER_H
#define QBS_NODETREEDUMPER_H

#include "artifact.h"
#include "buildgraphvisitor.h"
#include <language/forward_decls.h>

#include <QtCore/qlist.h>

QT_BEGIN_NAMESPACE
class QIODevice;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {

class NodeTreeDumper : public BuildGraphVisitor
{
public:
    NodeTreeDumper(QIODevice &outDevice);

    void start(const QVector<ResolvedProductPtr> &products);

private:
    bool visit(Artifact *artifact) override;
    void endVisit(Artifact *artifact) override;
    bool visit(RuleNode *rule) override;
    void endVisit(RuleNode *rule) override;

    void doEndVisit();
    void indent();
    void unindent();
    bool doVisit(BuildGraphNode *node, const QString &nodeRepr);
    QByteArray indentation() const;

    QIODevice &m_outDevice;
    ResolvedProductPtr m_currentProduct;
    NodeSet m_visited;
    int m_indentation = 0;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard.
