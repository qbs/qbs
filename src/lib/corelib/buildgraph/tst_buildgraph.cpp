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
#include "tst_buildgraph.h"

#include <buildgraph/artifact.h>
#include <buildgraph/buildgraph.h>
#include <buildgraph/cycledetector.h>
#include <buildgraph/productbuilddata.h>
#include <buildgraph/projectbuilddata.h>
#include <language/language.h>
#include <logging/logger.h>
#include <tools/error.h>

#include <QtTest>

namespace qbs {
namespace Internal {

const TopLevelProjectPtr project = TopLevelProject::create();

TestBuildGraph::TestBuildGraph(ILogSink *logSink) : m_logSink(logSink)
{
    project->buildData.reset(new ProjectBuildData);
}

void TestBuildGraph::initTestCase()
{
}

void TestBuildGraph::cleanupTestCase()
{
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
    const ResolvedProductPtr product = ResolvedProduct::create();
    product->project = project;
    product->buildData.reset(new ProductBuildData);
    Artifact * const root = new Artifact;
    root->product = product;
    Artifact * const child = new Artifact;
    child->product = product;
    product->buildData->roots.insert(root);
    product->buildData->nodes << root << child;
    qbs::Internal::connect(root, child);
    qbs::Internal::connect(child, root);
    return product;
}

ResolvedProductConstPtr TestBuildGraph::productWithLessDirectCycle()
{
    const ResolvedProductPtr product = ResolvedProduct::create();
    product->project = project;
    product->buildData.reset(new ProductBuildData);
    Artifact * const root = new Artifact;
    Artifact * const child = new Artifact;
    Artifact * const grandchild = new Artifact;
    root->product = product;
    child->product = product;
    grandchild->product = product;
    product->buildData->roots << root;
    product->buildData->nodes << root << child << grandchild;
    qbs::Internal::connect(root, child);
    qbs::Internal::connect(child, grandchild);
    qbs::Internal::connect(grandchild, root);
    return product;
}

// root appears as a child, but in a different tree
ResolvedProductConstPtr TestBuildGraph::productWithNoCycle()
{
    const ResolvedProductPtr product = ResolvedProduct::create();
    product->project = project;
    product->buildData.reset(new ProductBuildData);
    Artifact * const root = new Artifact;
    Artifact * const root2 = new Artifact;
    root->product = product;
    root2->product = product;
    product->buildData->roots << root << root2;
    product->buildData->nodes << root << root2;
    qbs::Internal::connect(root2, root);
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
