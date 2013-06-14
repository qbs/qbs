/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/
#include "tst_buildgraph.h"

#include <buildgraph/artifact.h>
#include <buildgraph/productbuilddata.h>
#include <buildgraph/cycledetector.h>
#include <language/language.h>
#include <logging/logger.h>
#include <tools/error.h>

#include <QtTest>

namespace qbs {
namespace Internal {

TestBuildGraph::TestBuildGraph(ILogSink *logSink) : m_logSink(logSink)
{
}

void TestBuildGraph::initTestCase()
{
}

void TestBuildGraph::cleanupTestCase()
{
    qDeleteAll(m_artifacts);
}


bool TestBuildGraph::cycleDetected(const ResolvedProductConstPtr &product)
{
    try {
        CycleDetector(Logger(m_logSink)).visitProduct(product);
        return false;
    } catch (const ErrorInfo &) {
        return true;
    }
}

ResolvedProductConstPtr TestBuildGraph::productWithDirectCycle()
{
    Artifact * const root = new Artifact;
    Artifact * const child = new Artifact;
    m_artifacts << root << child;
    root->children.insert(child);
    child->children.insert(root);
    const ResolvedProductPtr product = ResolvedProduct::create();
    product->buildData.reset(new ProductBuildData);
    product->buildData->targetArtifacts.insert(root);
    return product;
}

ResolvedProductConstPtr TestBuildGraph::productWithLessDirectCycle()
{
    Artifact * const root = new Artifact;
    Artifact * const child = new Artifact;
    Artifact * const grandchild = new Artifact;
    m_artifacts << root << child << grandchild;
    root->children.insert(child);
    child->children.insert(grandchild);
    grandchild->children.insert(root);
    const ResolvedProductPtr product = ResolvedProduct::create();
    product->buildData.reset(new ProductBuildData);
    product->buildData->targetArtifacts << root;
    return product;
}

// root appears as a child, but in a different tree
ResolvedProductConstPtr TestBuildGraph::productWithNoCycle()
{
    Artifact * const root = new Artifact;
    Artifact * const root2 = new Artifact;
    m_artifacts << root << root2;
    root2->children.insert(root);
    const ResolvedProductPtr product = ResolvedProduct::create();
    product->buildData.reset(new ProductBuildData);
    product->buildData->targetArtifacts << root << root2;
    return product;
}

void TestBuildGraph::testCycle()
{
    QVERIFY(cycleDetected(productWithDirectCycle()));
    QVERIFY(cycleDetected(productWithLessDirectCycle()));
    QVERIFY(!cycleDetected(productWithNoCycle()));
}

} // namespace Internal
} // namespace qbs
