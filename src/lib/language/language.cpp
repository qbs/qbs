/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#include "language.h"
#include "qbsengine.h"
#include <tools/scripttools.h>
#include <QCryptographicHash>
#include <QMutexLocker>
#include <QScriptValue>
#include <algorithm>

QT_BEGIN_NAMESPACE
inline QDataStream& operator>>(QDataStream &stream, qbs::JsImport &jsImport)
{
    stream >> jsImport.scopeName
           >> jsImport.fileNames
           >> jsImport.location;
    return stream;
}

inline QDataStream& operator<<(QDataStream &stream, const qbs::JsImport &jsImport)
{
    return stream << jsImport.scopeName
                  << jsImport.fileNames
                  << jsImport.location;
}
QT_END_NAMESPACE

namespace qbs {

Configuration::Configuration()
{
}

Configuration::Configuration(const Configuration &other)
    : m_value(other.m_value), m_scriptValueCache(other.m_scriptValueCache)
{
}

void Configuration::setValue(const QVariantMap &map)
{
    m_value = map;
    m_scriptValueCache.clear();
}

QScriptValue Configuration::toScriptValue(QScriptEngine *scriptEngine) const
{
    QMutexLocker ml(&m_scriptValueCacheMutex);
    QScriptValue result = m_scriptValueCache.value(scriptEngine);
    Q_ASSERT(!result.isValid() || result.engine() == scriptEngine);
    if (!result.isValid()) {
        result = scriptEngine->toScriptValue(m_value);
        m_scriptValueCache[scriptEngine] = result;
    }
    return result;
}

void Configuration::load(PersistentPool &, QDataStream &s)
{
    s >> m_value;
}

void Configuration::store(PersistentPool &, QDataStream &s) const
{
    s << m_value;
}

void FileTagger::load(PersistentPool &pool, QDataStream &s)
{
    Q_UNUSED(s);
    m_artifactExpression.setPattern(pool.idLoadString());
    m_fileTags = pool.idLoadStringList();
}

void FileTagger::store(PersistentPool &pool, QDataStream &s) const
{
    Q_UNUSED(s);
    pool.storeString(m_artifactExpression.pattern());
    pool.storeStringList(m_fileTags);
}

void SourceArtifact::load(PersistentPool &pool, QDataStream &s)
{
    s >> absoluteFilePath;
    s >> fileTags;
    configuration = pool.idLoadS<Configuration>(s);
}

void SourceArtifact::store(PersistentPool &pool, QDataStream &s) const
{
    s << absoluteFilePath;
    s << fileTags;
    pool.store(configuration);
}

void SourceWildCards::load(PersistentPool &pool, QDataStream &s)
{
    s >> recursive;
    prefix = pool.idLoadString();
    patterns = pool.idLoadStringList();
    excludePatterns = pool.idLoadStringList();
    loadContainerS(files, s, pool);
}

void SourceWildCards::store(PersistentPool &pool, QDataStream &s) const
{
    s << recursive;
    pool.storeString(prefix);
    pool.storeStringList(patterns);
    pool.storeStringList(excludePatterns);
    storeContainer(files, s, pool);
}

QList<SourceArtifact::Ptr> Group::allFiles() const
{
    QList<SourceArtifact::Ptr> lst = files;
    if (wildcards)
        lst.append(wildcards->files);
    return lst;
}

void Group::load(PersistentPool &pool, QDataStream &s)
{
    name = pool.idLoadString();
    loadContainerS(files, s, pool);
    wildcards = pool.idLoadS<SourceWildCards>(s);
    configuration = pool.idLoadS<Configuration>(s);
}

void Group::store(PersistentPool &pool, QDataStream &s) const
{
    pool.storeString(name);
    storeContainer(files, s, pool);
    pool.store(wildcards);
    pool.store(configuration);
}

void RuleArtifact::load(PersistentPool &pool, QDataStream &s)
{
    Q_UNUSED(pool);
    s >> fileScript;
    s >> fileTags;

    int i;
    s >> i;
    bindings.clear();
    bindings.reserve(i);
    Binding binding;
    for (; --i >= 0;) {
        s >> binding.name >> binding.code >> binding.location;
        bindings += binding;
    }
}

void RuleArtifact::store(PersistentPool &pool, QDataStream &s) const
{
    Q_UNUSED(pool);
    s << fileScript;
    s << fileTags;

    s << bindings.count();
    for (int i = bindings.count(); --i >= 0;) {
        const Binding &binding = bindings.at(i);
        s << binding.name << binding.code << binding.location;
    }
}

void RuleScript::load(PersistentPool &, QDataStream &s)
{
    s >> script;
    s >> location;
}

void RuleScript::store(PersistentPool &, QDataStream &s) const
{
    s << script;
    s << location;
}

void ResolvedModule::load(PersistentPool &pool, QDataStream &s)
{
    name = pool.idLoadString();
    moduleDependencies = pool.idLoadStringList();
    setupBuildEnvironmentScript = pool.idLoadString();
    setupRunEnvironmentScript = pool.idLoadString();
    s >> jsImports
      >> setupBuildEnvironmentScript
      >> setupRunEnvironmentScript;
}

void ResolvedModule::store(PersistentPool &pool, QDataStream &s) const
{
    pool.storeString(name);
    pool.storeStringList(moduleDependencies);
    pool.storeString(setupBuildEnvironmentScript);
    pool.storeString(setupRunEnvironmentScript);
    s << jsImports
      << setupBuildEnvironmentScript
      << setupRunEnvironmentScript;
}

QString Rule::toString() const
{
    return "[" + inputs.join(",") + " -> " + outputFileTags().join(",") + "]";
}

QStringList Rule::outputFileTags() const
{
    QStringList result;
    foreach (const RuleArtifact::ConstPtr &artifact, artifacts)
        result.append(artifact->fileTags);
    result.sort();
    std::unique(result.begin(), result.end());
    return result;
}

void Rule::load(PersistentPool &pool, QDataStream &s)
{
    script = pool.idLoadS<RuleScript>(s);
    module = pool.idLoadS<ResolvedModule>(s);
    s   >> jsImports
        >> inputs
        >> usings
        >> explicitlyDependsOn
        >> multiplex
        >> objectId;

    loadContainerS(artifacts, s, pool);
}

void Rule::store(PersistentPool &pool, QDataStream &s) const
{
    pool.store(script);
    pool.store(module);
    s   << jsImports
        << inputs
        << usings
        << explicitlyDependsOn
        << multiplex
        << objectId;

    storeContainer(artifacts, s, pool);
}

ResolvedProduct::ResolvedProduct()
    : project(0)
{
}

QList<SourceArtifact::Ptr> ResolvedProduct::allFiles() const
{
    QList<SourceArtifact::Ptr> lst;
    foreach (const Group::ConstPtr &group, groups)
        lst += group->allFiles();
    return lst;
}

QSet<QString> ResolvedProduct::fileTagsForFileName(const QString &fileName) const
{
    QSet<QString> result;
    foreach (FileTagger::ConstPtr tagger, fileTaggers) {
        if (FileInfo::globMatches(tagger->artifactExpression(), fileName)) {
            result.unite(tagger->fileTags().toSet());
        }
    }
    return result;
}

void ResolvedProduct::load(PersistentPool &pool, QDataStream &s)
{
    s   >> fileTags
        >> name
        >> targetName
        >> buildDirectory
        >> sourceDirectory
        >> destinationDirectory
        >> qbsFile;

    configuration = pool.idLoadS<Configuration>(s);
    loadContainerS(rules, s, pool);
    loadContainerS(uses, s, pool);
    loadContainerS(fileTaggers, s, pool);
    loadContainerS(modules, s, pool);
    loadContainerS(groups, s, pool);
}

void ResolvedProduct::store(PersistentPool &pool, QDataStream &s) const
{
    s   << fileTags
        << name
        << targetName
        << buildDirectory
        << sourceDirectory
        << destinationDirectory
        << qbsFile;

    pool.store(configuration);
    storeContainer(rules, s, pool);
    storeContainer(uses, s, pool);
    storeContainer(fileTaggers, s, pool);
    storeContainer(modules, s, pool);
    storeContainer(groups, s, pool);
}

QList<const ResolvedModule*> topSortModules(const QHash<const ResolvedModule*, QList<const ResolvedModule*> > &moduleChildren,
                                      const QList<const ResolvedModule*> &modules,
                                      QSet<QString> &seenModuleNames)
{
    QList<const ResolvedModule*> result;
    foreach (const ResolvedModule *m, modules) {
        if (m->name.isNull())
            continue;
        result.append(topSortModules(moduleChildren, moduleChildren.value(m), seenModuleNames));
        if (!seenModuleNames.contains(m->name)) {
            seenModuleNames.insert(m->name);
            result.append(m);
        }
    }
    return result;
}

static QScriptValue js_getenv(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() < 1)
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("getenv expects 1 argument"));
    QVariant v = engine->property("_qbs_procenv");
    QProcessEnvironment *procenv = reinterpret_cast<QProcessEnvironment*>(v.value<void*>());
    return engine->toScriptValue(procenv->value(context->argument(0).toString()));
}

static QScriptValue js_putenv(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() < 2)
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("putenv expects 2 arguments"));
    QVariant v = engine->property("_qbs_procenv");
    QProcessEnvironment *procenv = reinterpret_cast<QProcessEnvironment*>(v.value<void*>());
    procenv->insert(context->argument(0).toString(), context->argument(1).toString());
    return engine->undefinedValue();
}

enum EnvType
{
    BuildEnv, RunEnv
};

static QProcessEnvironment getProcessEnvironment(QbsEngine *engine, EnvType envType,
                                                 const QList<ResolvedModule::ConstPtr> &modules,
                                                 const Configuration::ConstPtr &productConfiguration,
                                                 ResolvedProject *project,
                                                 const QProcessEnvironment &systemEnvironment)
{
    QProcessEnvironment procenv = systemEnvironment;

    // Copy the environment of the platform configuration to the process environment.
    const QVariantMap &platformEnv = project->platformEnvironment;
    for (QVariantMap::const_iterator it = platformEnv.constBegin(); it != platformEnv.constEnd(); ++it)
        procenv.insert(it.key(), it.value().toString());

    QMap<QString, const ResolvedModule *> moduleMap;
    foreach (const ResolvedModule::ConstPtr &module, modules)
        moduleMap.insert(module->name, module.data());

    QHash<const ResolvedModule*, QList<const ResolvedModule*> > moduleParents;
    QHash<const ResolvedModule*, QList<const ResolvedModule*> > moduleChildren;
    foreach (ResolvedModule::ConstPtr module, modules) {
        foreach (const QString &moduleName, module->moduleDependencies) {
            const ResolvedModule * const depmod = moduleMap.value(moduleName);
            moduleParents[depmod].append(module.data());
            moduleChildren[module.data()].append(depmod);
        }
    }

    QList<const ResolvedModule *> rootModules;
    foreach (ResolvedModule::ConstPtr module, modules)
        if (moduleParents.value(module.data()).isEmpty())
            rootModules.append(module.data());

    {
        QVariant v;
        v.setValue<void*>(&procenv);
        engine->setProperty("_qbs_procenv", v);
    }

    engine->clearImportsCache();
    QScriptValue scope = engine->newObject();
    scope.setProperty("getenv", engine->newFunction(js_getenv, 1));
    scope.setProperty("putenv", engine->newFunction(js_putenv, 2));

    QSet<QString> seenModuleNames;
    QList<const ResolvedModule *> topSortedModules = topSortModules(moduleChildren, rootModules, seenModuleNames);
    foreach (const ResolvedModule *module, topSortedModules) {
        if ((envType == BuildEnv && module->setupBuildEnvironmentScript.isEmpty()) ||
            (envType == RunEnv && module->setupBuildEnvironmentScript.isEmpty() && module->setupRunEnvironmentScript.isEmpty()))
            continue;

        // handle imports
        engine->import(module->jsImports, scope, scope);

        // expose properties of direct module dependencies
        QScriptValue scriptValue;
        QVariantMap productModules = productConfiguration->value().value("modules").toMap();
        foreach (const ResolvedModule * const depmod, moduleChildren.value(module)) {
            scriptValue = engine->newObject();
            QVariantMap moduleCfg = productModules.value(depmod->name).toMap();
            for (QVariantMap::const_iterator it = moduleCfg.constBegin(); it != moduleCfg.constEnd(); ++it)
                scriptValue.setProperty(it.key(), engine->toScriptValue(it.value()));
            scope.setProperty(depmod->name, scriptValue);
        }

        // expose the module's properties
        QVariantMap moduleCfg = productModules.value(module->name).toMap();
        for (QVariantMap::const_iterator it = moduleCfg.constBegin(); it != moduleCfg.constEnd(); ++it)
            scope.setProperty(it.key(), engine->toScriptValue(it.value()));

        QString setupScript;
        if (envType == BuildEnv) {
            setupScript = module->setupBuildEnvironmentScript;
        } else {
            if (module->setupRunEnvironmentScript.isEmpty()) {
                setupScript = module->setupBuildEnvironmentScript;
            } else {
                setupScript = module->setupRunEnvironmentScript;
            }
        }

        QScriptContext *ctx = engine->currentContext();
        ctx->pushScope(scope);
        scriptValue = engine->evaluate(setupScript);
        ctx->popScope();
        if (scriptValue.isError() || engine->hasUncaughtException()) {
            QString envTypeStr = (envType == BuildEnv ? "build" : "run");
            throw Error(QString("Error while setting up %1 environment: %2").arg(envTypeStr, scriptValue.toString()));
        }
    }

    engine->setProperty("_qbs_procenv", QVariant());
    return procenv;
}

void ResolvedProduct::setupBuildEnvironment(QbsEngine *engine, const QProcessEnvironment &systemEnvironment) const
{
    if (!buildEnvironment.isEmpty())
        return;

    buildEnvironment = getProcessEnvironment(engine, BuildEnv, modules, configuration, project, systemEnvironment);
}

void ResolvedProduct::setupRunEnvironment(QbsEngine *engine, const QProcessEnvironment &systemEnvironment) const
{
    if (!runEnvironment.isEmpty())
        return;

    runEnvironment = getProcessEnvironment(engine, RunEnv, modules, configuration, project, systemEnvironment);
}

QString ResolvedProject::deriveId(const QVariantMap &config)
{
    const QVariantMap qbsProperties = config.value(QLatin1String("qbs")).toMap();
    const QString buildVariant = qbsProperties.value(QLatin1String("buildVariant")).toString();
    const QString profile = qbsProperties.value(QLatin1String("profile")).toString();
    return profile + QLatin1Char('-') + buildVariant;
}

void ResolvedProject::setConfiguration(const QVariantMap &config)
{
    if (!m_configuration)
        m_configuration = Configuration::create();
    m_configuration->setValue(config);
    m_id = deriveId(config);
}

void ResolvedProject::load(PersistentPool &pool, QDataStream &s)
{
    s >> m_id;
    s >> qbsFile;
    s >> platformEnvironment;

    int count;
    s >> count;
    products.clear();
    products.reserve(count);
    for (; --count >= 0;) {
        ResolvedProduct::Ptr rProduct = pool.idLoadS<ResolvedProduct>(s);
        rProduct->project = this;
        products.insert(rProduct);
    }
}

void ResolvedProject::store(PersistentPool &pool, QDataStream &s) const
{
    s << m_id;
    s << qbsFile;
    s << platformEnvironment;

    s << products.count();
    foreach (const ResolvedProduct::ConstPtr &product, products)
        pool.store(product);
}

} // namespace qbs
