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
#include "productbuilddata.h"
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
#include <vector>

namespace qbs {
namespace Internal {

Transformer::Transformer() : alwaysRun(false)
{
}

Transformer::~Transformer() = default;

static JSValue js_baseName(JSContext *ctx, JSValueConst this_val, int, JSValueConst *)
{
    return ScriptEngine::engineForContext(ctx)->getArtifactProperty(this_val,
            [ctx](const Artifact *a) {
        return makeJsString(ctx, FileInfo::baseName(a->filePath()));
    });
}

static JSValue js_completeBaseName(JSContext *ctx, JSValueConst this_val, int, JSValueConst *)
{
    return ScriptEngine::engineForContext(ctx)->getArtifactProperty(this_val,
            [ctx](const Artifact *a) {
        return makeJsString(ctx, FileInfo::completeBaseName(a->filePath()));
    });
}

static JSValue js_baseDir(JSContext *ctx, JSValueConst this_val, int, JSValueConst *)
{
    return ScriptEngine::engineForContext(ctx)->getArtifactProperty(this_val,
            [ctx](const Artifact *artifact) {
        QString basedir;
        if (artifact->artifactType == Artifact::SourceFile) {
            QDir sourceDir(artifact->product->sourceDirectory);
            basedir = FileInfo::path(sourceDir.relativeFilePath(artifact->filePath()));
        } else {
            QDir buildDir(artifact->product->buildDirectory());
            basedir = FileInfo::path(buildDir.relativeFilePath(artifact->filePath()));
        }
        return makeJsString(ctx, basedir);
    });
}

static JSValue js_children(JSContext *ctx, JSValueConst this_val, int, JSValueConst *)
{
    return ScriptEngine::engineForContext(ctx)->getArtifactProperty(this_val,
            [ctx](const Artifact *artifact) {
        JSValue sv = JS_NewArray(ctx);
        uint idx = 0;

        // FIXME: childArtifacts() is not guarded by any mutex ...
        for (Artifact *child : artifact->childArtifacts()) {
            JS_SetPropertyUint32(ctx, sv, idx++, Transformer::translateFileConfig(
                                     ScriptEngine::engineForContext(ctx), child, QString()));
        }
        return sv;
    });
}

static void setArtifactProperty(JSContext *ctx, JSValue &obj, const QString &name,
                                JSCFunction *func)
{
    const QByteArray nameBa = name.toUtf8();
    const JSValue jsFunc = JS_NewCFunction(ctx, func, nameBa.constData(), 0);
    const ScopedJsAtom nameAtom(ctx, nameBa);
    JS_DefinePropertyGetSet(ctx, obj, nameAtom, jsFunc, JS_UNDEFINED, JS_PROP_HAS_GET);
}

JSValue Transformer::translateFileConfig(ScriptEngine *engine, Artifact *artifact,
                                         const QString &defaultModuleName)
{
    return engine->getArtifactScriptValue(artifact, defaultModuleName, [&](JSValue obj) {
        ModuleProperties::init(engine, obj, artifact);
        JSContext * const ctx = engine->context();
        setJsProperty(ctx, obj, StringConstants::fileNameProperty(), artifact->fileName());
        setJsProperty(ctx, obj, StringConstants::filePathProperty(), artifact->filePath());
        setArtifactProperty(ctx, obj, StringConstants::baseNameProperty(), js_baseName);
        setArtifactProperty(ctx, obj, StringConstants::completeBaseNameProperty(), js_completeBaseName);
        setArtifactProperty(ctx, obj, QStringLiteral("baseDir"), js_baseDir);
        setArtifactProperty(ctx, obj, QStringLiteral("children"), js_children);
        const QStringList fileTags = sorted(artifact->fileTags().toStringList());
        const ScopedJsValue jsFileTags(ctx, engine->toScriptValue(fileTags));
        engine->setObservedProperty(obj, StringConstants::fileTagsProperty(), jsFileTags);
        engine->observer()->addArtifactId(jsObjectId(obj));
        if (!defaultModuleName.isEmpty())
            setJsProperty(ctx, obj, StringConstants::moduleNameProperty(), defaultModuleName);
    });
}

static bool compareByFilePath(const Artifact *a1, const Artifact *a2)
{
    return a1->filePath() < a2->filePath();
}

JSValue Transformer::translateInOutputs(ScriptEngine *engine, const ArtifactSet &artifacts,
                                        const QString &defaultModuleName)
{
    using TagArtifactsMap = QMap<QString, QList<Artifact*>>;
    TagArtifactsMap tagArtifactsMap;
    for (Artifact *artifact : artifacts)
        for (const FileTag &fileTag : artifact->fileTags())
            tagArtifactsMap[fileTag.toString()].push_back(artifact);
    for (TagArtifactsMap::Iterator it = tagArtifactsMap.begin(); it != tagArtifactsMap.end(); ++it)
        std::sort(it.value().begin(), it.value().end(), compareByFilePath);

    JSValue jsTagFiles = engine->newObject();
    for (auto tag = tagArtifactsMap.constBegin(); tag != tagArtifactsMap.constEnd(); ++tag) {
        const QList<Artifact*> &artifacts = tag.value();
        JSValue jsFileConfig = JS_NewArray(engine->context());
        int i = 0;
        for (Artifact * const artifact : artifacts) {
            JS_SetPropertyUint32(engine->context(), jsFileConfig, i++,
                                 translateFileConfig(engine, artifact, defaultModuleName));
        }
        setJsProperty(engine->context(), jsTagFiles, tag.key(), jsFileConfig);
    }

    return jsTagFiles;
}

ResolvedProductPtr Transformer::product() const
{
    if (outputs.empty())
        return {};
    return (*outputs.cbegin())->product.lock();
}

void Transformer::setupInputs(ScriptEngine *engine, JSValue targetScriptValue,
                              const ArtifactSet &inputs, const QString &defaultModuleName)
{
    JSValue scriptValue = translateInOutputs(engine, inputs, defaultModuleName);
    setJsProperty(engine->context(), targetScriptValue, StringConstants::inputsVar(), scriptValue);
    JSValue inputScriptValue = JS_UNDEFINED;
    if (inputs.size() == 1) {
        Artifact *input = *inputs.cbegin();
        const FileTags &fileTags = input->fileTags();
        QBS_ASSERT(!fileTags.empty(), return);
        const ScopedJsValue inputsForFileTag(
                    engine->context(),
                    getJsProperty(engine->context(), scriptValue, fileTags.cbegin()->toString()));
        inputScriptValue = JS_GetPropertyUint32(engine->context(), inputsForFileTag, 0);
    }
    setJsProperty(engine->context(), targetScriptValue, StringConstants::inputVar(),
                  inputScriptValue);
}

void Transformer::setupInputs(ScriptEngine *engine, const JSValue &targetScriptValue)
{
    setupInputs(engine, targetScriptValue, inputs, rule->module->name);
}

void Transformer::setupOutputs(ScriptEngine *engine, JSValue targetScriptValue)
{
    const QString &defaultModuleName = rule->module->name;
    JSValue scriptValue = translateInOutputs(engine, outputs, defaultModuleName);
    setJsProperty(engine->context(), targetScriptValue, StringConstants::outputsVar(), scriptValue);
    JSValue outputScriptValue = JS_UNDEFINED;
    if (outputs.size() == 1) {
        Artifact *output = *outputs.cbegin();
        const FileTags &fileTags = output->fileTags();
        QBS_ASSERT(!fileTags.empty(), return);
        const ScopedJsValue outputsForFileTag(
                    engine->context(),
                    getJsProperty(engine->context(), scriptValue, fileTags.cbegin()->toString()));
        outputScriptValue = JS_GetPropertyUint32(engine->context(), outputsForFileTag, 0);
    }
    setJsProperty(engine->context(), targetScriptValue, StringConstants::outputVar(),
                  outputScriptValue);
}

void Transformer::setupExplicitlyDependsOn(ScriptEngine *engine, JSValue targetScriptValue)
{
    JSValue scriptValue = translateInOutputs(engine, explicitlyDependsOn, rule->module->name);
    setJsProperty(engine->context(), targetScriptValue, StringConstants::explicitlyDependsOnVar(),
                  scriptValue);
}

AbstractCommandPtr Transformer::createCommandFromScriptValue(
        ScriptEngine *engine, const JSValue &scriptValue, const CodeLocation &codeLocation)
{
    AbstractCommandPtr cmdBase;
    if (JS_IsUndefined(scriptValue) || JS_IsUninitialized(scriptValue))
        return cmdBase;
    QString className = getJsStringProperty(engine->context(), scriptValue,
                                            StringConstants::classNameProperty());
    if (className == StringConstants::commandType())
        cmdBase = ProcessCommand::create();
    else if (className == StringConstants::javaScriptCommandType())
        cmdBase = JavaScriptCommand::create();
    if (cmdBase)
        cmdBase->fillFromScriptValue(engine->context(), &scriptValue, codeLocation);
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
                                 const JSValueList &args)
{
    JSValueList argv(args.cbegin(), args.cend());
    const JSValue function = script.getFunction(engine, Tr::tr("Invalid prepare script."));
    const ScopedJsValue scriptValue(
                engine->context(),
                JS_Call(engine->context(), function, engine->globalObject(),
                int(argv.size()), argv.data()));
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
    if (JsException ex = engine->checkAndClearException(script.location()))
        throw ex.toErrorInfo();
    commands.clear();
    if (JS_IsArray(engine->context(), scriptValue)) {
        const int count = JS_VALUE_GET_INT(getJsProperty(engine->context(), scriptValue,
                                                         StringConstants::lengthProperty()));
        for (qint32 i = 0; i < count; ++i) {
            ScopedJsValue item(engine->context(),
                               JS_GetPropertyUint32(engine->context(), scriptValue, i));
            if (!JS_IsUninitialized(item) && !JS_IsUndefined(item)) {
                const AbstractCommandPtr cmd = createCommandFromScriptValue(engine, item,
                                                                            script.location());
                if (cmd)
                    commands.addCommand(cmd);
            }
        }
    } else {
        const AbstractCommandPtr cmd = createCommandFromScriptValue(engine, scriptValue,
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
