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

#include "scriptengine.h"

#include "deprecationinfo.h"
#include "filecontextbase.h"
#include "jsimports.h"
#include "preparescriptobserver.h"
#include "scriptimporter.h"

#include <buildgraph/artifact.h>
#include <buildgraph/rulenode.h>
#include <jsextensions/jsextensions.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/profiling.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/scripttools.h>
#include <tools/stlutils.h>
#include <tools/stringconstants.h>
#include <tools/version.h>

#include <QtCore/qdatetime.h>
#include <QtCore/qdebug.h>
#include <QtCore/qdiriterator.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qtextstream.h>
#include <QtCore/qtimer.h>

#include <cstring>
#include <functional>
#include <set>
#include <utility>
#include <vector>

namespace qbs {
namespace Internal {

const bool debugJSImports = false;

bool operator==(const ScriptEngine::PropertyCacheKey &lhs,
        const ScriptEngine::PropertyCacheKey &rhs)
{
    return lhs.m_propertyMap == rhs.m_propertyMap
            && lhs.m_moduleName == rhs.m_moduleName
            && lhs.m_propertyName == rhs.m_propertyName;
}

static QHashValueType combineHash(QHashValueType h1, QHashValueType h2, QHashValueType seed)
{
    // stolen from qHash(QPair)
    return ((h1 << 16) | (h1 >> 16)) ^ h2 ^ seed;
}

QHashValueType qHash(const ScriptEngine::PropertyCacheKey &k, QHashValueType seed = 0)
{
    return combineHash(qHash(k.m_moduleName),
                       combineHash(qHash(k.m_propertyName), qHash(k.m_propertyMap), seed), seed);
}

ScriptEngine::ScriptEngine(Logger &logger, EvalContext evalContext, PrivateTag)
    : m_scriptImporter(new ScriptImporter(this)),
      m_logger(logger), m_evalContext(evalContext),
      m_observer(new PrepareScriptObserver(this, UnobserveMode::Disabled))
{
    setMaxStackSize();
    JS_SetRuntimeOpaque(m_jsRuntime, this);
    JS_SetInterruptHandler(m_jsRuntime, interruptor, this);
    setScopeLookup(m_context, &ScriptEngine::doExtraScopeLookup);
    setFoundUndefinedHandler(m_context, &ScriptEngine::handleUndefinedFound);
    setFunctionEnteredHandler(m_context, &ScriptEngine::handleFunctionEntered);
    setFunctionExitedHandler(m_context, &ScriptEngine::handleFunctionExited);
    m_dataWithPtrClass = registerClass("__data", nullptr, nullptr, JS_UNDEFINED);
    installQbsBuiltins();
    extendJavaScriptBuiltins();
}

std::unique_ptr<ScriptEngine> ScriptEngine::create(Logger &logger, EvalContext evalContext)
{
    return std::make_unique<ScriptEngine>(logger, evalContext, PrivateTag());
}

ScriptEngine *ScriptEngine::engineForRuntime(const JSRuntime *runtime)
{
    return static_cast<ScriptEngine *>(JS_GetRuntimeOpaque(const_cast<JSRuntime *>(runtime)));

}

ScriptEngine *ScriptEngine::engineForContext(const JSContext *ctx)
{
    return engineForRuntime(JS_GetRuntime(const_cast<JSContext *>(ctx)));
}

LookupResult ScriptEngine::doExtraScopeLookup(JSContext *ctx, JSAtom prop)
{
    static const LookupResult fail{JS_UNDEFINED, JS_UNDEFINED, false};

    ScriptEngine * const engine = engineForContext(ctx);
    engine->m_lastLookupWasSuccess = false;

    auto handleScope = [ctx, prop, engine](const JSValue &scope) -> LookupResult {
        const JSValue v = JS_GetProperty(ctx, scope, prop);
        if (!JS_IsUndefined(v) || engine->m_lastLookupWasSuccess) {
            engine->m_lastLookupWasSuccess = false;
            return {v, scope, true};
        }
        return fail;
    };

    if (!engine->m_scopeChains.empty()) {
        const auto scopes = engine->m_scopeChains.back();
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            const auto res = handleScope(*it);
            if (res.useResult)
                return res;
        }
    }
    if (JS_IsObject(engine->m_globalObject))
        return handleScope(engine->m_globalObject);
    return fail;
}

void ScriptEngine::handleUndefinedFound(JSContext *ctx)
{
    engineForContext(ctx)->setLastLookupStatus(true);
}

void ScriptEngine::handleFunctionEntered(JSContext *ctx, JSValue this_obj)
{
    ScriptEngine::engineForContext(ctx)->m_contextStack.push_back(this_obj);
}

void ScriptEngine::handleFunctionExited(JSContext *ctx)
{
    ScriptEngine::engineForContext(ctx)->m_contextStack.pop_back();
}

ScriptEngine::~ScriptEngine()
{
    reset();
    delete m_scriptImporter;
    if (m_elapsedTimeImporting != -1) {
        m_logger.qbsLog(LoggerInfo, true) << Tr::tr("Setting up imports took %1.")
                                             .arg(elapsedTimeString(m_elapsedTimeImporting));
    }
    for (const auto &ext : std::as_const(m_internalExtensions))
        JS_FreeValue(m_context, ext);
    for (const JSValue &s : std::as_const(m_stringCache))
        JS_FreeValue(m_context, s);
    for (JSValue * const externalRef : std::as_const(m_externallyCachedValues)) {
        JS_FreeValue(m_context, *externalRef);
        *externalRef = JS_UNDEFINED;
    }
    setPropertyOnGlobalObject(QLatin1String("console"), JS_UNDEFINED);
    JS_FreeContext(m_context);
    JS_FreeRuntime(m_jsRuntime);
}

void ScriptEngine::reset()
{
    // TODO: Check whether we can keep file and imports cache.
    //       We'd have to find a solution for the scope name problem then.
    clearImportsCache();
    for (const auto &e : std::as_const(m_jsFileCache))
        JS_FreeValue(m_context, e.second);
    m_jsFileCache.clear();

    for (const JSValue &s : std::as_const(m_jsValueCache))
        JS_FreeValue(m_context, s);
    m_jsValueCache.clear();

    for (auto it = m_evalResults.cbegin(); it != m_evalResults.cend(); ++it) {
        for (int i = 0; i < it.value(); ++i)
            JS_FreeValue(m_context, it.key());
    }
    m_evalResults.clear();
    for (const auto &e : std::as_const(m_projectScriptValues))
        JS_FreeValue(m_context, e.second);
    m_projectScriptValues.clear();
    for (const auto &e : std::as_const(m_baseProductScriptValues))
        JS_FreeValue(m_context, e.second);
    m_baseProductScriptValues.clear();
    for (const auto &e : std::as_const(m_productArtifactsMapScriptValues))
        JS_FreeValue(m_context, e.second);
    m_productArtifactsMapScriptValues.clear();
    for (const auto &e : std::as_const(m_moduleArtifactsMapScriptValues))
        JS_FreeValue(m_context, e.second);
    m_moduleArtifactsMapScriptValues.clear();
    for (const auto &e : std::as_const(m_baseModuleScriptValues))
        JS_FreeValue(m_context, e.second);
    m_baseModuleScriptValues.clear();
    for (auto it = m_artifactsScriptValues.cbegin(); it != m_artifactsScriptValues.cend(); ++it) {
        it.key().first->setDeregister({});
        JS_FreeValue(m_context, it.value());
    }
    m_artifactsScriptValues.clear();
    m_logger.clearWarnings();
}

void ScriptEngine::import(const FileContextBaseConstPtr &fileCtx, JSValue &targetObject,
                          ObserveMode observeMode)
{
    Importer(*this, fileCtx, targetObject, observeMode).run();
}

void ScriptEngine::import(const JsImport &jsImport, JSValue &targetObject)
{
    QBS_ASSERT(JS_IsObject(targetObject), return);

    if (debugJSImports)
        qDebug() << "[ENGINE] import into " << jsImport.scopeName;

    JSValue jsImportValue = m_jsImportCache.value(jsImport);
    if (JS_IsObject(jsImportValue)) {
        if (debugJSImports)
            qDebug() << "[ENGINE] " << jsImport.filePaths << " (cache hit)";
    } else {
        if (debugJSImports)
            qDebug() << "[ENGINE] " << jsImport.filePaths << " (cache miss)";

        ScopedJsValue scopedImportValue(m_context, JS_NewObject(m_context));
        for (const QString &filePath : jsImport.filePaths)
            importFile(filePath, scopedImportValue);
        jsImportValue = scopedImportValue.release();
        m_jsImportCache.insert(jsImport, jsImportValue);
        std::vector<QString> &filePathsForScriptValue
                = m_filePathsPerImport[jsObjectId(jsImportValue)];
        transform(jsImport.filePaths, filePathsForScriptValue, [](const auto &fp) {
            return fp; });
    }

    JSValue sv = JS_NewObjectProto(m_context, jsImportValue);
    setJsProperty(m_context, sv, StringConstants::importScopeNamePropertyInternal(),
                  jsImport.scopeName);
    setJsProperty(m_context, targetObject, jsImport.scopeName, sv);
    if (m_observeMode == ObserveMode::Enabled)
        observeImport(jsImportValue);
}

void ScriptEngine::observeImport(JSValue &jsImport)
{
    if (!m_observer->addImportId(quintptr((JS_VALUE_GET_OBJ(jsImport)))))
        return;
    handleJsProperties(jsImport, [this, &jsImport](const JSAtom &name,
                       const JSPropertyDescriptor &desc) {
        if (!JS_IsFunction(m_context, desc.value))
            return;
        const char *const nameStr = JS_AtomToCString(m_context, name);
        setObservedProperty(jsImport, QString::fromUtf8(nameStr, std::strlen(nameStr)), desc.value);
        JS_FreeCString(m_context, nameStr);
    });
}

void ScriptEngine::clearImportsCache()
{
    for (const auto &jsImport : std::as_const(m_jsImportCache))
        JS_FreeValue(m_context, jsImport);
    m_jsImportCache.clear();
    m_filePathsPerImport.clear();
    m_observer->clearImportIds();
}

void ScriptEngine::registerEvaluator(Evaluator *evaluator)
{
    QBS_ASSERT(!m_evaluator, return);
    m_evaluator = evaluator;
}

void ScriptEngine::unregisterEvaluator(const Evaluator *evaluator)
{
    QBS_ASSERT(m_evaluator == evaluator, return);
    m_evaluator = nullptr;
}

void ScriptEngine::checkContext(const QString &operation,
                                const DubiousContextList &dubiousContexts)
{
    for (const DubiousContext &info : dubiousContexts) {
        if (info.context != evalContext())
            continue;
        QString warning;
        switch (info.context) {
        case EvalContext::PropertyEvaluation:
            warning = Tr::tr("Suspicious use of %1 during property evaluation.").arg(operation);
            if (info.suggestion == DubiousContext::SuggestMoving)
                warning += QLatin1Char(' ') + Tr::tr("Should this call be in a Probe instead?");
            break;
        case EvalContext::RuleExecution:
            warning = Tr::tr("Suspicious use of %1 during rule execution.").arg(operation);
            if (info.suggestion == DubiousContext::SuggestMoving) {
                warning += QLatin1Char(' ')
                        + Tr::tr("Should this call be in a JavaScriptCommand instead?");
            }
            break;
        case EvalContext::ModuleProvider:
        case EvalContext::ProbeExecution:
        case EvalContext::JsCommand:
            QBS_ASSERT(false, continue);
            break;
        }
        const JSValue exVal = JS_NewError(m_context);
        const JsException ex(m_context, exVal, JS_GetBacktrace(m_context), {});
        m_logger.printWarning(ErrorInfo(warning, ex.stackTrace()));
        return;
    }
}

void ScriptEngine::handleDeprecation(
    const Version &removalVersion, const QString &message, const CodeLocation &loc)
{
    if (!m_setupParams)
        return;
    switch (m_setupParams->deprecationWarningMode()) {
    case DeprecationWarningMode::Error:
        throw ErrorInfo(message, loc);
    case DeprecationWarningMode::BeforeRemoval:
        if (!DeprecationInfo::isLastVersionBeforeRemoval(removalVersion))
            break;
        [[fallthrough]];
    case DeprecationWarningMode::On:
        m_logger.printWarning(ErrorInfo(message, loc));
        break;
    case DeprecationWarningMode::Off:
        break;
    }
}

void ScriptEngine::addPropertyRequestedFromArtifact(const Artifact *artifact,
                                                    const Property &property)
{
    m_propertiesRequestedFromArtifact[artifact->filePath()] << property;
}

void ScriptEngine::addImportRequestedInScript(quintptr importValueId)
{
    // Import list is assumed to be small, so let's not use a set.
    if (!contains(m_importsRequestedInScript, importValueId))
        m_importsRequestedInScript.push_back(importValueId);
}

std::vector<QString> ScriptEngine::importedFilesUsedInScript() const
{
    std::vector<QString> files;
    for (qint64 usedImport : m_importsRequestedInScript) {
        const auto it = m_filePathsPerImport.find(usedImport);
        QBS_CHECK(it != m_filePathsPerImport.cend());
        const std::vector<QString> &filePathsForImport = it->second;
        for (const QString &fp : filePathsForImport)
            if (!contains(files, fp))
                files.push_back(fp);
    }
    return files;
}

void ScriptEngine::enableProfiling(bool enable)
{
    m_elapsedTimeImporting = enable ? 0 : -1;
}

void ScriptEngine::addToPropertyCache(const QString &moduleName, const QString &propertyName,
        const PropertyMapConstPtr &propertyMap, const QVariant &value)
{
    m_propertyCache.insert(PropertyCacheKey(moduleName, propertyName, propertyMap), value);
}

QVariant ScriptEngine::retrieveFromPropertyCache(const QString &moduleName,
        const QString &propertyName, const PropertyMapConstPtr &propertyMap)
{
    return m_propertyCache.value(PropertyCacheKey(moduleName, propertyName, propertyMap));
}

static JSValue js_observedGet(JSContext *ctx, JSValueConst, int, JSValueConst *, int, JSValue *data)
{
    ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
    engine->observer()->onPropertyRead(data[0], getJsString(ctx, data[1]), data[2]);
    return JS_DupValue(engine->context(), data[2]);
}

void ScriptEngine::setObservedProperty(JSValue &object, const QString &name,
                                       const JSValue &value)
{
    ScopedJsValue jsName(m_context, makeJsString(m_context, name));
    JSValueList funcData{object, jsName, value};
    JSValue getterFunc = JS_NewCFunctionData(m_context, &js_observedGet, 0, 0, 3, funcData.data());
    const ScopedJsAtom nameAtom(m_context, name);
    JS_DefinePropertyGetSet(m_context, object, nameAtom, getterFunc, JS_UNDEFINED, JS_PROP_HAS_GET | JS_PROP_ENUMERABLE);
    if (m_observer->unobserveMode() == UnobserveMode::Enabled)
        m_observedProperties.emplace_back(object, name, value);
}

void ScriptEngine::unobserveProperties()
{
    for (auto &elem : m_observedProperties) {
        JSValue &object = std::get<0>(elem);
        const QString &name = std::get<1>(elem);
        const JSValue &value = std::get<2>(elem);
        const ScopedJsAtom jsName(m_context, name);
        JS_DefineProperty(m_context, object, jsName, value, JS_UNDEFINED, JS_UNDEFINED,
                          JS_PROP_HAS_VALUE);
    }
    m_observedProperties.clear();
}

QProcessEnvironment ScriptEngine::environment() const
{
    return m_environment;
}

void ScriptEngine::setEnvironment(const QProcessEnvironment &env)
{
    m_environment = env;
}

void ScriptEngine::importFile(const QString &filePath, JSValue targetObject)
{
    AccumulatingTimer importTimer(m_elapsedTimeImporting != -1 ? &m_elapsedTimeImporting : nullptr);
    JSValue &evaluationResult = m_jsFileCache[filePath];
    if (JS_IsObject(evaluationResult)) {
        ScriptImporter::copyProperties(m_context, evaluationResult, targetObject);
        return;
    }
    QFile file(filePath);
    if (Q_UNLIKELY(!file.open(QFile::ReadOnly)))
        throw ErrorInfo(Tr::tr("Cannot open '%1'.").arg(filePath));
    QTextStream stream(&file);
    setupDefaultCodec(stream);
    const QString sourceCode = stream.readAll();
    file.close();
    m_currentDirPathStack.push(FileInfo::path(filePath));
    evaluationResult = m_scriptImporter->importSourceCode(sourceCode, filePath, targetObject);
    m_currentDirPathStack.pop();
}

static QString findExtensionDir(const QStringList &searchPaths, const QString &extensionPath)
{
    for (const QString &searchPath : searchPaths) {
        QString dirPath = searchPath + QStringLiteral("/imports/") + extensionPath;
        QFileInfo fi(dirPath);
        if (fi.exists() && fi.isDir())
            return dirPath;
    }
    return {};
}

JSValue ScriptEngine::mergeExtensionObjects(const JSValueList &lst)
{
    JSValue result = JS_UNDEFINED;
    for (const JSValue &v : lst) {
        if (!JS_IsObject(result)) {
            result = v;
            continue;
        }
        ScriptImporter::copyProperties(m_context, v, result);
        JS_FreeValue(m_context, v);
    }
    return result;
}

JSValue ScriptEngine::getInternalExtension(const char *name) const
{
    const auto cached = m_internalExtensions.constFind(QLatin1String(name));
    if (cached != m_internalExtensions.constEnd())
        return JS_DupValue(m_context, cached.value());
    return JS_UNDEFINED;
}

void ScriptEngine::addInternalExtension(const char *name, JSValue ext)
{
    m_internalExtensions.insert(QLatin1String(name), JS_DupValue(m_context, ext));
}

JSValue ScriptEngine::asJsValue(const QVariant &v, quintptr id, bool frozen)
{
    if (v.isNull())
        return JS_UNDEFINED;
    switch (static_cast<QMetaType::Type>(v.userType())) {
    case QMetaType::QByteArray:
        return asJsValue(v.toByteArray());
    case QMetaType::QString:
        return asJsValue(v.toString());
    case QMetaType::QStringList:
        return asJsValue(v.toStringList());
    case QMetaType::QVariantList:
        return asJsValue(v.toList(), id, frozen);
    case QMetaType::Int:
    case QMetaType::UInt:
        return JS_NewInt32(m_context, v.toInt());
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return JS_NewInt64(m_context, v.toInt());
    case QMetaType::Bool:
        return JS_NewBool(m_context, v.toBool());
    case QMetaType::QDateTime:
        return JS_NewDate(m_context, v.toDateTime().toMSecsSinceEpoch());
    case QMetaType::QVariantMap:
        return asJsValue(v.toMap(), id, frozen);
    default:
        return JS_UNDEFINED;
    }
}

JSValue ScriptEngine::asJsValue(const QByteArray &s)
{
    return JS_NewArrayBufferCopy(
        m_context, reinterpret_cast<const uint8_t *>(s.constData()), s.size());
}

JSValue ScriptEngine::asJsValue(const QString &s)
{
    const auto it = m_stringCache.constFind(s);
    if (it != m_stringCache.constEnd())
        return JS_DupValue(m_context, it.value());
    const JSValue sv = JS_NewString(m_context, s.toUtf8().constData());
    m_stringCache.insert(s, sv);
    return JS_DupValue(m_context, sv);
}

JSValue ScriptEngine::asJsValue(const QStringList &l)
{
    JSValue array = JS_NewArray(m_context);
    setJsProperty(m_context, array, std::string_view("length"), JS_NewInt32(m_context, l.size()));
    for (int i = 0; i < l.size(); ++i)
        JS_SetPropertyUint32(m_context, array, i, asJsValue(l.at(i)));
    return array;
}

JSValue ScriptEngine::asJsValue(const QVariantMap &m, quintptr id, bool frozen)
{
    const auto it = id ? m_jsValueCache.constFind(id) : m_jsValueCache.constEnd();
    if (it != m_jsValueCache.constEnd())
        return JS_DupValue(m_context, it.value());
    frozen = id || frozen;
    JSValue obj = JS_NewObject(m_context);
    for (auto it = m.begin(); it != m.end(); ++it)
        setJsProperty(m_context, obj, it.key(), asJsValue(it.value(), 0, frozen));
    if (frozen)
        JS_FreezeObject(m_context, obj);
    if (!id)
        return obj;
    m_jsValueCache[id] = obj;
    return JS_DupValue(m_context, obj);
}

void ScriptEngine::setPropertyOnGlobalObject(const QString &property, JSValue value)
{
    const ScopedJsValue globalObject(m_context, JS_GetGlobalObject(m_context));
    setJsProperty(m_context, globalObject, property, value);
}

JSValue ScriptEngine::asJsValue(const QVariantList &l, quintptr id, bool frozen)
{
    const auto it = id ? m_jsValueCache.constFind(id) : m_jsValueCache.constEnd();
    if (it != m_jsValueCache.constEnd())
        return JS_DupValue(m_context, it.value());
    frozen = id || frozen;
    JSValue array = JS_NewArray(m_context);
    setJsProperty(m_context, array, std::string_view("length"), JS_NewInt32(m_context, l.size()));
    for (int i = 0; i < l.size(); ++i)
        JS_SetPropertyUint32(m_context, array, i, asJsValue(l.at(i), 0, frozen));
    if (frozen)
        JS_FreezeObject(m_context, array);
    if (!id)
        return array;
    m_jsValueCache[id] = array;
    return JS_DupValue(m_context, array);
}

JSValue ScriptEngine::loadInternalExtension(const QString &uri)
{
    const QString name = uri.mid(4);  // remove the "qbs." part
    const auto cached = m_internalExtensions.constFind(name);
    if (cached != m_internalExtensions.constEnd())
        return cached.value();
    JSValue extensionObj = JsExtensions::loadExtension(this, name);
    if (!JS_IsObject(extensionObj))
        return throwError(Tr::tr("loadExtension: cannot load extension '%1'.").arg(uri));
    m_internalExtensions.insert(name, extensionObj);
    return extensionObj;
}

JSValue ScriptEngine::js_require(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv, int, JSValue *func_data)
{
    Q_UNUSED(this_val)

    ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
    QBS_ASSERT(engine, return JS_EXCEPTION);
    if (argc < 1)
        return engine->throwError(Tr::tr("The require function requires a module name or path."));

    const QString moduleName = getJsString(ctx, argv[0]);

    // First try to load a named module if the argument doesn't look like a file path
    if (!moduleName.contains(QLatin1Char('/'))) {
        if (engine->m_extensionSearchPathsStack.empty())
            return engine->throwError(Tr::tr("require: internal error. No search paths."));

        if (engine->m_logger.debugEnabled()) {
            engine->m_logger.qbsDebug()
                    << "[require] loading extension " << moduleName;
        }

        QString moduleNameAsPath = moduleName;
        moduleNameAsPath.replace(QLatin1Char('.'), QLatin1Char('/'));
        const QStringList searchPaths = engine->m_extensionSearchPathsStack.top();
        const QString dirPath = findExtensionDir(searchPaths, moduleNameAsPath);
        if (dirPath.isEmpty()) {
            if (moduleName.startsWith(QStringLiteral("qbs.")))
                return JS_DupValue(ctx, engine->loadInternalExtension(moduleName));
        } else {
            QDirIterator dit(dirPath, StringConstants::jsFileWildcards(),
                             QDir::Files | QDir::Readable);
            JSValueList values;
            std::vector<QString> filePaths;
            try {
                while (dit.hasNext()) {
                    const QString filePath = dit.next();
                    if (engine->m_logger.debugEnabled()) {
                        engine->m_logger.qbsDebug()
                                << "[require] importing file " << filePath;
                    }
                    ScopedJsValue obj(engine->context(), engine->newObject());
                    engine->importFile(filePath, obj);
                    values << obj.release();
                    filePaths.push_back(filePath);
                }
            } catch (const ErrorInfo &e) {
                return engine->throwError(e.toString());
            }

            if (!values.empty()) {
                const JSValue mergedValue = engine->mergeExtensionObjects(values);
                engine->m_requireResults.push_back(mergedValue);
                engine->m_filePathsPerImport[jsObjectId(mergedValue)] = filePaths;
                return mergedValue;
            }
        }

        // The module name might be a file name component, which is assumed to be to a JavaScript
        // file located in the current directory search path; try that next
    }

    if (engine->m_currentDirPathStack.empty())
        return engine->throwError(Tr::tr("require: internal error. No current directory."));

    JSValue result;
    try {
        const QString filePath = FileInfo::resolvePath(engine->m_currentDirPathStack.top(),
                                                       moduleName);
        static const QString scopeNamePrefix = QStringLiteral("_qbs_scope_");
        const QString scopeName = scopeNamePrefix + QString::number(qHash(filePath), 16);
        result = getJsProperty(ctx, func_data[0], scopeName);
        if (JS_IsObject(result))
            return result; // Same JS file imported from same qbs file via different JS files (e.g. codesign.js from DarwinGCC.qbs via gcc.js and darwin.js).
        ScopedJsValue scopedResult(engine->context(), engine->newObject());
        engine->importFile(filePath, scopedResult);
        result = scopedResult.release();
        setJsProperty(ctx, result, StringConstants::importScopeNamePropertyInternal(), scopeName);
        setJsProperty(ctx, func_data[0], scopeName, result);
        engine->m_requireResults.push_back(result);
        engine->m_filePathsPerImport[jsObjectId(JS_DupValue(ctx, result))] = { filePath };
    } catch (const ErrorInfo &e) {
        result = engine->throwError(e.toString());
    }

    return result;
}

JSClassID ScriptEngine::modulePropertyScriptClass() const
{
    return m_modulePropertyScriptClass;
}

void ScriptEngine::setModulePropertyScriptClass(JSClassID modulePropertyScriptClass)
{
    m_modulePropertyScriptClass = modulePropertyScriptClass;
}

template<typename T> JSValue getScriptValue(JSContext *ctx, const T *t,
                                            const std::unordered_map<const T *, JSValue> &map)
{
    const auto it = map.find(t);
    return it == map.end() ? JS_UNDEFINED : JS_DupValue(ctx, it->second);
}

template<typename T> void setScriptValue(JSContext *ctx, const T *t, JSValue value,
                                         std::unordered_map<const T *, JSValue> &map)
{
    value = JS_DupValue(ctx, value);
    const auto it = map.find(t);
    if (it == map.end()) {
        map.insert(std::make_pair(t, value));
        return;
    }
    JS_FreeValue(ctx, it->second);
    it->second = value;
}

JSValue ScriptEngine::artifactsMapScriptValue(const ResolvedProduct *product)
{
    return getScriptValue(m_context, product, m_productArtifactsMapScriptValues);
}

void ScriptEngine::setArtifactsMapScriptValue(const ResolvedProduct *product, JSValue value)
{
    setScriptValue(m_context, product, value, m_productArtifactsMapScriptValues);
}

JSValue ScriptEngine::artifactsMapScriptValue(const ResolvedModule *module)
{
    return getScriptValue(m_context, module, m_moduleArtifactsMapScriptValues);
}

void ScriptEngine::setArtifactsMapScriptValue(const ResolvedModule *module, JSValue value)
{
    setScriptValue(m_context, module, value, m_moduleArtifactsMapScriptValues);
}

JSValue ScriptEngine::getArtifactProperty(JSValue obj,
                                          const std::function<JSValue (const Artifact *)> &propGetter)
{
    std::lock_guard lock(m_artifactsMutex);
    const Artifact * const a = attachedPointer<Artifact>(obj, dataWithPtrClass());
    return a ? propGetter(a) : JS_EXCEPTION;
}

void ScriptEngine::addCanonicalFilePathResult(const QString &filePath,
                                              const QString &resultFilePath)
{
    if (gatherFileResults())
        m_canonicalFilePathResult.insert(filePath, resultFilePath);
}

void ScriptEngine::addFileExistsResult(const QString &filePath, bool exists)
{
    if (gatherFileResults())
        m_fileExistsResult.insert(filePath, exists);
}

void ScriptEngine::addDirectoryEntriesResult(const QString &path, QDir::Filters filters,
                                             const QStringList &entries)
{
    if (gatherFileResults()) {
        m_directoryEntriesResult.insert(
                    std::pair<QString, quint32>(path, static_cast<quint32>(filters)),
                    entries);
    }
}

void ScriptEngine::addFileLastModifiedResult(const QString &filePath, const FileTime &fileTime)
{
    if (gatherFileResults())
        m_fileLastModifiedResult.insert(filePath, fileTime);
}

Set<QString> ScriptEngine::imports() const
{
    Set<QString> filePaths;
    for (auto it = m_jsImportCache.cbegin(); it != m_jsImportCache.cend(); ++it) {
        const JsImport &jsImport = it.key();
        for (const QString &filePath : jsImport.filePaths)
            filePaths << filePath;
    }
    for (const auto &kv : m_filePathsPerImport) {
        for (const QString &fp : kv.second)
            filePaths << fp;
    }
    return filePaths;
}

JSValue ScriptEngine::newObject() const
{
    return JS_NewObject(m_context);
}

JSValue ScriptEngine::newArray(int length, JsValueOwner owner)
{
    JSValue arr = JS_NewArray(m_context);
    JS_SetPropertyStr(m_context, arr, "length", JS_NewInt32(m_context, length));
    if (owner == JsValueOwner::ScriptEngine)
        ++m_evalResults[arr];
    return arr;
}

JSValue ScriptEngine::evaluate(
    JsValueOwner resultOwner,
    const QString &code,
    const QString &filePath,
    int line,
    qbs::Internal::span<const JSValue> scopeChain)
{
    m_scopeChains.emplace_back(scopeChain);
    const QByteArray &codeStr = code.toUtf8();
    const QByteArray &filePathStr = filePath.toUtf8();

    m_evalPositions.emplace(filePath, line);
    JSEvalOptions evalOptions{1, JS_EVAL_TYPE_GLOBAL, filePathStr.constData(), line};
    const JSValue v = JS_EvalThis2(
        m_context, globalObject(), codeStr.constData(), codeStr.length(), &evalOptions);
    m_evalPositions.pop();
    m_scopeChains.pop_back();
    if (resultOwner == JsValueOwner::ScriptEngine && JS_VALUE_HAS_REF_COUNT(v))
        ++m_evalResults[v];
    return v;
}

void ScriptEngine::handleJsProperties(JSValue obj, const PropertyHandler &handler)
{
    qbs::Internal::handleJsProperties(m_context, obj, handler);
}

ScopedJsValueList ScriptEngine::argumentList(const QStringList &argumentNames,
                                             const JSValue &context) const
{
    JSValueList result;
    for (const auto &name : argumentNames)
        result.push_back(getJsProperty(m_context, context, name));
    return {m_context, result};
}

JSClassID ScriptEngine::registerClass(const char *name, JSClassCall *constructor,
                                      JSClassFinalizer *finalizer, JSValue scope,
                                      GetPropertyNames getPropertyNames, GetProperty getProperty)
{
    JSClassID id = 0;
    const auto classIt = m_classes.constFind(QLatin1String(name));
    if (classIt == m_classes.constEnd()) {
        JS_NewClassID(m_jsRuntime, &id);
        const auto it = getProperty
                ? m_exoticMethods.insert(id, JSClassExoticMethods{getProperty, getPropertyNames})
                : m_exoticMethods.end();
        JSClassDef jsClass{name, finalizer, nullptr, constructor,
                    it != m_exoticMethods.end() ? &it.value() : nullptr};
        const int status = JS_NewClass(m_jsRuntime, id, &jsClass);
        QBS_ASSERT(status == 0, return 0);
                       m_classes.insert(QLatin1String(name), id);
    } else {
        id = classIt.value();
    }
    if (!JS_IsUndefined(scope)) {
        const JSValue classObj = JS_NewObjectClass(m_context, id);
        JS_SetConstructorBit(m_context, classObj, constructor != nullptr);
        JS_SetPropertyStr(m_context, scope, name, classObj);
    }
    return id;
}

JSClassID ScriptEngine::getClassId(const char *name) const
{
    return m_classes.value(QLatin1String(name));
}

bool ScriptEngine::checkForJsError(const CodeLocation &currentLocation)
{
    if (m_jsError.hasError()) {
        m_jsError.addOrPrependLocation(currentLocation);
        return true;
    }
    if (const JsException ex = checkAndClearException(currentLocation)) {
        m_jsError = ex.toErrorInfo();
        return true;
    }
    return false;
}

ErrorInfo ScriptEngine::getAndClearJsError()
{
    const ErrorInfo e = m_jsError;
    QBS_CHECK(e.hasError());
    m_jsError.clear();
    return e;
}

void ScriptEngine::throwOnJsError(const CodeLocation &fallbackLocation)
{
    if (checkForJsError(fallbackLocation))
        throwAndClearJsError();
}

JSValue ScriptEngine::throwError(const QString &message) const
{
    return qbs::Internal::throwError(m_context, message);
}

void ScriptEngine::cancel()
{
    m_canceling = true;
}

int ScriptEngine::interruptor(JSRuntime *, void *opaqueEngine)
{
    const auto engine = reinterpret_cast<ScriptEngine *>(opaqueEngine);
    if (engine->m_canceling) {
        engine->m_canceling = false;
        return 1;
    }
    return 0;
}

bool ScriptEngine::gatherFileResults() const
{
    return evalContext() == EvalContext::PropertyEvaluation
            || evalContext() == EvalContext::ProbeExecution;
}

void ScriptEngine::setMaxStackSize()
{
    size_t stackSize = 0; // Turn check off by default.
    bool ok;
    const int stackSizeFromEnv = qEnvironmentVariableIntValue("QBS_MAX_JS_STACK_SIZE", &ok);
    if (ok && stackSizeFromEnv >= 0)
        stackSize = stackSizeFromEnv;
    JS_SetMaxStackSize(m_jsRuntime, stackSize);
}

JSValue ScriptEngine::getArtifactScriptValue(Artifact *a, const QString &moduleName,
                                             const std::function<void(JSValue obj)> &setup)
{
    std::lock_guard lock(m_artifactsMutex);
    const auto it = m_artifactsScriptValues.constFind(qMakePair(a, moduleName));
    if (it != m_artifactsScriptValues.constEnd())
        return JS_DupValue(m_context, *it);
    a->setDeregister([this](const Artifact *a) {
        const std::lock_guard lock(m_artifactsMutex);
        for (auto it = m_artifactsScriptValues.begin(); it != m_artifactsScriptValues.end(); ) {
            if (it.key().first == a) {
                JS_SetOpaque(it.value(), nullptr);
                JS_FreeValue(m_context, it.value());
                it = m_artifactsScriptValues.erase(it);
            } else {
                ++it;
            }
        }
    });
    JSValue obj = JS_NewObjectClass(context(), dataWithPtrClass());
    attachPointerTo(obj, a);
    setup(obj);
    m_artifactsScriptValues.insert(qMakePair(a, moduleName), JS_DupValue(m_context, obj));
    return obj;
}

void ScriptEngine::releaseInputArtifactScriptValues(const RuleNode *ruleNode)
{
    for (auto it = m_artifactsScriptValues.begin(); it != m_artifactsScriptValues.end();) {
        Artifact * const a = it.key().first;
        if (ruleNode->children.contains(a)) {
            a->setDeregister({});
            JS_FreeValue(m_context, it.value());
            it = m_artifactsScriptValues.erase(it);
        } else {
            ++it;
        }
    }
}

class JSTypeExtender
{
public:
    JSTypeExtender(ScriptEngine *engine, const QString &typeName) : m_engine(engine)
    {
        const ScopedJsValue globalObject(engine->context(), JS_GetGlobalObject(engine->context()));
        const ScopedJsValue type(engine->context(), getJsProperty(engine->context(),
                                                                  globalObject, typeName));
        m_proto = getJsProperty(engine->context(), type, QStringLiteral("prototype"));
        QBS_ASSERT(JS_IsObject(m_proto), return);
    }

    ~JSTypeExtender()
    {
        JS_FreeValue(m_engine->context(), m_proto);
    }

    void addFunction(const QString &name, const QString &code)
    {
        const JSValue f = m_engine->evaluate(JsValueOwner::Caller, code);
        QBS_ASSERT(JS_IsFunction(m_engine->context(), f), return);
        JS_DefinePropertyValueStr(m_engine->context(), m_proto, name.toUtf8().constData(), f, 0);
    }

private:
    ScriptEngine * const m_engine;
    JSValue m_proto = JS_UNDEFINED;
};

static JSValue js_consoleFunc(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv,
                              int level)
{
    ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
    QBS_ASSERT(engine, return JS_EXCEPTION);
    if (Q_UNLIKELY(argc != 1))
        return engine->throwError(Tr::tr("The console functions expect 1 argument."));
    engine->logger().qbsLog(static_cast<LoggerLevel>(level)) << getJsString(ctx, argv[0]);
    return engine->undefinedValue();
}

void ScriptEngine::installQbsBuiltins()
{
    const JSValue consoleObj = newObject();
    setPropertyOnGlobalObject(QLatin1String("console"), consoleObj);
    installConsoleFunction(consoleObj, QStringLiteral("debug"), LoggerDebug);
    installConsoleFunction(consoleObj, QStringLiteral("error"), LoggerError);
    installConsoleFunction(consoleObj, QStringLiteral("info"), LoggerInfo);
    installConsoleFunction(consoleObj, QStringLiteral("log"), LoggerDebug);
    installConsoleFunction(consoleObj, QStringLiteral("warn"), LoggerWarning);
}

void ScriptEngine::extendJavaScriptBuiltins()
{
    JSTypeExtender arrayExtender(this, QStringLiteral("Array"));
    arrayExtender.addFunction(QStringLiteral("contains"),
        QStringLiteral("(function(e){return this.indexOf(e) !== -1;})"));
    arrayExtender.addFunction(QStringLiteral("containsAll"),
        QStringLiteral("(function(e){var $this = this;"
                        "return e.every(function (v) { return $this.contains(v) });})"));
    arrayExtender.addFunction(QStringLiteral("containsAny"),
        QStringLiteral("(function(e){var $this = this;"
                        "return e.some(function (v) { return $this.contains(v) });})"));
    arrayExtender.addFunction(QStringLiteral("uniqueConcat"),
        QStringLiteral("(function(other){"
                        "var r = this.concat();"
                        "var s = {};"
                        "r.forEach(function(x){ s[x] = true; });"
                        "other.forEach(function(x){"
                            "if (!s[x]) {"
                              "s[x] = true;"
                              "r.push(x);"
                            "}"
                        "});"
                        "return r;})"));

    JSTypeExtender stringExtender(this, QStringLiteral("String"));
    stringExtender.addFunction(QStringLiteral("contains"),
        QStringLiteral("(function(e){return this.indexOf(e) !== -1;})"));
}

void ScriptEngine::installConsoleFunction(JSValue consoleObj, const QString &name,
                                          LoggerLevel level)
{
    JS_SetPropertyStr(m_context, consoleObj, name.toUtf8().constData(),
            JS_NewCFunctionMagic(m_context, js_consoleFunc, name.toUtf8().constData(), 1,
                                 JS_CFUNC_generic_magic, level));
}

static QString requireString() { return QStringLiteral("require"); }

void ScriptEngine::installImportFunctions(JSValue importScope)
{
    const JSValue require = JS_NewCFunctionData(m_context, js_require, 1, 0, 1, &importScope);
    setPropertyOnGlobalObject(requireString(), require);
}

void ScriptEngine::uninstallImportFunctions()
{
    setPropertyOnGlobalObject(requireString(), JS_UNDEFINED);
}

ScriptEngine::PropertyCacheKey::PropertyCacheKey(QString moduleName,
        QString propertyName, PropertyMapConstPtr propertyMap)
    : m_moduleName(std::move(moduleName))
    , m_propertyName(std::move(propertyName))
    , m_propertyMap(std::move(propertyMap))
{
}

JsException ScriptEngine::checkAndClearException(const CodeLocation &fallbackLocation) const
{
    return {m_context, JS_GetException(m_context), JS_GetBacktrace(m_context), fallbackLocation};
}

void ScriptEngine::clearRequestedProperties()
{
    m_propertiesRequestedInScript.clear();
    m_propertiesRequestedFromArtifact.clear();
    m_importsRequestedInScript.clear();
    m_productsWithRequestedDependencies.clear();
    m_requestedArtifacts.clear();
    m_requestedExports.clear();
};

void ScriptEngine::takeOwnership(JSValue v)
{
    ++m_evalResults[v];
}

ScriptEngine::Importer::Importer(
            ScriptEngine &engine, const FileContextBaseConstPtr &fileCtx, JSValue &targetObject,
            ObserveMode observeMode)
    : m_engine(engine), m_fileCtx(fileCtx), m_targetObject(targetObject)
{
    m_engine.installImportFunctions(targetObject);
    m_engine.m_currentDirPathStack.push(FileInfo::path(fileCtx->filePath()));
    m_engine.m_extensionSearchPathsStack.push(fileCtx->searchPaths());
    m_engine.m_observeMode = observeMode;
}

ScriptEngine::Importer::~Importer()
{
    m_engine.m_requireResults.clear();
    m_engine.m_currentDirPathStack.pop();
    m_engine.m_extensionSearchPathsStack.pop();
    m_engine.uninstallImportFunctions();
}

void ScriptEngine::Importer::run()
{
    for (const JsImport &jsImport : m_fileCtx->jsImports())
        m_engine.import(jsImport, m_targetObject);
    if (m_engine.m_observeMode == ObserveMode::Enabled) {
        for (JSValue &sv : m_engine.m_requireResults)
            m_engine.observeImport(sv);
    }
}

} // namespace Internal
} // namespace qbs
