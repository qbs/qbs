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
#include <jsextensions/moduleproperties.h>
#include <language/language.h>
#include <language/preparescriptobserver.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/scripttools.h>
#include <tools/qbsassert.h>
#include <tools/stringconstants.h>
#include <tools/stlutils.h>

#include <QtCore/qdir.h>

#include <algorithm>

namespace qbs {
namespace Internal {

Transformer::Transformer() : alwaysRun(false)
{
}

Transformer::~Transformer() = default;

static QScriptValue js_baseName(QScriptContext *ctx, QScriptEngine *engine,
                                const Artifact *artifact)
{
    Q_UNUSED(ctx);
    Q_UNUSED(engine);
    return {FileInfo::baseName(artifact->filePath())};
}

static QScriptValue js_completeBaseName(QScriptContext *ctx, QScriptEngine *engine,
                                        const Artifact *artifact)
{
    Q_UNUSED(ctx);
    Q_UNUSED(engine);
    return {FileInfo::completeBaseName(artifact->filePath())};
}

static QScriptValue js_baseDir(QScriptContext *ctx, QScriptEngine *engine,
                               const Artifact *artifact)
{
    Q_UNUSED(ctx);
    Q_UNUSED(engine);
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

static QScriptValue js_children(QScriptContext *ctx, QScriptEngine *engine, const Artifact *artifact)
{
    Q_UNUSED(ctx);
    QScriptValue sv = engine->newArray();
    uint idx = 0;
    for (const Artifact *child : artifact->childArtifacts()) {
        sv.setProperty(idx++, Transformer::translateFileConfig(static_cast<ScriptEngine *>(engine),
                                                               child, QString()));
    }
    return sv;
}

static void setArtifactProperty(QScriptValue &obj, const QString &name,
        QScriptValue (*func)(QScriptContext *, QScriptEngine *, const Artifact *),
        const Artifact *artifact)
{
    obj.setProperty(name, static_cast<ScriptEngine *>(obj.engine())->newFunction(func, artifact),
                    QScriptValue::PropertyGetter);
}

QScriptValue Transformer::translateFileConfig(ScriptEngine *scriptEngine, const Artifact *artifact,
                                              const QString &defaultModuleName)
{
    QScriptValue obj = scriptEngine->newObject();
    attachPointerTo(obj, artifact);
    ModuleProperties::init(obj, artifact);
    obj.setProperty(StringConstants::fileNameProperty(), artifact->fileName());
    obj.setProperty(StringConstants::filePathProperty(), artifact->filePath());
    setArtifactProperty(obj, StringConstants::baseNameProperty(), js_baseName, artifact);
    setArtifactProperty(obj, StringConstants::completeBaseNameProperty(), js_completeBaseName,
                        artifact);
    setArtifactProperty(obj, QStringLiteral("baseDir"), js_baseDir, artifact);
    setArtifactProperty(obj, QStringLiteral("children"), js_children, artifact);
    const QStringList fileTags = sorted(artifact->fileTags().toStringList());
    scriptEngine->setObservedProperty(obj, StringConstants::fileTagsProperty(),
                                      scriptEngine->toScriptValue(fileTags));
    scriptEngine->observer()->addArtifactId(obj.objectId());
    if (!defaultModuleName.isEmpty())
        obj.setProperty(StringConstants::moduleNameProperty(), defaultModuleName);
    return obj;
}

static bool compareByFilePath(const Artifact *a1, const Artifact *a2)
{
    return a1->filePath() < a2->filePath();
}

QScriptValue Transformer::translateInOutputs(ScriptEngine *scriptEngine,
                                             const ArtifactSet &artifacts,
                                             const QString &defaultModuleName)
{
    using TagArtifactsMap = QMap<QString, QList<Artifact*>>;
    TagArtifactsMap tagArtifactsMap;
    for (Artifact *artifact : artifacts)
        for (const FileTag &fileTag : artifact->fileTags())
            tagArtifactsMap[fileTag.toString()].push_back(artifact);
    for (TagArtifactsMap::Iterator it = tagArtifactsMap.begin(); it != tagArtifactsMap.end(); ++it)
        std::sort(it.value().begin(), it.value().end(), compareByFilePath);

    QScriptValue jsTagFiles = scriptEngine->newObject();
    for (TagArtifactsMap::const_iterator tag = tagArtifactsMap.constBegin(); tag != tagArtifactsMap.constEnd(); ++tag) {
        const QList<Artifact*> &artifacts = tag.value();
        QScriptValue jsFileConfig = scriptEngine->newArray(artifacts.size());
        int i = 0;
        for (Artifact * const artifact : artifacts) {
            jsFileConfig.setProperty(i++, translateFileConfig(scriptEngine, artifact,
                                                              defaultModuleName));
        }
        jsTagFiles.setProperty(tag.key(), jsFileConfig);
    }

    return jsTagFiles;
}

ResolvedProductPtr Transformer::product() const
{
    if (outputs.empty())
        return {};
    return (*outputs.cbegin())->product.lock();
}

void Transformer::setupInputs(QScriptValue targetScriptValue, const ArtifactSet &inputs,
        const QString &defaultModuleName)
{
    const auto scriptEngine = static_cast<ScriptEngine *>(targetScriptValue.engine());
    QScriptValue scriptValue = translateInOutputs(scriptEngine, inputs, defaultModuleName);
    targetScriptValue.setProperty(StringConstants::inputsVar(), scriptValue);
    QScriptValue inputScriptValue;
    if (inputs.size() == 1) {
        Artifact *input = *inputs.cbegin();
        const FileTags &fileTags = input->fileTags();
        QBS_ASSERT(!fileTags.empty(), return);
        QScriptValue inputsForFileTag = scriptValue.property(fileTags.cbegin()->toString());
        inputScriptValue = inputsForFileTag.property(0);
    }
    targetScriptValue.setProperty(StringConstants::inputVar(), inputScriptValue);
}

void Transformer::setupInputs(const QScriptValue &targetScriptValue)
{
    setupInputs(targetScriptValue, inputs, rule->module->name);
}

void Transformer::setupOutputs(QScriptValue targetScriptValue)
{
    const auto scriptEngine = static_cast<ScriptEngine *>(targetScriptValue.engine());
    const QString &defaultModuleName = rule->module->name;
    QScriptValue scriptValue = translateInOutputs(scriptEngine, outputs, defaultModuleName);
    targetScriptValue.setProperty(StringConstants::outputsVar(), scriptValue);
    QScriptValue outputScriptValue;
    if (outputs.size() == 1) {
        Artifact *output = *outputs.cbegin();
        const FileTags &fileTags = output->fileTags();
        QBS_ASSERT(!fileTags.empty(), return);
        QScriptValue outputsForFileTag = scriptValue.property(fileTags.cbegin()->toString());
        outputScriptValue = outputsForFileTag.property(0);
    }
    targetScriptValue.setProperty(StringConstants::outputVar(), outputScriptValue);
}

void Transformer::setupExplicitlyDependsOn(QScriptValue targetScriptValue)
{
    const auto scriptEngine = static_cast<ScriptEngine *>(targetScriptValue.engine());
    QScriptValue scriptValue = translateInOutputs(scriptEngine, explicitlyDependsOn,
                                                  rule->module->name);
    targetScriptValue.setProperty(StringConstants::explicitlyDependsOnVar(), scriptValue);
}

AbstractCommandPtr Transformer::createCommandFromScriptValue(const QScriptValue &scriptValue,
                                                             const CodeLocation &codeLocation)
{
    AbstractCommandPtr cmdBase;
    if (scriptValue.isUndefined() || !scriptValue.isValid())
        return cmdBase;
    QString className = scriptValue.property(StringConstants::classNameProperty()).toString();
    if (className == StringConstants::commandType())
        cmdBase = ProcessCommand::create();
    else if (className == StringConstants::javaScriptCommandType())
        cmdBase = JavaScriptCommand::create();
    if (cmdBase)
        cmdBase->fillFromScriptValue(&scriptValue, codeLocation);
    if (className == StringConstants::commandType()) {
        auto procCmd = static_cast<ProcessCommand *>(cmdBase.get());
        procCmd->clearRelevantEnvValues();
        const auto envVars = procCmd->relevantEnvVars();
        for (const QString &key : envVars)
            procCmd->addRelevantEnvValue(key, product()->buildEnvironment.value(key));
    }
    return cmdBase;
}

void Transformer::createCommands(ScriptEngine *engine, const PrivateScriptFunction &script,
                                 const QScriptValueList &args)
{
    if (!script.scriptFunction.isValid() || script.scriptFunction.engine() != engine) {
        script.scriptFunction = engine->evaluate(script.sourceCode(),
                                                  script.location().filePath(),
                                                  script.location().line());
        if (Q_UNLIKELY(!script.scriptFunction.isFunction()))
            throw ErrorInfo(Tr::tr("Invalid prepare script."), script.location());
    }

    QScriptValue scriptValue = script.scriptFunction.call(QScriptValue(), args);
    engine->releaseResourcesOfScriptObjects();
    propertiesRequestedInPrepareScript = engine->propertiesRequestedInScript();
    propertiesRequestedFromArtifactInPrepareScript = engine->propertiesRequestedFromArtifact();
    importedFilesUsedInPrepareScript = engine->importedFilesUsedInScript();
    depsRequestedInPrepareScript = engine->requestedDependencies();
    artifactsMapRequestedInPrepareScript = engine->requestedArtifacts();
    lastPrepareScriptExecutionTime = FileTime::currentTime();
    for (const ResolvedProduct * const p : engine->requestedExports()) {
        exportedModulesAccessedInPrepareScript.insert(std::make_pair(p->uniqueName(),
                                                                     p->exportedModule));
    }
    engine->clearRequestedProperties();
    if (Q_UNLIKELY(engine->hasErrorOrException(scriptValue)))
        throw engine->lastError(scriptValue, script.location());
    commands.clear();
    if (scriptValue.isArray()) {
        const int count = scriptValue.property(StringConstants::lengthProperty()).toInt32();
        for (qint32 i = 0; i < count; ++i) {
            QScriptValue item = scriptValue.property(i);
            if (item.isValid() && !item.isUndefined()) {
                const AbstractCommandPtr cmd
                        = createCommandFromScriptValue(item, script.location());
                if (cmd)
                    commands.addCommand(cmd);
            }
        }
    } else {
        const AbstractCommandPtr cmd = createCommandFromScriptValue(scriptValue,
                                                                    script.location());
        if (cmd)
            commands.addCommand(cmd);
    }
}

void Transformer::rescueChangeTrackingData(const TransformerConstPtr &other)
{
    if (!other)
        return;
    propertiesRequestedInPrepareScript = other->propertiesRequestedInPrepareScript;
    propertiesRequestedInCommands = other->propertiesRequestedInCommands;
    propertiesRequestedFromArtifactInPrepareScript
            = other->propertiesRequestedFromArtifactInPrepareScript;
    propertiesRequestedFromArtifactInCommands = other->propertiesRequestedFromArtifactInCommands;
    importedFilesUsedInPrepareScript = other->importedFilesUsedInPrepareScript;
    importedFilesUsedInCommands = other->importedFilesUsedInCommands;
    depsRequestedInPrepareScript = other->depsRequestedInPrepareScript;
    depsRequestedInCommands = other->depsRequestedInCommands;
    artifactsMapRequestedInPrepareScript = other->artifactsMapRequestedInPrepareScript;
    artifactsMapRequestedInCommands = other->artifactsMapRequestedInCommands;
    lastCommandExecutionTime = other->lastCommandExecutionTime;
    lastPrepareScriptExecutionTime = other->lastPrepareScriptExecutionTime;
    prepareScriptNeedsChangeTracking = other->prepareScriptNeedsChangeTracking;
    commandsNeedChangeTracking = other->commandsNeedChangeTracking;
    markedForRerun = other->markedForRerun;
    exportedModulesAccessedInPrepareScript = other->exportedModulesAccessedInPrepareScript;
    exportedModulesAccessedInCommands = other->exportedModulesAccessedInCommands;
}

Set<QString> Transformer::jobPools() const
{
    Set<QString> pools;
    for (const AbstractCommandPtr &c : commands.commands()) {
        if (!c->jobPool().isEmpty())
            pools.insert(c->jobPool());
    }
    return pools;
}

} // namespace Internal
} // namespace qbs
