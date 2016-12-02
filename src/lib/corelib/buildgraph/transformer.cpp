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
#include "transformer.h"

#include "artifact.h"
#include "command.h"
#include "rulesevaluationcontext.h"
#include <jsextensions/moduleproperties.h>
#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/persistence.h>
#include <tools/scripttools.h>
#include <tools/qbsassert.h>

#include <QDir>

#include <algorithm>

namespace qbs {
namespace Internal {

Transformer::Transformer() : alwaysRun(false)
{
}

Transformer::~Transformer()
{
}

static QScriptValue js_baseName(QScriptContext *ctx, QScriptEngine *engine, void *arg)
{
    Q_UNUSED(ctx);
    Q_UNUSED(engine);
    Artifact *artifact = reinterpret_cast<Artifact *>(arg);
    return QScriptValue(FileInfo::baseName(artifact->filePath()));
}

static QScriptValue js_completeBaseName(QScriptContext *ctx, QScriptEngine *engine, void *arg)
{
    Q_UNUSED(ctx);
    Q_UNUSED(engine);
    Artifact *artifact = reinterpret_cast<Artifact *>(arg);
    return QScriptValue(FileInfo::completeBaseName(artifact->filePath()));
}

static QScriptValue js_baseDir(QScriptContext *ctx, QScriptEngine *engine, void *arg)
{
    Q_UNUSED(ctx);
    Q_UNUSED(engine);
    Artifact *artifact = reinterpret_cast<Artifact *>(arg);
    QString basedir;
    if (artifact->artifactType == Artifact::SourceFile) {
        QDir sourceDir(artifact->product->sourceDirectory);
        basedir = FileInfo::path(sourceDir.relativeFilePath(artifact->filePath()));
    } else {
        QDir buildDir(artifact->product->buildDirectory());
        basedir = FileInfo::path(buildDir.relativeFilePath(artifact->filePath()));
    }
    return basedir;
}

static void setArtifactProperty(QScriptValue &obj, const QString &name,
        QScriptEngine::FunctionWithArgSignature func, Artifact *artifact)
{
    obj.setProperty(name, obj.engine()->newFunction(func, (void *)artifact),
                    QScriptValue::PropertyGetter);
}

QScriptValue Transformer::translateFileConfig(QScriptEngine *scriptEngine, Artifact *artifact, const QString &defaultModuleName)
{
    QScriptValue obj = scriptEngine->newObject();
    attachPointerTo(obj, artifact);
    ModuleProperties::init(obj, artifact);
    obj.setProperty(QLatin1String("fileName"), artifact->fileName());
    obj.setProperty(QLatin1String("filePath"), artifact->filePath());
    setArtifactProperty(obj, QLatin1String("baseName"), js_baseName, artifact);
    setArtifactProperty(obj, QLatin1String("completeBaseName"), js_completeBaseName, artifact);
    setArtifactProperty(obj, QLatin1String("baseDir"), js_baseDir, artifact);
    const QStringList fileTags = artifact->fileTags().toStringList();
    obj.setProperty(QLatin1String("fileTags"), scriptEngine->toScriptValue(fileTags));
    if (!defaultModuleName.isEmpty())
        obj.setProperty(QLatin1String("moduleName"), defaultModuleName);
    return obj;
}

static bool compareByFilePath(const Artifact *a1, const Artifact *a2)
{
    return a1->filePath() < a2->filePath();
}

QScriptValue Transformer::translateInOutputs(QScriptEngine *scriptEngine, const ArtifactSet &artifacts, const QString &defaultModuleName)
{
    typedef QMap<QString, QList<Artifact*> > TagArtifactsMap;
    TagArtifactsMap tagArtifactsMap;
    foreach (Artifact *artifact, artifacts)
        foreach (const FileTag &fileTag, artifact->fileTags())
            tagArtifactsMap[fileTag.toString()].append(artifact);
    for (TagArtifactsMap::Iterator it = tagArtifactsMap.begin(); it != tagArtifactsMap.end(); ++it)
        std::sort(it.value().begin(), it.value().end(), compareByFilePath);

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
    QScriptValue inputScriptValue;
    if (inputs.count() == 1) {
        Artifact *input = *inputs.begin();
        const FileTags &fileTags = input->fileTags();
        QBS_ASSERT(!fileTags.isEmpty(), return);
        QScriptValue inputsForFileTag = scriptValue.property(fileTags.begin()->toString());
        inputScriptValue = inputsForFileTag.property(0);
    }
    targetScriptValue.setProperty(QLatin1String("input"), inputScriptValue);
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
    QScriptValue outputScriptValue;
    if (outputs.count() == 1) {
        Artifact *output = *outputs.begin();
        const FileTags &fileTags = output->fileTags();
        QBS_ASSERT(!fileTags.isEmpty(), return);
        QScriptValue outputsForFileTag = scriptValue.property(fileTags.begin()->toString());
        outputScriptValue = outputsForFileTag.property(0);
    }
    targetScriptValue.setProperty(QLatin1String("output"), outputScriptValue);
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
        script->scriptFunction = engine->evaluate(script->sourceCode,
                                                  script->location.filePath(),
                                                  script->location.line());
        if (Q_UNLIKELY(!script->scriptFunction.isFunction()))
            throw ErrorInfo(Tr::tr("Invalid prepare script."), script->location);
    }

    QScriptValue scriptValue = script->scriptFunction.call(QScriptValue(), args);
    propertiesRequestedInPrepareScript = engine->propertiesRequestedInScript();
    propertiesRequestedFromArtifactInPrepareScript = engine->propertiesRequestedFromArtifact();
    engine->clearRequestedProperties();
    if (Q_UNLIKELY(engine->hasErrorOrException(scriptValue))) {
        ErrorInfo errorInfo(engine->lastErrorString(scriptValue),
                            engine->uncaughtExceptionBacktraceOrEmpty());
        if (rule)
            errorInfo.append(QStringLiteral("Rule.prepare"), script->location);
        else
            errorInfo.append(QStringLiteral("Transformer.prepare"), script->location);
        throw errorInfo;
    }

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

void Transformer::load(PersistentPool &pool)
{
    rule = pool.idLoadS<Rule>();
    pool.loadContainer(inputs);
    pool.loadContainer(outputs);
    propertiesRequestedInPrepareScript = restorePropertySet(pool);
    propertiesRequestedInCommands = restorePropertySet(pool);
    propertiesRequestedFromArtifactInPrepareScript = restorePropertyHash(pool);
    propertiesRequestedFromArtifactInCommands = restorePropertyHash(pool);
    commands = loadCommandList(pool);
    pool.stream() >> alwaysRun;
}

void Transformer::store(PersistentPool &pool) const
{
    pool.store(rule);
    pool.storeContainer(inputs);
    pool.storeContainer(outputs);
    storePropertySet(pool, propertiesRequestedInPrepareScript);
    storePropertySet(pool, propertiesRequestedInCommands);
    storePropertyHash(pool, propertiesRequestedFromArtifactInPrepareScript);
    storePropertyHash(pool, propertiesRequestedFromArtifactInCommands);
    storeCommandList(commands, pool);
    pool.stream() << alwaysRun;
}

} // namespace Internal
} // namespace qbs
