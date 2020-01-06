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

#include "filecontextbase.h"
#include "jsimports.h"
#include "propertymapinternal.h"
#include "scriptimporter.h"
#include "preparescriptobserver.h"

#include <buildgraph/artifact.h>
#include <jsextensions/jsextensions.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/profiling.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/stlutils.h>
#include <tools/stringconstants.h>

#include <QtCore/qdebug.h>
#include <QtCore/qdiriterator.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qtextstream.h>
#include <QtCore/qtimer.h>

#include <QtScript/qscriptclass.h>
#include <QtScript/qscriptvalueiterator.h>

#include <functional>
#include <set>
#include <utility>

namespace qbs {
namespace Internal {

static QString getterFuncHelperProperty() { return QStringLiteral("qbsdata"); }

const bool debugJSImports = false;

bool operator==(const ScriptEngine::PropertyCacheKey &lhs,
        const ScriptEngine::PropertyCacheKey &rhs)
{
    return lhs.m_propertyMap == rhs.m_propertyMap
            && lhs.m_moduleName == rhs.m_moduleName
            && lhs.m_propertyName == rhs.m_propertyName;
}

static inline uint combineHash(uint h1, uint h2, uint seed)
{
    // stolen from qHash(QPair)
    return ((h1 << 16) | (h1 >> 16)) ^ h2 ^ seed;
}

uint qHash(const ScriptEngine::PropertyCacheKey &k, uint seed = 0)
{
    return combineHash(qHash(k.m_moduleName),
                       combineHash(qHash(k.m_propertyName), qHash(k.m_propertyMap), seed), seed);
}

std::mutex ScriptEngine::m_creationDestructionMutex;

ScriptEngine::ScriptEngine(Logger &logger, EvalContext evalContext, QObject *parent)
    : QScriptEngine(parent), m_scriptImporter(new ScriptImporter(this)),
      m_modulePropertyScriptClass(nullptr),
      m_propertyCacheEnabled(true), m_active(false), m_logger(logger), m_evalContext(evalContext),
      m_observer(new PrepareScriptObserver(this, UnobserveMode::Disabled))
{
    setProcessEventsInterval(1000); // For the cancelation mechanism to work.
    m_cancelationError = currentContext()->throwValue(tr("Execution canceled"));
    QScriptValue objectProto = globalObject().property(QStringLiteral("Object"));
    m_definePropertyFunction = objectProto.property(QStringLiteral("defineProperty"));
    QBS_ASSERT(m_definePropertyFunction.isFunction(), /* ignore */);
    m_emptyFunction = evaluate(QStringLiteral("(function(){})"));
    QBS_ASSERT(m_emptyFunction.isFunction(), /* ignore */);
    // Initially push a new context to turn off scope chain insanity mode.
    QScriptEngine::pushContext();
    installQbsBuiltins();
    extendJavaScriptBuiltins();
}

ScriptEngine *ScriptEngine::create(Logger &logger, EvalContext evalContext, QObject *parent)
{
    std::lock_guard<std::mutex> lock(m_creationDestructionMutex);
    return new ScriptEngine(logger, evalContext, parent);
}

ScriptEngine::~ScriptEngine()
{
    m_creationDestructionMutex.lock();
    connect(this, &QObject::destroyed, std::bind(&std::mutex::unlock, &m_creationDestructionMutex));

    releaseResourcesOfScriptObjects();
    delete (m_scriptImporter);
    if (m_elapsedTimeImporting != -1) {
        m_logger.qbsLog(LoggerInfo, true) << Tr::tr("Setting up imports took %1.")
                                             .arg(elapsedTimeString(m_elapsedTimeImporting));
    }
    delete m_modulePropertyScriptClass;
    delete m_productPropertyScriptClass;
}

void ScriptEngine::import(const FileContextBaseConstPtr &fileCtx, QScriptValue &targetObject,
                          ObserveMode observeMode)
{
    installImportFunctions();
    m_currentDirPathStack.push(FileInfo::path(fileCtx->filePath()));
    m_extensionSearchPathsStack.push(fileCtx->searchPaths());
    m_observeMode = observeMode;

    for (const JsImport &jsImport : fileCtx->jsImports())
        import(jsImport, targetObject);
    if (m_observeMode == ObserveMode::Enabled) {
        for (QScriptValue &sv : m_requireResults)
            observeImport(sv);
        m_requireResults.clear();
    }

    m_currentDirPathStack.pop();
    m_extensionSearchPathsStack.pop();
    uninstallImportFunctions();
}

void ScriptEngine::import(const JsImport &jsImport, QScriptValue &targetObject)
{
    QBS_ASSERT(targetObject.isObject(), return);
    QBS_ASSERT(targetObject.engine() == this, return);

    if (debugJSImports)
        qDebug() << "[ENGINE] import into " << jsImport.scopeName;

    QScriptValue jsImportValue = m_jsImportCache.value(jsImport);
    if (jsImportValue.isValid()) {
        if (debugJSImports)
            qDebug() << "[ENGINE] " << jsImport.filePaths << " (cache hit)";
    } else {
        if (debugJSImports)
            qDebug() << "[ENGINE] " << jsImport.filePaths << " (cache miss)";
        jsImportValue = newObject();
        for (const QString &filePath : jsImport.filePaths)
            importFile(filePath, jsImportValue);
        m_jsImportCache.insert(jsImport, jsImportValue);
        std::vector<QString> &filePathsForScriptValue
                = m_filePathsPerImport[jsImportValue.objectId()];
        for (const QString &fp : jsImport.filePaths)
            filePathsForScriptValue.push_back(fp);
    }

    QScriptValue sv = newObject();
    sv.setPrototype(jsImportValue);
    sv.setProperty(StringConstants::importScopeNamePropertyInternal(), jsImport.scopeName);
    targetObject.setProperty(jsImport.scopeName, sv);
    if (m_observeMode == ObserveMode::Enabled)
        observeImport(jsImportValue);
}

void ScriptEngine::observeImport(QScriptValue &jsImport)
{
    if (!m_observer->addImportId(jsImport.objectId()))
        return;
    QScriptValueIterator it(jsImport);
    while (it.hasNext()) {
        it.next();
        if (it.flags() & QScriptValue::PropertyGetter)
            continue;
        QScriptValue property = it.value();
        if (!property.isFunction())
            continue;
        setObservedProperty(jsImport, it.name(), property);
    }
}

void ScriptEngine::clearImportsCache()
{
    m_jsImportCache.clear();
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
        m_logger.printWarning(ErrorInfo(warning, currentContext()->backtrace()));
        return;
    }
}

void ScriptEngine::addPropertyRequestedFromArtifact(const Artifact *artifact,
                                                    const Property &property)
{
    m_propertiesRequestedFromArtifact[artifact->filePath()] << property;
}

void ScriptEngine::addImportRequestedInScript(qint64 importValueId)
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

void ScriptEngine::defineProperty(QScriptValue &object, const QString &name,
                                  const QScriptValue &descriptor)
{
    QScriptValue arguments = newArray();
    arguments.setProperty(0, object);
    arguments.setProperty(1, name);
    arguments.setProperty(2, descriptor);
    QScriptValue result = m_definePropertyFunction.call(QScriptValue(), arguments);
    QBS_ASSERT(!hasErrorOrException(result), qDebug() << name << result.toString());
}

static QScriptValue js_observedGet(QScriptContext *context, QScriptEngine *,
                                   ScriptPropertyObserver * const observer)
{
    const QScriptValue data = context->callee().property(getterFuncHelperProperty());
    const QScriptValue value = data.property(2);
    observer->onPropertyRead(data.property(0), data.property(1).toVariant().toString(), value);
    return value;
}

void ScriptEngine::setObservedProperty(QScriptValue &object, const QString &name,
                                       const QScriptValue &value)
{
    QScriptValue data = newArray();
    data.setProperty(0, object);
    data.setProperty(1, name);
    data.setProperty(2, value);
    QScriptValue getterFunc = newFunction(js_observedGet,
                                          static_cast<ScriptPropertyObserver *>(m_observer.get()));
    getterFunc.setProperty(getterFuncHelperProperty(), data);
    object.setProperty(name, getterFunc, QScriptValue::PropertyGetter);
    if (m_observer->unobserveMode() == UnobserveMode::Enabled)
        m_observedProperties.emplace_back(object, name, value);
}

void ScriptEngine::unobserveProperties()
{
    for (auto &elem : m_observedProperties) {
        QScriptValue &object = std::get<0>(elem);
        const QString &name = std::get<1>(elem);
        const QScriptValue &value = std::get<2>(elem);
        object.setProperty(name, QScriptValue(), QScriptValue::PropertyGetter);
        object.setProperty(name, value, QScriptValue::PropertyFlags());
    }
    m_observedProperties.clear();
}

static QScriptValue js_deprecatedGet(QScriptContext *context, QScriptEngine *qtengine)
{
    const auto engine = static_cast<const ScriptEngine *>(qtengine);
    const QScriptValue data = context->callee().property(getterFuncHelperProperty());
    engine->logger().qbsWarning()
            << ScriptEngine::tr("Property %1 is deprecated. Please use %2 instead.").arg(
                   data.property(0).toString(), data.property(1).toString());
    return data.property(2);
}

void ScriptEngine::setDeprecatedProperty(QScriptValue &object, const QString &oldName,
        const QString &newName, const QScriptValue &value)
{
    QScriptValue data = newArray();
    data.setProperty(0, oldName);
    data.setProperty(1, newName);
    data.setProperty(2, value);
    QScriptValue getterFunc = newFunction(js_deprecatedGet);
    getterFunc.setProperty(getterFuncHelperProperty(), data);
    object.setProperty(oldName, getterFunc, QScriptValue::PropertyGetter
                       | QScriptValue::SkipInEnumeration);
}

QProcessEnvironment ScriptEngine::environment() const
{
    return m_environment;
}

void ScriptEngine::setEnvironment(const QProcessEnvironment &env)
{
    m_environment = env;
}

void ScriptEngine::importFile(const QString &filePath, QScriptValue &targetObject)
{
    AccumulatingTimer importTimer(m_elapsedTimeImporting != -1 ? &m_elapsedTimeImporting : nullptr);
    QScriptValue &evaluationResult = m_jsFileCache[filePath];
    if (evaluationResult.isValid()) {
        ScriptImporter::copyProperties(evaluationResult, targetObject);
        return;
    }
    QFile file(filePath);
    if (Q_UNLIKELY(!file.open(QFile::ReadOnly)))
        throw ErrorInfo(tr("Cannot open '%1'.").arg(filePath));
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    const QString sourceCode = stream.readAll();
    file.close();
    m_currentDirPathStack.push(FileInfo::path(filePath));
    evaluationResult = m_scriptImporter->importSourceCode(sourceCode, filePath, targetObject);
    m_currentDirPathStack.pop();
}

static QString findExtensionDir(const QStringList &searchPaths, const QString &extensionPath)
{
    for (const QString &searchPath : searchPaths) {
        const QString dirPath = searchPath + QStringLiteral("/imports/") + extensionPath;
        QFileInfo fi(dirPath);
        if (fi.exists() && fi.isDir())
            return dirPath;
    }
    return {};
}

static QScriptValue mergeExtensionObjects(const QScriptValueList &lst)
{
    QScriptValue result;
    for (const QScriptValue &v : lst) {
        if (!result.isValid()) {
            result = v;
            continue;
        }
        QScriptValueIterator svit(v);
        while (svit.hasNext()) {
            svit.next();
            result.setProperty(svit.name(), svit.value());
        }
    }
    return result;
}

static QScriptValue loadInternalExtension(QScriptContext *context, ScriptEngine *engine,
        const QString &uri)
{
    const QString name = uri.mid(4);  // remove the "qbs." part
    QScriptValue extensionObj = JsExtensions::loadExtension(engine, name);
    if (!extensionObj.isValid()) {
        return context->throwError(ScriptEngine::tr("loadExtension: "
                                                    "cannot load extension '%1'.").arg(uri));
    }
    return extensionObj;
}

QScriptValue ScriptEngine::js_loadExtension(QScriptContext *context, QScriptEngine *qtengine)
{
    if (context->argumentCount() < 1) {
        return context->throwError(
                    ScriptEngine::tr("The loadExtension function requires "
                                     "an extension name."));
    }

    const auto engine = static_cast<const ScriptEngine *>(qtengine);
    ErrorInfo deprWarning(Tr::tr("The loadExtension() function is deprecated and will be "
                                 "removed in a future version of Qbs. Use require() "
                                 "instead."), context->backtrace());
    engine->logger().printWarning(deprWarning);

    return js_require(context, qtengine);
}

QScriptValue ScriptEngine::js_loadFile(QScriptContext *context, QScriptEngine *qtengine)
{
    if (context->argumentCount() < 1) {
        return context->throwError(
                    ScriptEngine::tr("The loadFile function requires a file path."));
    }

    const auto engine = static_cast<const ScriptEngine *>(qtengine);
    ErrorInfo deprWarning(Tr::tr("The loadFile() function is deprecated and will be "
                                 "removed in a future version of Qbs. Use require() "
                                 "instead."), context->backtrace());
    engine->logger().printWarning(deprWarning);

    return js_require(context, qtengine);
}

QScriptValue ScriptEngine::js_require(QScriptContext *context, QScriptEngine *qtengine)
{
    const auto engine = static_cast<ScriptEngine *>(qtengine);
    if (context->argumentCount() < 1) {
        return context->throwError(
                    ScriptEngine::tr("The require function requires a module name or path."));
    }

    const QString moduleName = context->argument(0).toString();

    // First try to load a named module if the argument doesn't look like a file path
    if (!moduleName.contains(QLatin1Char('/'))) {
        if (engine->m_extensionSearchPathsStack.empty())
            return context->throwError(
                        ScriptEngine::tr("require: internal error. No search paths."));

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
                return loadInternalExtension(context, engine, moduleName);
        } else {
            QDirIterator dit(dirPath, StringConstants::jsFileWildcards(),
                             QDir::Files | QDir::Readable);
            QScriptValueList values;
            std::vector<QString> filePaths;
            try {
                while (dit.hasNext()) {
                    const QString filePath = dit.next();
                    if (engine->m_logger.debugEnabled()) {
                        engine->m_logger.qbsDebug()
                                << "[require] importing file " << filePath;
                    }
                    QScriptValue obj = engine->newObject();
                    engine->importFile(filePath, obj);
                    values << obj;
                    filePaths.push_back(filePath);
                }
            } catch (const ErrorInfo &e) {
                return context->throwError(e.toString());
            }

            if (!values.empty()) {
                const QScriptValue mergedValue = mergeExtensionObjects(values);
                engine->m_requireResults.push_back(mergedValue);
                engine->m_filePathsPerImport[mergedValue.objectId()] = filePaths;
                return mergedValue;
            }
        }

        // The module name might be a file name component, which is assumed to be to a JavaScript
        // file located in the current directory search path; try that next
    }

    if (engine->m_currentDirPathStack.empty()) {
        return context->throwError(
            ScriptEngine::tr("require: internal error. No current directory."));
    }

    QScriptValue result;
    try {
        const QString filePath = FileInfo::resolvePath(engine->m_currentDirPathStack.top(),
                                                       moduleName);
        result = engine->newObject();
        engine->importFile(filePath, result);
        static const QString scopeNamePrefix = QStringLiteral("_qbs_scope_");
        const QString scopeName = scopeNamePrefix + QString::number(qHash(filePath), 16);
        result.setProperty(StringConstants::importScopeNamePropertyInternal(), scopeName);
        context->thisObject().setProperty(scopeName, result);
        engine->m_requireResults.push_back(result);
        engine->m_filePathsPerImport[result.objectId()] = { filePath };
    } catch (const ErrorInfo &e) {
        result = context->throwError(e.toString());
    }

    return result;
}

QScriptClass *ScriptEngine::modulePropertyScriptClass() const
{
    return m_modulePropertyScriptClass;
}

void ScriptEngine::setModulePropertyScriptClass(QScriptClass *modulePropertyScriptClass)
{
    m_modulePropertyScriptClass = modulePropertyScriptClass;
}

void ScriptEngine::addResourceAcquiringScriptObject(ResourceAcquiringScriptObject *obj)
{
    m_resourceAcquiringScriptObjects.push_back(obj);
}

void ScriptEngine::releaseResourcesOfScriptObjects()
{
    if (m_resourceAcquiringScriptObjects.empty())
        return;
    std::for_each(m_resourceAcquiringScriptObjects.begin(), m_resourceAcquiringScriptObjects.end(),
                  std::mem_fn(&ResourceAcquiringScriptObject::releaseResources));
    m_resourceAcquiringScriptObjects.clear();
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

QScriptValueList ScriptEngine::argumentList(const QStringList &argumentNames,
        const QScriptValue &context)
{
    QScriptValueList result;
    for (const auto &name : argumentNames)
        result += context.property(name);
    return result;
}

CodeLocation ScriptEngine::lastErrorLocation(const QScriptValue &v,
                                             const CodeLocation &fallbackLocation) const
{
    const QScriptValue &errorVal = lastErrorValue(v);
    const CodeLocation errorLoc(errorVal.property(StringConstants::fileNameProperty()).toString(),
            errorVal.property(QStringLiteral("lineNumber")).toInt32(),
            errorVal.property(QStringLiteral("expressionCaretOffset")).toInt32(),
            false);
    return errorLoc.isValid() ? errorLoc : fallbackLocation;
}

ErrorInfo ScriptEngine::lastError(const QScriptValue &v, const CodeLocation &fallbackLocation) const
{
    const QString msg = lastErrorString(v);
    CodeLocation errorLocation = lastErrorLocation(v);
    if (errorLocation.isValid())
        return ErrorInfo(msg, errorLocation);
    const QStringList backtrace = uncaughtExceptionBacktraceOrEmpty();
    if (!backtrace.empty()) {
        ErrorInfo e(msg, backtrace);
        if (e.hasLocation())
            return e;
    }
    return ErrorInfo(msg, fallbackLocation);
}

void ScriptEngine::cancel()
{
    QTimer::singleShot(0, this, [this] { abort(); });
}

void ScriptEngine::abort()
{
    abortEvaluation(m_cancelationError);
}

bool ScriptEngine::gatherFileResults() const
{
    return evalContext() == EvalContext::PropertyEvaluation
            || evalContext() == EvalContext::ProbeExecution;
}

class JSTypeExtender
{
public:
    JSTypeExtender(ScriptEngine *engine, const QString &typeName)
        : m_engine(engine)
    {
        m_proto = engine->globalObject().property(typeName)
                .property(QStringLiteral("prototype"));
        QBS_ASSERT(m_proto.isObject(), return);
        m_descriptor = engine->newObject();
    }

    void addFunction(const QString &name, const QString &code)
    {
        QScriptValue f = m_engine->evaluate(code);
        QBS_ASSERT(f.isFunction(), return);
        m_descriptor.setProperty(QStringLiteral("value"), f);
        m_engine->defineProperty(m_proto, name, m_descriptor);
    }

private:
    ScriptEngine *const m_engine;
    QScriptValue m_proto;
    QScriptValue m_descriptor;
};

static QScriptValue js_consoleError(QScriptContext *context, QScriptEngine *engine, Logger *logger)
{
    if (Q_UNLIKELY(context->argumentCount() != 1))
        return context->throwError(QScriptContext::SyntaxError,
                                   QStringLiteral("console.error() expects 1 argument"));
    logger->qbsLog(LoggerError) << context->argument(0).toString();
    return engine->undefinedValue();
}

static QScriptValue js_consoleWarn(QScriptContext *context, QScriptEngine *engine, Logger *logger)
{
    if (Q_UNLIKELY(context->argumentCount() != 1))
        return context->throwError(QScriptContext::SyntaxError,
                                   QStringLiteral("console.warn() expects 1 argument"));
    logger->qbsWarning() << context->argument(0).toString();
    return engine->undefinedValue();
}

static QScriptValue js_consoleInfo(QScriptContext *context, QScriptEngine *engine, Logger *logger)
{
    if (Q_UNLIKELY(context->argumentCount() != 1))
        return context->throwError(QScriptContext::SyntaxError,
                                   QStringLiteral("console.info() expects 1 argument"));
    logger->qbsInfo() << context->argument(0).toString();
    return engine->undefinedValue();
}

static QScriptValue js_consoleDebug(QScriptContext *context, QScriptEngine *engine, Logger *logger)
{
    if (Q_UNLIKELY(context->argumentCount() != 1))
        return context->throwError(QScriptContext::SyntaxError,
                                   QStringLiteral("console.debug() expects 1 argument"));
    logger->qbsDebug() << context->argument(0).toString();
    return engine->undefinedValue();
}

static QScriptValue js_consoleLog(QScriptContext *context, QScriptEngine *engine, Logger *logger)
{
    return js_consoleDebug(context, engine, logger);
}

void ScriptEngine::installQbsBuiltins()
{
    globalObject().setProperty(StringConstants::qbsModule(), m_qbsObject = newObject());

    globalObject().setProperty(QStringLiteral("console"), m_consoleObject = newObject());
    installConsoleFunction(QStringLiteral("debug"), &js_consoleDebug);
    installConsoleFunction(QStringLiteral("error"), &js_consoleError);
    installConsoleFunction(QStringLiteral("info"), &js_consoleInfo);
    installConsoleFunction(QStringLiteral("log"), &js_consoleLog);
    installConsoleFunction(QStringLiteral("warn"), &js_consoleWarn);
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
    stringExtender.addFunction(QStringLiteral("startsWith"),
        QStringLiteral("(function(e){return this.slice(0, e.length) === e;})"));
    stringExtender.addFunction(QStringLiteral("endsWith"),
                               QStringLiteral("(function(e){return this.slice(-e.length) === e;})"));
}

void ScriptEngine::installFunction(const QString &name, int length, QScriptValue *functionValue,
        FunctionSignature f, QScriptValue *targetObject = nullptr)
{
    if (!functionValue->isValid())
        *functionValue = newFunction(f, length);
    (targetObject ? *targetObject : globalObject()).setProperty(name, *functionValue);
}

void ScriptEngine::installQbsFunction(const QString &name, int length, FunctionSignature f)
{
    QScriptValue functionValue;
    installFunction(name, length, &functionValue, f, &m_qbsObject);
}

void ScriptEngine::installConsoleFunction(const QString &name,
        QScriptValue (*f)(QScriptContext *, QScriptEngine *, Logger *))
{
    m_consoleObject.setProperty(name, newFunction(f, &m_logger));
}

static QString loadFileString() { return QStringLiteral("loadFile"); }
static QString loadExtensionString() { return QStringLiteral("loadExtension"); }
static QString requireString() { return QStringLiteral("require"); }

void ScriptEngine::installImportFunctions()
{
    installFunction(loadFileString(), 1, &m_loadFileFunction, js_loadFile);
    installFunction(loadExtensionString(), 1, &m_loadExtensionFunction, js_loadExtension);
    installFunction(requireString(), 1, &m_requireFunction, js_require);
}

void ScriptEngine::uninstallImportFunctions()
{
    globalObject().setProperty(loadFileString(), QScriptValue());
    globalObject().setProperty(loadExtensionString(), QScriptValue());
    globalObject().setProperty(requireString(), QScriptValue());
}

ScriptEngine::PropertyCacheKey::PropertyCacheKey(QString moduleName,
        QString propertyName, PropertyMapConstPtr propertyMap)
    : m_moduleName(std::move(moduleName))
    , m_propertyName(std::move(propertyName))
    , m_propertyMap(std::move(propertyMap))
{
}

} // namespace Internal
} // namespace qbs
