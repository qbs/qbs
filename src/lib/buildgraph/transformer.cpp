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
    qDeleteAll(commands);
}

QScriptValue Transformer::translateFileConfig(QScriptEngine *scriptEngine, Artifact *artifact, const QString &defaultModuleName)
{
    QScriptValue artifactConfig = scriptEngine->newObject();
    ModuleProperties::init(artifactConfig, artifact);
    artifactConfig.setProperty(QLatin1String("fileName"), artifact->filePath());
    const QStringList fileTags = artifact->fileTags.toStringList();
    artifactConfig.setProperty(QLatin1String("fileTags"), scriptEngine->toScriptValue(fileTags));
    if (!defaultModuleName.isEmpty())
        artifactConfig.setProperty(QLatin1String("moduleName"), defaultModuleName);
    return artifactConfig;
}

QScriptValue Transformer::translateInOutputs(QScriptEngine *scriptEngine, const ArtifactList &artifacts, const QString &defaultModuleName)
{
    typedef QMap<QString, QList<Artifact*> > TagArtifactsMap;
    TagArtifactsMap tagArtifactsMap;
    foreach (Artifact *artifact, artifacts)
        foreach (const FileTag &fileTag, artifact->fileTags)
            tagArtifactsMap[fileTag.toString()].append(artifact);

    QScriptValue jsTagFiles = scriptEngine->newObject();
    for (TagArtifactsMap::const_iterator tag = tagArtifactsMap.constBegin(); tag != tagArtifactsMap.constEnd(); ++tag) {
        const QList<Artifact*> &artifactList = tag.value();
        QScriptValue jsFileConfig = scriptEngine->newArray(artifactList.count());
        int i=0;
        foreach (Artifact *artifact, artifactList) {
            jsFileConfig.setProperty(i++, translateFileConfig(scriptEngine, artifact, defaultModuleName));
        }
        jsTagFiles.setProperty(tag.key(), jsFileConfig);
    }

    return jsTagFiles;
}

void Transformer::setupInputs(QScriptEngine *scriptEngine, QScriptValue targetScriptValue)
{
    const QString &defaultModuleName = rule->module->name;
    QScriptValue scriptValue = translateInOutputs(scriptEngine, inputs, defaultModuleName);
    targetScriptValue.setProperty("inputs", scriptValue);
    if (inputs.count() == 1) {
        Artifact *input = *inputs.begin();
        const FileTags &fileTags = input->fileTags;
        QBS_ASSERT(!fileTags.isEmpty(), return);
        QScriptValue inputsForFileTag = scriptValue.property(fileTags.begin()->toString());
        QScriptValue inputScriptValue = inputsForFileTag.property(0);
        targetScriptValue.setProperty("input", inputScriptValue);
    } else {
        targetScriptValue.setProperty(QLatin1String("input"), scriptEngine->undefinedValue());
    }
}

void Transformer::setupOutputs(QScriptEngine *scriptEngine, QScriptValue targetScriptValue)
{
    const QString &defaultModuleName = rule->module->name;
    QScriptValue scriptValue = translateInOutputs(scriptEngine, outputs, defaultModuleName);
    targetScriptValue.setProperty("outputs", scriptValue);
    if (outputs.count() == 1) {
        Artifact *output = *outputs.begin();
        const FileTags &fileTags = output->fileTags;
        QBS_ASSERT(!fileTags.isEmpty(), return);
        QScriptValue outputsForFileTag = scriptValue.property(fileTags.begin()->toString());
        QScriptValue outputScriptValue = outputsForFileTag.property(0);
        targetScriptValue.setProperty("output", outputScriptValue);
    } else {
        targetScriptValue.setProperty(QLatin1String("output"), scriptEngine->undefinedValue());
    }
}

static AbstractCommand *createCommandFromScriptValue(const QScriptValue &scriptValue,
                                                     const CodeLocation &codeLocation)
{
    if (scriptValue.isUndefined() || !scriptValue.isValid())
        return 0;
    AbstractCommand *cmdBase = 0;
    QString className = scriptValue.property("className").toString();
    if (className == "Command")
        cmdBase = new ProcessCommand;
    else if (className == "JavaScriptCommand")
        cmdBase = new JavaScriptCommand;
    if (cmdBase)
        cmdBase->fillFromScriptValue(&scriptValue, codeLocation);
    return cmdBase;
}

void Transformer::createCommands(const PrepareScriptConstPtr &script,
                                 const RulesEvaluationContextPtr &evalContext)
{
    ScriptEngine * const engine = evalContext->engine();
    if (!script->scriptFunction.isValid() || script->scriptFunction.engine() != engine) {
        script->scriptFunction = engine->evaluate(script->script);
        if (Q_UNLIKELY(!script->scriptFunction.isFunction()))
            throw ErrorInfo(Tr::tr("Invalid prepare script."), script->location);
    }

    QScriptValue scriptValue = script->scriptFunction.call();
    modulePropertiesUsedInPrepareScript = engine->properties();
    engine->clearProperties();
    if (Q_UNLIKELY(engine->hasUncaughtException()))
        throw ErrorInfo("evaluating prepare script: " + engine->uncaughtException().toString(),
                    CodeLocation(script->location.fileName(),
                                 script->location.line() + engine->uncaughtExceptionLineNumber() - 1));

    qDeleteAll(commands);
    commands.clear();
    if (scriptValue.isArray()) {
        const int count = scriptValue.property("length").toInt32();
        for (qint32 i = 0; i < count; ++i) {
            QScriptValue item = scriptValue.property(i);
            if (item.isValid() && !item.isUndefined()) {
                AbstractCommand *cmd = createCommandFromScriptValue(item, script->location);
                if (cmd)
                    commands += cmd;
            }
        }
    } else {
        AbstractCommand *cmd = createCommandFromScriptValue(scriptValue, script->location);
        if (cmd)
            commands += cmd;
    }
}

void Transformer::load(PersistentPool &pool)
{
    rule = pool.idLoadS<Rule>();
    pool.loadContainer(inputs);
    pool.loadContainer(outputs);
    int count;
    pool.stream() >> count;
    modulePropertiesUsedInPrepareScript.reserve(count);
    while (--count >= 0) {
        Property p;
        p.moduleName = pool.idLoadString();
        p.propertyName = pool.idLoadString();
        int k;
        pool.stream() >> p.value >> k;
        p.kind = static_cast<Property::Kind>(k);
        modulePropertiesUsedInPrepareScript += p;
    }
    int cmdType;
    pool.stream() >> count;
    commands.reserve(count);
    while (--count >= 0) {
        pool.stream() >> cmdType;
        AbstractCommand *cmd = AbstractCommand::createByType(static_cast<AbstractCommand::CommandType>(cmdType));
        cmd->load(pool.stream());
        commands += cmd;
    }
}

void Transformer::store(PersistentPool &pool) const
{
    pool.store(rule);
    pool.storeContainer(inputs);
    pool.storeContainer(outputs);
    pool.stream() << modulePropertiesUsedInPrepareScript.count();
    foreach (const Property &p, modulePropertiesUsedInPrepareScript) {
        pool.storeString(p.moduleName);
        pool.storeString(p.propertyName);
        pool.stream() << p.value << static_cast<int>(p.kind);
    }
    pool.stream() << commands.count();
    foreach (AbstractCommand *cmd, commands) {
        pool.stream() << int(cmd->type());
        cmd->store(pool.stream());
    }
}

} // namespace Internal
} // namespace qbs
