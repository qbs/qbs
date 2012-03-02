/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#include "language.h"
#include <tools/scripttools.h>
#include <QtCore/QCryptographicHash>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValue>
#include <algorithm>

namespace qbs {

QMutex Configuration::m_scriptValueCacheMutex;

void Configuration::setValue(const QVariantMap &map)
{
    m_value = map;
    m_scriptValueCache.clear();
}

QScriptValue Configuration::cachedScriptValue(QScriptEngine *scriptEngine) const
{
    m_scriptValueCacheMutex.lock();
    const QScriptValue result = m_scriptValueCache.value(scriptEngine);
    m_scriptValueCacheMutex.unlock();
    Q_ASSERT(!result.isValid() || result.engine() == scriptEngine);
    return result;
}

void Configuration::cacheScriptValue(QScriptEngine *scriptEngine, const QScriptValue &scriptValue)
{
    Q_ASSERT(scriptEngine == scriptValue.engine());
    m_scriptValueCacheMutex.lock();
    m_scriptValueCache.insert(scriptEngine, scriptValue);
    m_scriptValueCacheMutex.unlock();
}

void Configuration::load(PersistentPool &, PersistentObjectData &data)
{
    QDataStream s(data);
    s >> m_value;
}

void Configuration::store(PersistentPool &, PersistentObjectData &data) const
{
    QDataStream s(&data, QIODevice::WriteOnly);
    s << m_value;
}

void FileTagger::load(PersistentPool &pool, PersistentObjectData &data)
{
    QDataStream s(data);
    artifactExpression.setPattern(pool.idLoadString(s));
    fileTags = pool.idLoadStringList(s);
}

void FileTagger::store(PersistentPool &pool, PersistentObjectData &data) const
{
    QDataStream s(&data, QIODevice::WriteOnly);
    s << pool.storeString(artifactExpression.pattern());
    s << pool.storeStringList(fileTags);
}

void SourceArtifact::load(PersistentPool &pool, PersistentObjectData &data)
{
    QDataStream s(data);
    s >> absoluteFilePath;
    s >> fileTags;
    configuration = pool.idLoadS<Configuration>(s);
}

void SourceArtifact::store(PersistentPool &pool, PersistentObjectData &data) const
{
    QDataStream s(&data, QIODevice::WriteOnly);
    s << absoluteFilePath;
    s << fileTags;
    s << pool.store(configuration);
}

void RuleArtifact::load(PersistentPool &pool, PersistentObjectData &data)
{
    Q_UNUSED(pool);
    QDataStream s(data);
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

void RuleArtifact::store(PersistentPool &pool, PersistentObjectData &data) const
{
    Q_UNUSED(pool);
    QDataStream s(&data, QIODevice::WriteOnly);
    s << fileScript;
    s << fileTags;

    s << bindings.count();
    for (int i = bindings.count(); --i >= 0;) {
        const Binding &binding = bindings.at(i);
        s << binding.name << binding.code << binding.location;
    }
}

void RuleScript::load(PersistentPool &, PersistentObjectData &data)
{
    QDataStream s(data);
    s >> script;
    s >> location;
}

void RuleScript::store(PersistentPool &, PersistentObjectData &data) const
{
    QDataStream s(&data, QIODevice::WriteOnly);
    s << script;
    s << location;
}

void ResolvedModule::load(PersistentPool &pool, PersistentObjectData &data)
{
    Q_UNUSED(pool);
    QDataStream s(data);
    name = pool.idLoadString(s);
    moduleDependencies = pool.idLoadStringList(s);
    setupBuildEnvironmentScript = pool.idLoadString(s);
    setupRunEnvironmentScript = pool.idLoadString(s);
    s >> jsImports
      >> setupBuildEnvironmentScript
      >> setupRunEnvironmentScript;
}

void ResolvedModule::store(PersistentPool &pool, PersistentObjectData &data) const
{
    Q_UNUSED(pool);
    QDataStream s(&data, QIODevice::WriteOnly);
    s << pool.storeString(name)
      << pool.storeStringList(moduleDependencies)
      << pool.storeString(setupBuildEnvironmentScript)
      << pool.storeString(setupRunEnvironmentScript)
      << jsImports
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
    foreach (RuleArtifact::Ptr artifact, artifacts)
        result.append(artifact->fileTags);
    result.sort();
    std::unique(result.begin(), result.end());
    return result;
}

void Rule::load(PersistentPool &pool, PersistentObjectData &data)
{
    QDataStream s(data);
    script = pool.idLoadS<RuleScript>(s);
    module = pool.idLoadS<ResolvedModule>(s);
    s   >> jsImports
        >> inputs
        >> usings
        >> explicitlyDependsOn
        >> multiplex
        >> objectId
        >> transformProperties;

    loadContainerS(artifacts, s, pool);
}

void Rule::store(PersistentPool &pool, PersistentObjectData &data) const
{
    QDataStream s(&data, QIODevice::WriteOnly);
    s   << pool.store(script)
        << pool.store(module)
        << jsImports
        << inputs
        << usings
        << explicitlyDependsOn
        << multiplex
        << objectId
        << transformProperties;

    storeContainer(artifacts, s, pool);
}

ResolvedProduct::ResolvedProduct()
    : project(0)
{
}

QSet<QString> ResolvedProduct::fileTagsForFileName(const QString &fileName) const
{
    QSet<QString> result;
    foreach (FileTagger::Ptr tagger, fileTaggers) {
        if (FileInfo::globMatches(tagger->artifactExpression, fileName)) {
            result.unite(tagger->fileTags.toSet());
        }
    }
    return result;
}

void ResolvedProduct::load(PersistentPool &pool, PersistentObjectData &data)
{
    QDataStream s(data);
    s   >> fileTags
        >> name
        >> buildDirectory
        >> sourceDirectory
        >> destinationDirectory
        >> qbsFile;

    configuration = pool.idLoadS<Configuration>(s);
    loadContainerS(sources, s, pool);
    loadContainerS(rules, s, pool);
    loadContainerS(uses, s, pool);
    loadContainerS(fileTaggers, s, pool);
    loadContainerS(modules, s, pool);
}

void ResolvedProduct::store(PersistentPool &pool, PersistentObjectData &data) const
{
    QDataStream s(&data, QIODevice::WriteOnly);
    s   << fileTags
        << name
        << buildDirectory
        << sourceDirectory
        << destinationDirectory
        << qbsFile;

    s << pool.store(configuration);
    storeContainer(sources, s, pool);
    storeContainer(rules, s, pool);
    storeContainer(uses, s, pool);
    storeContainer(fileTaggers, s, pool);
    storeContainer(modules, s, pool);
}

QList<ResolvedModule*> topSortModules(const QHash<ResolvedModule*, QList<ResolvedModule*> > &moduleChildren,
                                      const QList<ResolvedModule*> &modules,
                                      QSet<QString> &seenModuleNames)
{
    QList<ResolvedModule*> result;
    foreach (ResolvedModule *m, modules) {
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

static QProcessEnvironment getProcessEnvironment(QScriptEngine *scriptEngine, EnvType envType,
                                                 const QList<ResolvedModule::Ptr> &modules,
                                                 const Configuration::Ptr &productConfiguration,
                                                 ResolvedProject *project,
                                                 const QProcessEnvironment &systemEnvironment)
{
    QProcessEnvironment procenv = systemEnvironment;

    // Copy the environment of the platform configuration to the process environment.
    const QVariantMap platformEnv = project->configuration->value().value("environment").toMap();
    for (QVariantMap::const_iterator it = platformEnv.constBegin(); it != platformEnv.constEnd(); ++it)
        procenv.insert(it.key(), it.value().toString());

    QMap<QString, ResolvedModule*> moduleMap;
    foreach (ResolvedModule::Ptr module, modules)
        moduleMap.insert(module->name, module.data());

    QHash<ResolvedModule*, QList<ResolvedModule*> > moduleParents;
    QHash<ResolvedModule*, QList<ResolvedModule*> > moduleChildren;
    foreach (ResolvedModule::Ptr module, modules) {
        foreach (const QString &moduleName, module->moduleDependencies) {
            ResolvedModule *depmod = moduleMap.value(moduleName);
            moduleParents[depmod].append(module.data());
            moduleChildren[module.data()].append(depmod);
        }
    }

    QList<ResolvedModule*> rootModules;
    foreach (ResolvedModule::Ptr module, modules)
        if (moduleParents.value(module.data()).isEmpty())
            rootModules.append(module.data());

    {
        QVariant v;
        v.setValue<void*>(&procenv);
        scriptEngine->setProperty("_qbs_procenv", v);
    }

    QSet<QString> seenModuleNames;
    QList<ResolvedModule*> topSortedModules = topSortModules(moduleChildren, rootModules, seenModuleNames);
    foreach (ResolvedModule *module, topSortedModules) {
        if ((envType == BuildEnv && module->setupBuildEnvironmentScript.isEmpty()) ||
            (envType == RunEnv && module->setupBuildEnvironmentScript.isEmpty() && module->setupRunEnvironmentScript.isEmpty()))
            continue;

        // expose functions
        scriptEngine->globalObject().setProperty("getenv", scriptEngine->newFunction(js_getenv, 1));
        scriptEngine->globalObject().setProperty("putenv", scriptEngine->newFunction(js_putenv, 2));

        QScriptContext *ctx = scriptEngine->pushContext();

        // handle imports
        QScriptValue scriptValue;
        for (JsImports::const_iterator it = module->jsImports.begin(); it != module->jsImports.end(); ++it) {
            foreach (const QString &fileName, it.value()) {
                QFile file(fileName);
                if (!file.open(QFile::ReadOnly))
                    throw Error(QString("Can't open '%1'.").arg(fileName));
                QScriptProgram program(file.readAll(), fileName);
                scriptValue = addJSImport(scriptEngine, program, it.key());
                if (scriptValue.isError())
                    throw Error(scriptValue.toString());
            }
        }

        // expose properties of direct module dependencies
        QScriptValue activationObject = ctx->activationObject();
        QVariantMap productModules = productConfiguration->value().value("modules").toMap();
        foreach (ResolvedModule *depmod, moduleChildren.value(module)) {
            scriptValue = scriptEngine->newObject();
            QVariantMap moduleCfg = productModules.value(depmod->name).toMap();
            for (QVariantMap::const_iterator it = moduleCfg.constBegin(); it != moduleCfg.constEnd(); ++it)
                scriptValue.setProperty(it.key(), scriptEngine->toScriptValue(it.value()));
            activationObject.setProperty(depmod->name, scriptValue);
        }

        // expose the module's properties
        QVariantMap moduleCfg = productModules.value(module->name).toMap();
        for (QVariantMap::const_iterator it = moduleCfg.constBegin(); it != moduleCfg.constEnd(); ++it)
            activationObject.setProperty(it.key(), scriptEngine->toScriptValue(it.value()));

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
        scriptValue = scriptEngine->evaluate(setupScript);
        if (scriptValue.isError() || scriptEngine->hasUncaughtException()) {
            QString envTypeStr = (envType == BuildEnv ? "build" : "run");
            throw Error(QString("Error while setting up %1 environment: %2").arg(envTypeStr, scriptValue.toString()));
        }

        scriptEngine->popContext();
    }

    scriptEngine->setProperty("_qbs_procenv", QVariant());
    return procenv;
}

void ResolvedProduct::setupBuildEnvironment(QScriptEngine *scriptEngine, const QProcessEnvironment &systemEnvironment) const
{
    if (!buildEnvironment.isEmpty())
        return;

    buildEnvironment = getProcessEnvironment(scriptEngine, BuildEnv, modules, configuration, project, systemEnvironment);
}

void ResolvedProduct::setupRunEnvironment(QScriptEngine *scriptEngine, const QProcessEnvironment &systemEnvironment) const
{
    if (!runEnvironment.isEmpty())
        return;

    runEnvironment = getProcessEnvironment(scriptEngine, RunEnv, modules, configuration, project, systemEnvironment);
}

void ResolvedProject::load(PersistentPool &pool, PersistentObjectData &data)
{
    QDataStream s(data);
    s >> id;
    s >> qbsFile;

    int count, i;
    s >> count;
    products.clear();
    products.reserve(count);
    for (i = count; --i >= 0;) {
        ResolvedProduct::Ptr rProduct = pool.idLoadS<ResolvedProduct>(s);
        rProduct->project = this;
        products.insert(rProduct);
    }
}

void ResolvedProject::store(PersistentPool &pool, PersistentObjectData &data) const
{
    QDataStream s(&data, QIODevice::WriteOnly);
    s << id;
    s << qbsFile;

    s << products.count();
    foreach (ResolvedProduct::Ptr product, products)
        s << pool.store(product);
}

} // namespace qbs
