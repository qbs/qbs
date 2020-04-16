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
#include "tst_buildgraph.h"

#include <buildgraph/artifact.h>
#include <buildgraph/buildgraph.h>
#include <buildgraph/cycledetector.h>
#include <buildgraph/productbuilddata.h>
#include <buildgraph/projectbuilddata.h>
#include <language/language.h>
#include <logging/logger.h>
#include <tools/error.h>

#include "../shared/logging/consolelogger.h"

#include <QtTest/qtest.h>

#include <memory>

using namespace qbs;
using namespace qbs::Internal;

const TopLevelProjectPtr project = TopLevelProject::create();

TestBuildGraph::TestBuildGraph(ILogSink *logSink) : m_logSink(logSink)
{
    project->buildData = std::make_unique<ProjectBuildData>();
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
    product->buildData = std::make_unique<ProductBuildData>();
    const auto root = new Artifact;
    root->product = product;
    const auto child = new Artifact;
    child->product = product;
    product->buildData->addRootNode(root);
    product->buildData->addNode(root);
    product->buildData->addNode(child);
    qbs::Internal::connect(root, child);
    qbs::Internal::connect(child, root);
    return product;
}

ResolvedProductConstPtr TestBuildGraph::productWithLessDirectCycle()
{
    const ResolvedProductPtr product = ResolvedProduct::create();
    product->project = project;
    product->buildData = std::make_unique<ProductBuildData>();
    const auto root = new Artifact;
    const auto child = new Artifact;
    const auto grandchild = new Artifact;
    root->product = product;
    child->product = product;
    grandchild->product = product;
    product->buildData->addRootNode(root);
    product->buildData->addNode(root);
    product->buildData->addNode(child);
    product->buildData->addNode(grandchild);
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
    product->buildData = std::make_unique<ProductBuildData>();
    const auto root = new Artifact;
    const auto root2 = new Artifact;
    root->product = product;
    root2->product = product;
    product->buildData->addRootNode(root);
    product->buildData->addRootNode(root2);
    product->buildData->addNode(root);
    product->buildData->addNode(root2);
    qbs::Internal::connect(root2, root);
    return product;
}

void TestBuildGraph::testCycle()
{
    QVERIFY(cycleDetected(productWithDirectCycle()));
    QVERIFY(cycleDetected(productWithLessDirectCycle()));
    QVERIFY(!cycleDetected(productWithNoCycle()));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    TestBuildGraph tbg(ConsoleLogger::instance().logSink());
    return QTest::qExec(&tbg, argc, argv);
}
