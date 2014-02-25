/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
#include "transformer.h"

#include "artifact.h"
#include "command.h"
#include "rulesevaluationcontext.h"
#include <jsextensions/moduleproperties.h>
#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/persistence.h>
#include <tools/qbsassert.h>

namespace qbs {
namespace Internal {

Transformer::Transformer()
{
}

Transformer::~Transformer()
{
}

QScriptValue Transformer::translateFileConfig(QScriptEngine *scriptEngine, Artifact *artifact, const QString &defaultModuleName)
{
    QScriptValue obj = scriptEngine->newObject();
    ModuleProperties::init(obj, artifact);

    // ### undeprecate "fileName" and turn into a real file name in qbs 1.3
    ScriptEngine *qbsScriptEngine = static_cast<ScriptEngine *>(scriptEngine);
    qbsScriptEngine->setDeprecatedProperty(obj, QLatin1String("fileName"),
            QLatin1String("filePath"), artifact->filePath());

    obj.setProperty(QLatin1String("filePath"), artifact->filePath());
    const QStringList fileTags = artifact->fileTags.toStringList();
    obj.setProperty(QLatin1String("fileTags"), scriptEngine->toScriptValue(fileTags));
    if (!defaultModuleName.isEmpty())
        obj.setProperty(QLatin1String("moduleName"), defaultModuleName);
    return obj;
}

QScriptValue Transformer::translateInOutputs(QScriptEngine *scriptEngine, const ArtifactSet &artifacts, const QString &defaultModuleName)
{
    typedef QMap<QString, QList<Artifact*> > TagArtifactsMap;
    TagArtifactsMap tagArtifactsMap;
    foreach (Artifact *artifact, artifacts)
        foreach (const FileTag &fileTag, artifact->fileTags)
            tagArtifactsMap[fileTag.toString()].append(artifact);

    QScriptValue jsTagFiles = scriptEngine->newObject();
    for (TagArtifactsMap::const_iterator tag = tagArtifactsMap.constBegin(); tag != tagArtifactsMap.constEnd(); ++tag) {
        const QList<Artifact*> &artifacts = tag.value();
        QScriptValue jsFileConfig = scriptEngine->newArray(artifacts.count());
        int i=0;
        foreach (Artifact *artifact, artifacts) {
            jsFileConfig.setProperty(i++, translateFileConfig(scriptEngine, artifact, defaultModuleName));
        }
        jsTagFiles.setProperty(tag.key(), jsFileConfig);
    }

    return jsTagFiles;
}

ResolvedProductPtr Transformer::product() const
{
    if (outputs.isEmpty())
        return ResolvedProductPtr();
    return (*outputs.begin())->product;
}

void Transformer::setupInputs(QScriptValue targetScriptValue, const ArtifactSet &inputs,
        const QString &defaultModuleName)
{
    QScriptEngine *const scriptEngine = targetScriptValue.engine();
    QScriptValue scriptValue = translateInOutputs(scriptEngine, inputs, defaultModuleName);
    targetScriptValue.setProperty(QLatin1String("inputs"), scriptValue);
    if (inputs.count() == 1) {
        Artifact *input = *inputs.begin();
        const FileTags &fileTags = input->fileTags;
        QBS_ASSERT(!fileTags.isEmpty(), return);
        QScriptValue inputsForFileTag = scriptValue.property(fileTags.begin()->toString());
        QScriptValue inputScriptValue = inputsForFileTag.property(0);
        targetScriptValue.setProperty(QLatin1String("input"), inputScriptValue);
    } else {
        targetScriptValue.setProperty(QLatin1String("input"), scriptEngine->undefinedValue());
    }
}

void Transformer::setupInputs(QScriptValue targetScriptValue)
{
    setupInputs(targetScriptValue, inputs, rule->module->name);
}

void Transformer::setupOutputs(QScriptEngine *scriptEngine, QScriptValue targetScriptValue)
{
    const QString &defaultModuleName = rule->module->name;
    QScriptValue scriptValue = translateInOutputs(scriptEngine, outputs, defaultModuleName);
    targetScriptValue.setProperty(QLatin1String("outputs"), scriptValue);
    if (outputs.count() == 1) {
        Artifact *output = *outputs.begin();
        const FileTags &fileTags = output->fileTags;
        QBS_ASSERT(!fileTags.isEmpty(), return);
        QScriptValue outputsForFileTag = scriptValue.property(fileTags.begin()->toString());
        QScriptValue outputScriptValue = outputsForFileTag.property(0);
        targetScriptValue.setProperty(QLatin1String("output"), outputScriptValue);
    } else {
        targetScriptValue.setProperty(QLatin1String("output"), scriptEngine->undefinedValue());
    }
}

static AbstractCommandPtr createCommandFromScriptValue(const QScriptValue &scriptValue,
                                                       const CodeLocation &codeLocation)
{
    AbstractCommandPtr cmdBase;
    if (scriptValue.isUndefined() || !scriptValue.isValid())
        return cmdBase;
    QString className = scriptValue.property(QLatin1String("className")).toString();
    if (className == QLatin1String("Command"))
        cmdBase = ProcessCommand::create();
    else if (className == QLatin1String("JavaScriptCommand"))
        cmdBase = JavaScriptCommand::create();
    if (cmdBase)
        cmdBase->fillFromScriptValue(&scriptValue, codeLocation);
    return cmdBase;
}

void Transformer::createCommands(const ScriptFunctionConstPtr &script,
        const RulesEvaluationContextPtr &evalContext, const QScriptValueList &args)
{
    ScriptEngine * const engine = evalContext->engine();
    if (!script->scriptFunction.isValid() || script->scriptFunction.engine() != engine) {
        script->scriptFunction = engine->evaluate(script->sourceCode);
        if (Q_UNLIKELY(!script->scriptFunction.isFunction()))
            throw ErrorInfo(Tr::tr("Invalid prepare script."), script->location);
    }

    QScriptValue scriptValue = script->scriptFunction.call(QScriptValue(), args);
    propertiesRequestedInPrepareScript = engine->propertiesRequestedInScript();
    propertiesRequestedFromArtifactInPrepareScript = engine->propertiesRequestedFromArtifact();
    engine->clearRequestedProperties();
    if (Q_UNLIKELY(engine->hasErrorOrException(scriptValue)))
        throw ErrorInfo(Tr::tr("evaluating prepare script: ")
                        + engine->uncaughtException().toString(),
                    CodeLocation(script->location.fileName(),
                                 script->location.line() + engine->uncaughtExceptionLineNumber() - 1));

    commands.clear();
    if (scriptValue.isArray()) {
        const int count = scriptValue.property(QLatin1String("length")).toInt32();
        for (qint32 i = 0; i < count; ++i) {
            QScriptValue item = scriptValue.property(i);
            if (item.isValid() && !item.isUndefined()) {
                const AbstractCommandPtr cmd = createCommandFromScriptValue(item, script->location);
                if (cmd)
                    commands += cmd;
            }
        }
    } else {
        const AbstractCommandPtr cmd = createCommandFromScriptValue(scriptValue, script->location);
        if (cmd)
            commands += cmd;
    }
}

static void restorePropertyList(PersistentPool &pool, PropertyList &list)
{
    int count;
    pool.stream() >> count;
    list.reserve(count);
    while (--count >= 0) {
        Property p;
        p.moduleName = pool.idLoadString();
        p.propertyName = pool.idLoadString();
        int k;
        pool.stream() >> p.value >> k;
        p.kind = static_cast<Property::Kind>(k);
        list += p;
    }
}

void Transformer::load(PersistentPool &pool)
{
    rule = pool.idLoadS<Rule>();
    pool.loadContainer(inputs);
    pool.loadContainer(outputs);
    restorePropertyList(pool, propertiesRequestedInPrepareScript);
    restorePropertyList(pool, propertiesRequestedInCommands);
    int count;
    pool.stream() >> count;
    propertiesRequestedFromArtifactInPrepareScript.reserve(count);
    while (--count >= 0) {
        const QString artifactName = pool.idLoadString();
        int listCount;
        pool.stream() >> listCount;
        PropertyList list;
        list.reserve(listCount);
        while (--listCount >= 0) {
            Property p;
            p.moduleName = pool.idLoadString();
            p.propertyName = pool.idLoadString();
            pool.stream() >> p.value;
            p.kind = Property::PropertyInModule;
            list += p;
        }
        propertiesRequestedFromArtifactInPrepareScript.insert(artifactName, list);
    }
    commands = loadCommandList(pool);
}

static void storePropertyList(PersistentPool &pool, const PropertyList &list)
{
    pool.stream() << list.count();
    foreach (const Property &p, list) {
        pool.storeString(p.moduleName);
        pool.storeString(p.propertyName);
        pool.stream() << p.value << static_cast<int>(p.kind);
    }
}

void Transformer::store(PersistentPool &pool) const
{
    pool.store(rule);
    pool.storeContainer(inputs);
    pool.storeContainer(outputs);
    storePropertyList(pool, propertiesRequestedInPrepareScript);
    storePropertyList(pool, propertiesRequestedInCommands);
    pool.stream() << propertiesRequestedFromArtifactInPrepareScript.count();
    for (QHash<QString, PropertyList>::ConstIterator it = propertiesRequestedFromArtifactInPrepareScript.constBegin();
         it != propertiesRequestedFromArtifactInPrepareScript.constEnd(); ++it) {
        pool.storeString(it.key());
        const PropertyList &properties = it.value();
        pool.stream() << properties.count();
        foreach (const Property &p, properties) {
            pool.storeString(p.moduleName);
            pool.storeString(p.propertyName);
            pool.stream() << p.value; // kind is always PropertyInModule
        }
    }
    storeCommandList(commands, pool);
}

} // namespace Internal
} // namespace qbs
