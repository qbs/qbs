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
#include "scriptpropertyobserver.h"

#include <buildgraph/artifact.h>
#include <jsextensions/jsextensions.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/profiling.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>

#include <QtCore/qdebug.h>
#include <QtCore/qdiriterator.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qtextstream.h>
#include <QtCore/qtimer.h>

#include <QtScript/qscriptclass.h>
#include <QtScript/qscriptvalueiterator.h>

#include <set>

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

ScriptEngine::ScriptEngine(Logger &logger, EvalContext evalContext, QObject *parent)
    : QScriptEngine(parent), m_scriptImporter(new ScriptImporter(this)),
      m_modulePropertyScriptClass(nullptr),
      m_propertyCacheEnabled(true), m_active(false), m_logger(logger), m_evalContext(evalContext)
{
    setProcessEventsInterval(1000); // For the cancelation mechanism to work.
    m_cancelationError = currentContext()->throwValue(tr("Execution canceled"));
    QScriptValue objectProto = globalObject().property(QLatin1String("Object"));
    m_definePropertyFunction = objectProto.property(QLatin1String("defineProperty"));
    QBS_ASSERT(m_definePropertyFunction.isFunction(), /* ignore */);
    m_emptyFunction = evaluate(QLatin1String("(function(){})"));
    QBS_ASSERT(m_emptyFunction.isFunction(), /* ignore */);
    // Initially push a new context to turn off scope chain insanity mode.
    QScriptEngine::pushContext();
    installQbsBuiltins();
    extendJavaScriptBuiltins();
}

ScriptEngine::~ScriptEngine()
{
    qDeleteAll(m_ownedVariantMaps);
    delete (m_scriptImporter);
    if (m_elapsedTimeImporting != -1) {
        m_logger.qbsLog(LoggerInfo, true) << Tr::tr("Setting up imports took %1.")
                                             .arg(elapsedTimeString(m_elapsedTimeImporting));
    }
    delete m_modulePropertyScriptClass;
}

void ScriptEngine::import(const FileContextBaseConstPtr &fileCtx, QScriptValue &targetObject)
{
    installImportFunctions();
    m_currentDirPathStack.push(FileInfo::path(fileCtx->filePath()));
    m_extensionSearchPathsStack.push(fileCtx->searchPaths());

    for (const JsImport &jsImport : fileCtx->jsImports())
        import(jsImport, targetObject);

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
    }

    QScriptValue sv = newObject();
    sv.setPrototype(jsImportValue);
    sv.setProperty(QLatin1String("_qbs_importScopeName"), jsImport.scopeName);
    targetObject.setProperty(jsImport.scopeName, sv);
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
    const QScriptValue data = context->callee().property(QLatin1String("qbsdata"));
    const QScriptValue value = data.property(2);
    observer->onPropertyRead(data.property(0), data.property(1).toVariant().toString(), value);
    return value;
}

void ScriptEngine::setObservedProperty(QScriptValue &object, const QString &name,
                                       const QScriptValue &value, ScriptPropertyObserver *observer)
{
    if (!observer) {
        object.setProperty(name, value);
        return;
    }

    QScriptValue data = newArray();
    data.setProperty(0, object);
    data.setProperty(1, name);
    data.setProperty(2, value);
    QScriptValue getterFunc = newFunction(js_observedGet, observer);
    getterFunc.setProperty(QLatin1String("qbsdata"), data);
    object.setProperty(name, getterFunc, QScriptValue::PropertyGetter);
}

static QScriptValue js_deprecatedGet(QScriptContext *context, QScriptEngine *qtengine)
{
    ScriptEngine *engine = static_cast<ScriptEngine *>(qtengine);
    const QScriptValue data = context->callee().property(QLatin1String("qbsdata"));
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
    getterFunc.setProperty(QLatin1String("qbsdata"), data);
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
    QFile file(filePath);
    if (Q_UNLIKELY(!file.open(QFile::ReadOnly)))
        throw ErrorInfo(tr("Cannot open '%1'.").arg(filePath));
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    const QString sourceCode = stream.readAll();
    file.close();
    m_currentDirPathStack.push(FileInfo::path(filePath));
    m_scriptImporter->importSourceCode(sourceCode, filePath, targetObject);
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
    return QString();
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

    ScriptEngine *engine = static_cast<ScriptEngine *>(qtengine);
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

    ScriptEngine *engine = static_cast<ScriptEngine *>(qtengine);
    ErrorInfo deprWarning(Tr::tr("The loadFile() function is deprecated and will be "
                                 "removed in a future version of Qbs. Use require() "
                                 "instead."), context->backtrace());
    engine->logger().printWarning(deprWarning);

    return js_require(context, qtengine);
}

QScriptValue ScriptEngine::js_require(QScriptContext *context, QScriptEngine *qtengine)
{
    ScriptEngine *engine = static_cast<ScriptEngine *>(qtengine);
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

        const QString uri = moduleName;
        if (engine->m_logger.debugEnabled()) {
            engine->m_logger.qbsDebug()
                    << "[require] loading extension " << uri;
        }

        QString uriAsPath = uri;
        uriAsPath.replace(QLatin1Char('.'), QLatin1Char('/'));
        const QStringList searchPaths = engine->m_extensionSearchPathsStack.top();
        const QString dirPath = findExtensionDir(searchPaths, uriAsPath);
        if (dirPath.isEmpty()) {
            if (uri.startsWith(QLatin1String("qbs.")))
                return loadInternalExtension(context, engine, uri);
        } else {
            QDirIterator dit(dirPath,
                             QStringList() << QStringLiteral("*.js"),
                             QDir::Files | QDir::Readable);
            QScriptValueList values;
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
                }
            } catch (const ErrorInfo &e) {
                return context->throwError(e.toString());
            }

            if (!values.isEmpty())
                return mergeExtensionObjects(values);
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
        const QString scopeName = QLatin1String("_qbs_scope_")
                + QString::number(qHash(filePath), 16);
        result.setProperty(QLatin1String("_qbs_importScopeName"), scopeName);
        context->thisObject().setProperty(scopeName, result);
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

void ScriptEngine::addEnvironmentVariable(const QString &name, const QString &value)
{
    m_usedEnvironment.insert(name, value);
}

void ScriptEngine::addCanonicalFilePathResult(const QString &filePath,
                                              const QString &resultFilePath)
{
    m_canonicalFilePathResult.insert(filePath, resultFilePath);
}

void ScriptEngine::addFileExistsResult(const QString &filePath, bool exists)
{
    m_fileExistsResult.insert(filePath, exists);
}

void ScriptEngine::addDirectoryEntriesResult(const QString &path, QDir::Filters filters,
                                             const QStringList &entries)
{
    m_directoryEntriesResult.insert(
                std::pair<QString, quint32>(path, static_cast<quint32>(filters)),
                entries);
}

void ScriptEngine::addFileLastModifiedResult(const QString &filePath, const FileTime &fileTime)
{
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
    return filePaths;
}

QScriptValueList ScriptEngine::argumentList(const QStringList &argumentNames,
        const QScriptValue &context)
{
    QScriptValueList result;
    for (int i = 0; i < argumentNames.count(); ++i)
        result += context.property(argumentNames.at(i));
    return result;
}

CodeLocation ScriptEngine::lastErrorLocation(const QScriptValue &v,
                                             const CodeLocation &fallbackLocation) const
{
    const QScriptValue &errorVal = lastErrorValue(v);
    const CodeLocation errorLoc(errorVal.property(QLatin1String("fileName")).toString(),
            errorVal.property(QLatin1String("lineNumber")).toInt32(),
            errorVal.property(QLatin1String("expressionCaretOffset")).toInt32(),
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
    if (!backtrace.isEmpty())
        return ErrorInfo(msg, backtrace);
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

class JSTypeExtender
{
public:
    JSTypeExtender(ScriptEngine *engine, const QString &typeName)
        : m_engine(engine)
    {
        m_proto = engine->globalObject().property(typeName)
                .property(QLatin1String("prototype"));
        QBS_ASSERT(m_proto.isObject(), return);
        m_descriptor = engine->newObject();
    }

    void addFunction(const QString &name, const QString &code)
    {
        QScriptValue f = m_engine->evaluate(code);
        QBS_ASSERT(f.isFunction(), return);
        m_descriptor.setProperty(QLatin1String("value"), f);
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
                                   QLatin1String("console.error() expects 1 argument"));
    logger->qbsLog(LoggerError) << context->argument(0).toString();
    return engine->undefinedValue();
}

static QScriptValue js_consoleWarn(QScriptContext *context, QScriptEngine *engine, Logger *logger)
{
    if (Q_UNLIKELY(context->argumentCount() != 1))
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("console.warn() expects 1 argument"));
    logger->qbsWarning() << context->argument(0).toString();
    return engine->undefinedValue();
}

static QScriptValue js_consoleInfo(QScriptContext *context, QScriptEngine *engine, Logger *logger)
{
    if (Q_UNLIKELY(context->argumentCount() != 1))
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("console.info() expects 1 argument"));
    logger->qbsInfo() << context->argument(0).toString();
    return engine->undefinedValue();
}

static QScriptValue js_consoleDebug(QScriptContext *context, QScriptEngine *engine, Logger *logger)
{
    if (Q_UNLIKELY(context->argumentCount() != 1))
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("console.debug() expects 1 argument"));
    logger->qbsDebug() << context->argument(0).toString();
    return engine->undefinedValue();
}

static QScriptValue js_consoleLog(QScriptContext *context, QScriptEngine *engine, Logger *logger)
{
    return js_consoleDebug(context, engine, logger);
}

void ScriptEngine::installQbsBuiltins()
{
    globalObject().setProperty(QLatin1String("qbs"), m_qbsObject = newObject());

    globalObject().setProperty(QLatin1String("console"), m_consoleObject = newObject());
    installConsoleFunction(QLatin1String("debug"), &js_consoleDebug);
    installConsoleFunction(QLatin1String("error"), &js_consoleError);
    installConsoleFunction(QLatin1String("info"), &js_consoleInfo);
    installConsoleFunction(QLatin1String("log"), &js_consoleLog);
    installConsoleFunction(QLatin1String("warn"), &js_consoleWarn);
}

void ScriptEngine::extendJavaScriptBuiltins()
{
    JSTypeExtender arrayExtender(this, QLatin1String("Array"));
    arrayExtender.addFunction(QLatin1String("contains"),
        QLatin1String("(function(e){return this.indexOf(e) !== -1;})"));
    arrayExtender.addFunction(QLatin1String("containsAll"),
        QLatin1String("(function(e){var $this = this;"
                        "return e.every(function (v) { return $this.contains(v) });})"));
    arrayExtender.addFunction(QLatin1String("containsAny"),
        QLatin1String("(function(e){var $this = this;"
                        "return e.some(function (v) { return $this.contains(v) });})"));
    arrayExtender.addFunction(QLatin1String("uniqueConcat"),
        QLatin1String("(function(other){"
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

    JSTypeExtender stringExtender(this, QLatin1String("String"));
    stringExtender.addFunction(QLatin1String("contains"),
        QLatin1String("(function(e){return this.indexOf(e) !== -1;})"));
    stringExtender.addFunction(QLatin1String("startsWith"),
        QLatin1String("(function(e){return this.slice(0, e.length) === e;})"));
    stringExtender.addFunction(QLatin1String("endsWith"),
                               QLatin1String("(function(e){return this.slice(-e.length) === e;})"));
}

void ScriptEngine::installFunction(const QString &name, int length, QScriptValue *functionValue,
        FunctionSignature f, QScriptValue *targetObject = 0)
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

void ScriptEngine::installImportFunctions()
{
    installFunction(QLatin1String("loadFile"), 1, &m_loadFileFunction, js_loadFile);
    installFunction(QLatin1String("loadExtension"), 1, &m_loadExtensionFunction, js_loadExtension);
    installFunction(QLatin1String("require"), 1, &m_requireFunction, js_require);
}

void ScriptEngine::uninstallImportFunctions()
{
    globalObject().setProperty(QLatin1String("loadFile"), QScriptValue());
    globalObject().setProperty(QLatin1String("loadExtension"), QScriptValue());
    globalObject().setProperty(QLatin1String("require"), QScriptValue());
}

ScriptEngine::PropertyCacheKey::PropertyCacheKey(const QString &moduleName,
        const QString &propertyName, const PropertyMapConstPtr &propertyMap)
    : m_moduleName(moduleName), m_propertyName(propertyName), m_propertyMap(propertyMap)
{
}

} // namespace Internal
} // namespace qbs
