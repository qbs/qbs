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
#include "buildproduct.h"

#include "artifact.h"
#include "buildproject.h"
#include "rulegraph.h"
#include "rulesevaluationcontext.h"
#include <language/language.h>
#include <logging/logger.h>
#include <tools/error.h>
#include <tools/persistence.h>
#include <tools/qbsassert.h>

namespace qbs {
namespace Internal {

BuildProduct::BuildProduct()
{
}

BuildProduct::~BuildProduct()
{
    qDeleteAll(artifacts);
}

static void internalDump(const BuildProduct *product, Artifact *artifact, QByteArray indent)
{
    Artifact *artifactInProduct = product->lookupArtifact(artifact->filePath());
    if (artifactInProduct && artifactInProduct != artifact) {
        qFatal("\ntree corrupted. %p ('%s') resolves to %p ('%s')\n",
                artifact,  qPrintable(artifact->filePath()), product->lookupArtifact(artifact->filePath()),
                qPrintable(product->lookupArtifact(artifact->filePath())->filePath()));

    }
    printf("%s", indent.constData());
    printf("Artifact (%p) ", artifact);
    printf("%s%s %s [%s]",
           qPrintable(QString(toString(artifact->buildState).at(0))),
           artifactInProduct ? "" : " SBS",     // SBS == side-by-side artifact from other product
           qPrintable(artifact->filePath()),
           qPrintable(artifact->fileTags.toStringList().join(QLatin1String(", "))));
    printf("\n");
    indent.append("  ");
    foreach (Artifact *child, artifact->children) {
        internalDump(product, child, indent);
    }
}

void BuildProduct::dump() const
{
    foreach (Artifact *artifact, artifacts)
        if (artifact->parents.isEmpty())
            internalDump(this, artifact, QByteArray());
}

const QList<RuleConstPtr> &BuildProduct::topSortedRules() const
{
    if (m_topSortedRules.isEmpty()) {
        FileTags productFileTags = rProduct->fileTags;
        productFileTags += rProduct->additionalFileTags;
        RuleGraph ruleGraph;
        ruleGraph.build(rProduct->rules, productFileTags);
//        ruleGraph.dump();
        m_topSortedRules = ruleGraph.topSorted();
//        int i=0;
//        foreach (RulePtr r, m_topSortedRules)
//            qDebug() << ++i << r->toString() << (void*)r.data();
    }
    return m_topSortedRules;
}

Artifact *BuildProduct::lookupArtifact(const QString &dirPath, const QString &fileName) const
{
    QList<Artifact *> artifacts = project->lookupArtifacts(dirPath, fileName);
    for (QList<Artifact *>::const_iterator it = artifacts.constBegin(); it != artifacts.constEnd(); ++it)
        if ((*it)->product == this)
            return *it;
    return 0;
}

Artifact *BuildProduct::lookupArtifact(const QString &filePath) const
{
    QString dirPath, fileName;
    FileInfo::splitIntoDirectoryAndFileName(filePath, &dirPath, &fileName);
    return lookupArtifact(dirPath, fileName);
}

Artifact *BuildProduct::createArtifact(const SourceArtifactConstPtr &sourceArtifact,
                                       const Logger &logger)
{
    Artifact *artifact = new Artifact(project);
    artifact->artifactType = Artifact::SourceFile;
    artifact->setFilePath(sourceArtifact->absoluteFilePath);
    artifact->fileTags = sourceArtifact->fileTags;
    artifact->properties = sourceArtifact->properties;
    insertArtifact(artifact, logger);
    return artifact;
}

void BuildProduct::insertArtifact(Artifact *artifact, const Logger &logger)
{
    QBS_CHECK(!artifact->product);
    QBS_CHECK(!artifact->filePath().isEmpty());
    QBS_CHECK(!artifacts.contains(artifact));
#ifdef QT_DEBUG
    foreach (const BuildProductPtr &otherProduct, project->buildProducts()) {
        if (otherProduct->lookupArtifact(artifact->filePath())) {
            if (artifact->artifactType == Artifact::Generated) {
                QString pl;
                pl.append(QString("  - %1 \n").arg(rProduct->name));
                foreach (const BuildProductPtr &p, project->buildProducts()) {
                    if (p->lookupArtifact(artifact->filePath()))
                        pl.append(QString("  - %1 \n").arg(p->rProduct->name));
                }
                throw Error(QString ("BUG: already inserted in this project: %1\n%2")
                            .arg(artifact->filePath()).arg(pl));
            }
        }
    }
#endif
    artifacts.insert(artifact);
    artifact->product = this;
    project->insertIntoArtifactLookupTable(artifact);
    project->markDirty();

    if (logger.traceEnabled()) {
        logger.qbsTrace() << QString::fromLocal8Bit("[BG] insert artifact '%1'")
                             .arg(artifact->filePath());
    }
}

void BuildProduct::load(PersistentPool &pool)
{
    // artifacts
    int i;
    pool.stream() >> i;
    artifacts.clear();
    for (; --i >= 0;) {
        Artifact *artifact = pool.idLoad<Artifact>();
        artifacts.insert(artifact);
        artifact->product = this;
    }

    // edges
    for (i = artifacts.count(); --i >= 0;) {
        Artifact *artifact = pool.idLoad<Artifact>();
        int k;
        pool.stream() >> k;
        artifact->parents.clear();
        artifact->parents.reserve(k);
        for (; --k >= 0;)
            artifact->parents.insert(pool.idLoad<Artifact>());

        pool.stream() >> k;
        artifact->children.clear();
        artifact->children.reserve(k);
        for (; --k >= 0;)
            artifact->children.insert(pool.idLoad<Artifact>());

        pool.stream() >> k;
        artifact->fileDependencies.clear();
        artifact->fileDependencies.reserve(k);
        for (; --k >= 0;)
            artifact->fileDependencies.insert(pool.idLoad<Artifact>());
    }

    // other data
    rProduct = pool.idLoadS<ResolvedProduct>();
    pool.loadContainer(targetArtifacts);

    pool.stream() >> i;
    dependencies.clear();
    dependencies.reserve(i);
    for (; --i >= 0;)
        dependencies += pool.idLoadS<BuildProduct>();
}

void BuildProduct::store(PersistentPool &pool) const
{
    pool.stream() << artifacts.count();

    //artifacts
    for (ArtifactList::const_iterator i = artifacts.constBegin(); i != artifacts.constEnd(); i++)
        pool.store(*i);

    // edges
    for (ArtifactList::const_iterator i = artifacts.constBegin(); i != artifacts.constEnd(); i++) {
        Artifact * artifact = *i;
        pool.store(artifact);

        pool.stream() << artifact->parents.count();
        foreach (Artifact * n, artifact->parents)
            pool.store(n);
        pool.stream() << artifact->children.count();
        foreach (Artifact * n, artifact->children)
            pool.store(n);
        pool.stream() << artifact->fileDependencies.count();
        foreach (Artifact * n, artifact->fileDependencies)
            pool.store(n);
    }

    // other data
    pool.store(rProduct);
    pool.storeContainer(targetArtifacts);
    pool.storeContainer(dependencies);
}

} // namespace Internal
} // namespace qbs
