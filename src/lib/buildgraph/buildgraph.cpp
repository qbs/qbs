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
#include "buildgraph.h"

#include "artifact.h"
#include "buildproject.h"
#include "rulesevaluationcontext.h"

#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/logger.h>
#include <tools/fileinfo.h>

#include <QFile>

namespace qbs {
namespace Internal {

void setupScriptEngineForProduct(ScriptEngine *engine, const ResolvedProductConstPtr &product,
                                 const RuleConstPtr &rule, QScriptValue targetObject)
{
    const ResolvedProject *lastSetupProject
            = reinterpret_cast<ResolvedProject *>(engine->property("lastSetupProject").toULongLong());
    const ResolvedProduct *lastSetupProduct
            = reinterpret_cast<ResolvedProduct *>(engine->property("lastSetupProduct").toULongLong());

    if (lastSetupProject != product->project) {
        engine->setProperty("lastSetupProject",
                QVariant(reinterpret_cast<qulonglong>(product->project.data())));
        QScriptValue projectScriptValue;
        projectScriptValue = engine->newObject();
        projectScriptValue.setProperty("filePath", product->project->location.fileName);
        projectScriptValue.setProperty("path", FileInfo::path(product->project->location.fileName));
        targetObject.setProperty("project", projectScriptValue);
    }

    QScriptValue productScriptValue;
    if (lastSetupProduct != product.data()) {
        engine->setProperty("lastSetupProduct",
                QVariant(reinterpret_cast<qulonglong>(product.data())));
        {
            QVariant v;
            v.setValue<void*>(&product->buildEnvironment);
            engine->setProperty("_qbs_procenv", v);
        }
        productScriptValue = product->properties->toScriptValue(engine);
        productScriptValue.setProperty("name", product->name);
        QString destinationDirectory = product->destinationDirectory;
        if (destinationDirectory.isEmpty())
            destinationDirectory = ".";
        productScriptValue.setProperty("destinationDirectory", destinationDirectory);
        targetObject.setProperty("product", productScriptValue);
    } else {
        productScriptValue = targetObject.property("product");
    }

    // If the Rule is in a Module, set up the 'module' property
    if (!rule->module->name.isEmpty()) {
        productScriptValue.setProperty("module",
                productScriptValue.property("modules").property(rule->module->name));
    }

    engine->import(rule->jsImports, targetObject, targetObject);
}

bool findPath(Artifact *u, Artifact *v, QList<Artifact*> &path)
{
    if (u == v) {
        path.append(v);
        return true;
    }

    for (ArtifactList::const_iterator it = u->children.begin(); it != u->children.end(); ++it) {
        if (findPath(*it, v, path)) {
            path.prepend(u);
            return true;
        }
    }

    return false;
}

/*
 *  c must be built before p
 *  p ----> c
 *  p.children = c
 *  c.parents = p
 *
 * also:  children means i depend on or i am produced by
 *        parent means "produced by me" or "depends on me"
 */
void connect(Artifact *p, Artifact *c)
{
    Q_ASSERT(p != c);
    p->children.insert(c);
    c->parents.insert(p);
    p->project->markDirty();
}

void loggedConnect(Artifact *u, Artifact *v, const Logger &logger)
{
    Q_ASSERT(u != v);
    if (logger.traceEnabled()) {
        logger.qbsTrace() << QString::fromLocal8Bit("[BG] connect '%1' -> '%2'")
                             .arg(relativeArtifactFileName(u), relativeArtifactFileName(v));
    }
    connect(u, v);
}

static bool existsPath(Artifact *u, Artifact *v)
{
    if (u == v)
        return true;

    for (ArtifactList::const_iterator it = u->children.begin(); it != u->children.end(); ++it)
        if (existsPath(*it, v))
            return true;

    return false;
}

bool safeConnect(Artifact *u, Artifact *v, const Logger &logger)
{
    Q_ASSERT(u != v);
    if (logger.traceEnabled()) {
        logger.qbsTrace() << QString::fromLocal8Bit("[BG] safeConnect: '%1' '%2'")
                             .arg(relativeArtifactFileName(u), relativeArtifactFileName(v));
    }

    if (existsPath(v, u)) {
        QList<Artifact *> circle;
        findPath(v, u, circle);
        logger.qbsTrace() << "[BG] safeConnect: circle detected " << toStringList(circle);
        return false;
    }

    connect(u, v);
    return true;
}

void disconnect(Artifact *u, Artifact *v, const Logger &logger)
{
    if (logger.traceEnabled()) {
        logger.qbsTrace() << QString::fromLocal8Bit("[BG] disconnect: '%1' '%2'")
                             .arg(relativeArtifactFileName(u), relativeArtifactFileName(v));
    }
    u->children.remove(v);
    v->parents.remove(u);
}

void removeGeneratedArtifactFromDisk(Artifact *artifact, const Logger &logger)
{
    if (artifact->artifactType != Artifact::Generated)
        return;

    QFile file(artifact->filePath());
    if (!file.exists())
        return;

    logger.qbsDebug() << "removing " << artifact->fileName();
    if (!file.remove()) {
        logger.qbsWarning() << QString::fromLocal8Bit("Cannot remove '%1'.")
                               .arg(artifact->filePath());
    }
}

QString relativeArtifactFileName(const Artifact *n)
{
    const QString &buildDir = n->project->resolvedProject()->buildDirectory;
    QString str = n->filePath();
    if (str.startsWith(buildDir))
        str.remove(0, buildDir.count());
    if (str.startsWith('/'))
        str.remove(0, 1);
    return str;
}

} // namespace Internal
} // namespace qbs
