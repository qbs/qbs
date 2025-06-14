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

#include "graphvizgenerator.h"
#include "dotgraph.h"

#include <logging/logger.h>
#include <tools/stlutils.h>

#include <deque>
#include <set>

namespace qbs {

using namespace Internal;

static DotGraphNode makeProjectNode(size_t node, const ProjectData &project)
{
    DotGraphNode result;
    result.id = node;
    result.label = project.name();
    result.enabled = true;
    result.type = DotGraphNode::Type::Project;
    return result;
}

static DotGraphNode makeProductNode(size_t node, const ProductData &product)
{
    DotGraphNode result;
    result.id = node;
    result.label = product.name();
    result.enabled = product.isEnabled();
    result.type = DotGraphNode::Type::Product;
    return result;
}

static DotGraphNode makeModuleNode(size_t node, const QString &module)
{
    DotGraphNode result;
    result.id = node;
    result.label = module;
    result.type = DotGraphNode::Type::Module;
    return result;
}

static std::unordered_map<QString, ProductData> createProductMap(const QList<ProductData> &products)
{
    std::unordered_map<QString, ProductData> result;
    for (const auto &product : products)
        result[product.name()] = product;
    return result;
}

static QString getFileName(const ProjectData &projectData, const QString &suffix)
{
    return projectData.buildDirectory() + QStringLiteral("/graphviz/") + projectData.name() + suffix
           + QStringLiteral(".dot");
}

QString GraphvizGenerator::generatorName() const
{
    return QStringLiteral("graphviz");
}

void GraphvizGenerator::generate()
{
    const auto projects = project().projects.values();
    for (const Project &theProject : projects) {
        writeProjectsGraph(theProject.projectData());
        writeProductsGraph(theProject.projectData());
    }
}

void GraphvizGenerator::writeProjectsGraph(const ProjectData &projectData)
{
    DotGraph dotGraph;

    std::unordered_map<QString, size_t> projectToNode;
    std::unordered_map<QString /*project*/, QString /*parent*/> projectParentMap;
    iterateProjects(projectData, [&](const ProjectData &currentProjectData) {
        projectToNode[currentProjectData.name()] = dotGraph.addNode(
            makeProjectNode(dotGraph.createNodeId(), currentProjectData));

        const auto subProjects = currentProjectData.subProjects();
        for (const auto &subProject : subProjects)
            projectParentMap[subProject.name()] = currentProjectData.name();
    });

    std::unordered_map<QString, size_t> productToNode;
    std::unordered_map<QString /*product*/, QString /*parent*/> productParentMap;
    iterateProjects(projectData, [&](const ProjectData &currentProjectData) {
        const auto &products = currentProjectData.products();
        for (const auto &product : products) {
            productToNode[product.name()] = dotGraph.addNode(
                makeProductNode(dotGraph.createNodeId(), product));
            productParentMap[product.name()] = currentProjectData.name();
        }
    });

    for (const auto &item : projectParentMap) {
        const auto childNode = projectToNode[item.first];
        const auto parentNode = projectToNode[item.second];
        dotGraph.addRelation({parentNode, childNode});
    }

    for (const auto &item : productParentMap) {
        const auto childNode = productToNode[item.first];
        const auto parentNode = projectToNode[item.second];
        dotGraph.addRelation({parentNode, childNode});
    }

    const QString graphFilePath = getFileName(projectData, QLatin1String(".projects"));
    dotGraph.write(graphFilePath);
}

void GraphvizGenerator::writeProductsGraph(const ProjectData &projectData)
{
    DotGraph dotGraph;

    const auto allProducts = projectData.allProducts();
    const auto productMap = createProductMap(allProducts);

    std::unordered_map<QString, size_t> productToNode;
    for (const auto &product : allProducts) {
        productToNode[product.name()] = dotGraph.addNode(
            makeProductNode(dotGraph.createNodeId(), product));
    }

    for (const auto &product : allProducts) {
        const auto parentNode = productToNode[product.name()];
        for (const auto &dep : product.dependencies()) {
            const auto it = productMap.find(dep);
            if (it == productMap.end())
                continue;
            const auto childProduct = it->second;
            if (product.type().contains(QStringLiteral("autotest-result"))
                && childProduct.type().contains(QStringLiteral("autotest"))) {
                continue;
            }
            const auto childNode = productToNode[dep];
            dotGraph.addRelation({parentNode, childNode});
        }
    }

    dotGraph.write(getFileName(projectData, QLatin1String(".products")));

    for (const auto &product : allProducts) {
        writeProductGraph(projectData, product, productMap);
    }
}

void GraphvizGenerator::writeProductGraph(
    const ProjectData &projectData,
    const ProductData &product,
    const std::unordered_map<QString, ProductData> &productMap)
{
    DotGraph dotGraph;

    std::unordered_map<QString, size_t> productToNode;

    std::set<QString> deps;
    iterateProducts(product, productMap, [&](const ProductData &currentProduct) {
        deps.insert(currentProduct.name());
    });

    for (const auto &dep : deps) {
        productToNode[dep] = dotGraph.addNode(
            makeProductNode(dotGraph.createNodeId(), productMap.at(dep)));
    }
    for (const auto &productName : deps) {
        const auto currentProduct = productMap.at(productName);
        const auto parentNode = productToNode[productName];
        for (const auto &dep : currentProduct.dependencies()) {
            const auto childNode = productToNode[dep];
            dotGraph.addRelation({parentNode, childNode});
        }
    }

    const auto productNode = dotGraph.addNode(
        makeProductNode(dotGraph.createNodeId(), productMap.at(product.name())));
    for (const auto &module : product.moduleProperties().allModules()) {
        const auto childNode = dotGraph.addNode(makeModuleNode(dotGraph.createNodeId(), module));
        dotGraph.addRelation({productNode, childNode});
    }

    const auto fileTagsString = [](const ArtifactData &artifact) {
        return QStringLiteral("[%1]").arg(artifact.fileTags().join(QLatin1String(", ")));
    };

    const auto productNode2 = dotGraph.addNode(
        makeProductNode(dotGraph.createNodeId(), productMap.at(product.name())));

    std::unordered_map<QString, size_t> artifactToNode;
    std::unordered_map<QString, ArtifactData> artifactMap;

    for (const auto &group : product.groups()) {
        for (const auto &artifact : group.allSourceArtifacts()) {
            artifactMap[artifact.filePath()] = artifact;
        }
    }

    std::deque<std::pair<ArtifactData, size_t /*node*/>> artifactQueue;
    for (const auto &artifact : product.generatedArtifacts()) {
        artifactMap[artifact.filePath()] = artifact;
        if (artifact.isTargetArtifact())
            artifactQueue.push_back({artifact, productNode2});
    }

    // Traverse artifacts starting from target artifacts down to their dependencies.
    // This way we create nodes only for artifacts reachable from the top of the graph.
    // Otherwise, graph becomes too complicated.
    while (!artifactQueue.empty()) {
        const auto &[artifact, parentNodeId] = artifactQueue.front();
        const auto currentPath = artifact.filePath();

        auto &nodeId = artifactToNode[currentPath];
        if (nodeId) {
            dotGraph.addRelation({parentNodeId, nodeId});
            artifactQueue.pop_front();
            continue;
        }
        DotGraphNode node;
        node.id = dotGraph.createNodeId();
        node.label = QFileInfo(currentPath).fileName() + QLatin1String("\n")
                     + fileTagsString(artifact);
        node.type = artifact.isGenerated() ? DotGraphNode::Type::Artifact
                                           : DotGraphNode::Type::File;
        nodeId = dotGraph.addNode(std::move(node));

        dotGraph.addRelation({parentNodeId, nodeId});

        for (const auto &childPath : artifact.childPaths()) {
            auto it = artifactMap.find(childPath);
            if (it != artifactMap.end()) {
                artifactQueue.push_back({it->second, nodeId});
            }
        }
        artifactQueue.pop_front();
    }

    const QString graphFilePath = getFileName(
        projectData, QLatin1String(".products/") + product.name());
    dotGraph.write(graphFilePath);
}

void GraphvizGenerator::iterateProjects(
    const ProjectData &projectData, std::function<void(const ProjectData &)> func)
{
    std::deque<ProjectData> queue;
    queue.push_back(projectData);
    while (!queue.empty()) {
        const auto size = queue.size();
        for (size_t i = 0; i < size; ++i) {
            const auto &currentProjectData = queue[i];
            func(currentProjectData);
            const auto subProjects = currentProjectData.subProjects();
            std::copy(subProjects.begin(), subProjects.end(), std::back_inserter(queue));
        }
        queue.erase(queue.begin(), queue.begin() + size);
    }
}

void GraphvizGenerator::iterateProducts(
    const ProductData &product,
    const std::unordered_map<QString, ProductData> &productMap,
    std::function<void(const ProductData &)> func)
{
    std::deque<ProductData> productQueue{product};
    while (!productQueue.empty()) {
        const auto &currentProduct = productQueue.front();
        func(currentProduct);
        for (const auto &dep : currentProduct.dependencies()) {
            const auto it = productMap.find(dep);
            if (it != productMap.end())
                productQueue.push_back(it->second);
        }
        productQueue.pop_front();
    }
}

} // namespace qbs
