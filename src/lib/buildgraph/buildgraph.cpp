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
#include "projectbuilddata.h"
#include "productbuilddata.h"

#include <jsextensions/jsextensions.h>
#include <jsextensions/moduleproperties.h>
#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/qbsassert.h>

#include <QFile>

namespace qbs {
namespace Internal {

static void setupProductScriptValue(ScriptEngine *engine, QScriptValue &productScriptValue,
                                    const ResolvedProductConstPtr &product,
                                    ScriptPropertyObserver *observer);

class DependenciesFunction
{
public:
    DependenciesFunction(ScriptEngine *engine)
        : m_engine(engine)
    {
    }

    void init(QScriptValue &productScriptValue, const ResolvedProductConstPtr &product)
    {
        QScriptValue depfunc = m_engine->newFunction(&js_productDependencies);
        setProduct(depfunc, product.data());
        QScriptValue descriptor = m_engine->newObject();
        descriptor.setProperty(QLatin1String("get"), depfunc);
        descriptor.setProperty(QLatin1String("set"), m_engine->evaluate("(function(){})"));
        descriptor.setProperty(QLatin1String("enumerable"), true);
        m_engine->defineProperty(productScriptValue, QLatin1String("dependencies"), descriptor);
    }

private:
    static QScriptValue js_productDependencies(QScriptContext *context, QScriptEngine *engine)
    {
        QScriptValue callee = context->callee();
        const ResolvedProduct * const product = getProduct(callee);
        QScriptValue result = engine->newArray();
        quint32 idx = 0;
        foreach (const ResolvedProductPtr &dependency, product->dependencies) {
            QScriptValue obj = engine->newObject();
            setupProductScriptValue(static_cast<ScriptEngine *>(engine), obj, dependency, 0);
            result.setProperty(idx++, obj);
        }
        foreach (const ResolvedModuleConstPtr &dependency, product->modules) {
            QScriptValue obj = engine->newObject();
            setupModuleScriptValue(static_cast<ScriptEngine *>(engine), obj,
                                   product->properties->value(), dependency->name);
            result.setProperty(idx++, obj);
        }
        return result;
    }

    static QScriptValue js_moduleDependencies(QScriptContext *context, QScriptEngine *engine)
    {
        QScriptValue callee = context->callee();
        const QVariantMap modulesMap = callee.data().toVariant().toMap();
        QScriptValue result = engine->newArray();
        quint32 idx = 0;
        for (QVariantMap::const_iterator it = modulesMap.begin(); it != modulesMap.end(); ++it) {
            QScriptValue obj = engine->newObject();
            obj.setProperty(QLatin1String("name"), it.key());
            setupModuleScriptValue(static_cast<ScriptEngine *>(engine), obj, it.value().toMap(),
                                   it.key());
            result.setProperty(idx++, obj);
        }
        return result;
    }

    static void setupModuleScriptValue(ScriptEngine *engine, QScriptValue &moduleScriptValue,
                                       const QVariantMap &propertyMap,
                                       const QString &moduleName)
    {
        const QVariantMap propMap
                = propertyMap.value(QLatin1String("modules")).toMap().value(moduleName).toMap();
        for (QVariantMap::ConstIterator it = propMap.constBegin(); it != propMap.constEnd(); ++it) {
            const QVariant &value = it.value();
            if (value.isValid()
                    && (!value.canConvert<QVariantMap>() || it.key() != QLatin1String("modules")))
                moduleScriptValue.setProperty(it.key(), engine->toScriptValue(value));
        }
        QScriptValue depfunc = engine->newFunction(&js_moduleDependencies);
        depfunc.setData(engine->toScriptValue(propMap.value(QLatin1String("modules"))));
        QScriptValue descriptor = engine->newObject();
        descriptor.setProperty(QLatin1String("get"), depfunc);
        descriptor.setProperty(QLatin1String("set"), engine->evaluate("(function(){})"));
        descriptor.setProperty(QLatin1String("enumerable"), true);
        engine->defineProperty(moduleScriptValue, QLatin1String("dependencies"), descriptor);
        moduleScriptValue.setProperty(QLatin1String("type"), QLatin1String("module"));
    }

    static void setProduct(QScriptValue scriptValue, const ResolvedProduct *product)
    {
        QVariant v;
        v.setValue<quintptr>(reinterpret_cast<quintptr>(product));
        scriptValue.setData(scriptValue.engine()->newVariant(v));
    }

    static const ResolvedProduct *getProduct(const QScriptValue &scriptValue)
    {
        const quintptr ptr = scriptValue.data().toVariant().value<quintptr>();
        return reinterpret_cast<const ResolvedProduct *>(ptr);
    }

    ScriptEngine *m_engine;
};

static void setupProductScriptValue(ScriptEngine *engine, QScriptValue &productScriptValue,
                                    const ResolvedProductConstPtr &product,
                                    ScriptPropertyObserver *observer)
{
    ModuleProperties::init(productScriptValue, product);
    DependenciesFunction(engine).init(productScriptValue, product);
    const QVariantMap &propMap = product->properties->value();
    for (QVariantMap::ConstIterator it = propMap.constBegin(); it != propMap.constEnd(); ++it) {
        const QVariant &value = it.value();
        if (value.isValid()
                && (!value.canConvert<QVariantMap>() || it.key() != QLatin1String("modules")))
            engine->setObservedProperty(productScriptValue, it.key(),
                                        engine->toScriptValue(value), observer);
    }
}

void setupScriptEngineForProduct(ScriptEngine *engine, const ResolvedProductConstPtr &product,
                                 const RuleConstPtr &rule, QScriptValue targetObject,
                                 ScriptPropertyObserver *observer)
{
    ScriptEngine::ScriptValueCache * const cache = engine->scriptValueCache();
    if (cache->observer != observer) {
        cache->project = 0;
        cache->product = 0;
    }

    if (cache->project != product->project) {
        cache->project = product->project.data();
        cache->projectScriptValue = engine->newObject();
        cache->projectScriptValue.setProperty(QLatin1String("filePath"),
                product->project->location.fileName());
        cache->projectScriptValue.setProperty(QLatin1String("path"),
                FileInfo::path(product->project->location.fileName()));
        const QVariantMap &projectProperties = product->project->projectProperties();
        for (QVariantMap::const_iterator it = projectProperties.begin();
                it != projectProperties.end(); ++it)
            cache->projectScriptValue.setProperty(it.key(), engine->toScriptValue(it.value()));
    }
    targetObject.setProperty(QLatin1String("project"), cache->projectScriptValue);

    if (cache->product != product) {
        cache->product = product.data();
        {
            QVariant v;
            v.setValue<void*>(&product->buildEnvironment);
            engine->setProperty("_qbs_procenv", v);
        }
        cache->productScriptValue = engine->newObject();
        setupProductScriptValue(engine, cache->productScriptValue, product, observer);
    }
    targetObject.setProperty(QLatin1String("product"), cache->productScriptValue);

    // If the Rule is in a Module, set up the 'moduleName' property
    cache->productScriptValue.setProperty(QLatin1String("moduleName"),
            rule->module->name.isEmpty() ? QScriptValue() : rule->module->name);

    engine->import(rule->jsImports, targetObject, targetObject);
    JsExtensions::setupExtensions(rule->jsExtensions, targetObject);
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
    QBS_CHECK(p != c);
    p->children.insert(c);
    c->parents.insert(p);
    p->product->topLevelProject()->buildData->isDirty = true;
}

void loggedConnect(Artifact *u, Artifact *v, const Logger &logger)
{
    QBS_CHECK(u != v);
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
    QBS_CHECK(u != v);
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

QString relativeArtifactFileName(const Artifact *artifact)
{
    const QString &buildDir = artifact->product->topLevelProject()->buildDirectory;
    QString str = artifact->filePath();
    if (str.startsWith(buildDir))
        str.remove(0, buildDir.count());
    if (str.startsWith('/'))
        str.remove(0, 1);
    return str;
}

Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const QString &dirPath,
                         const QString &fileName)
{
    const QList<FileResourceBase *> lookupResults
            = product->topLevelProject()->buildData->lookupFiles(dirPath, fileName);
    for (QList<FileResourceBase *>::const_iterator it = lookupResults.constBegin();
            it != lookupResults.constEnd(); ++it) {
        Artifact *artifact = dynamic_cast<Artifact *>(*it);
        if (artifact && artifact->product == product)
            return artifact;
    }
    return 0;
}

Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const QString &filePath)
{
    QString dirPath, fileName;
    FileInfo::splitIntoDirectoryAndFileName(filePath, &dirPath, &fileName);
    return lookupArtifact(product, dirPath, fileName);
}

Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const Artifact *artifact)
{
    return lookupArtifact(product, artifact->dirPath(), artifact->fileName());
}

Artifact *createArtifact(const ResolvedProductPtr &product,
                         const SourceArtifactConstPtr &sourceArtifact, const Logger &logger)
{
    Artifact *artifact = new Artifact;
    artifact->artifactType = Artifact::SourceFile;
    artifact->setFilePath(sourceArtifact->absoluteFilePath);
    artifact->fileTags = sourceArtifact->fileTags;
    artifact->properties = sourceArtifact->properties;
    insertArtifact(product, artifact, logger);
    return artifact;
}

void insertArtifact(const ResolvedProductPtr &product, Artifact *artifact, const Logger &logger)
{
    QBS_CHECK(!artifact->product);
    QBS_CHECK(!artifact->filePath().isEmpty());
    QBS_CHECK(!product->buildData->artifacts.contains(artifact));
#ifdef QT_DEBUG
    foreach (const ResolvedProductConstPtr &otherProduct, product->project->products) {
        if (lookupArtifact(otherProduct, artifact->filePath())) {
            if (artifact->artifactType == Artifact::Generated) {
                QString pl;
                pl.append(QString("  - %1 \n").arg(product->name));
                foreach (const ResolvedProductConstPtr &p, product->project->products) {
                    if (lookupArtifact(p, artifact->filePath()))
                        pl.append(QString("  - %1 \n").arg(p->name));
                }
                throw ErrorInfo(QString ("BUG: already inserted in this project: %1\n%2")
                            .arg(artifact->filePath()).arg(pl));
            }
        }
    }
#endif
    product->buildData->artifacts.insert(artifact);
    artifact->product = product;
    product->topLevelProject()->buildData->insertIntoLookupTable(artifact);
    product->topLevelProject()->buildData->isDirty = true;

    if (logger.traceEnabled()) {
        logger.qbsTrace() << QString::fromLocal8Bit("[BG] insert artifact '%1'")
                             .arg(artifact->filePath());
    }
}

} // namespace Internal
} // namespace qbs
