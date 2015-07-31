/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "scriptengine.h"

#include "evaluatorscriptclass.h"
#include "filecontextbase.h"
#include "jsimports.h"
#include "propertymapinternal.h"
#include "scriptpropertyobserver.h"

#include <buildgraph/artifact.h>
#include <jsextensions/jsextensions.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/qbsassert.h>

#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QScriptProgram>
#include <QScriptValueIterator>
#include <QSet>
#include <QTextStream>

namespace qbs {
namespace Internal {

const bool debugJSImports = false;

bool operator==(const ScriptEngine::PropertyCacheKey &lhs,
        const ScriptEngine::PropertyCacheKey &rhs)
{
    return lhs.m_oneValue == rhs.m_oneValue
            && lhs.m_propertyMap == rhs.m_propertyMap
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
                       combineHash(qHash(k.m_propertyName),
                                   combineHash(qHash(k.m_oneValue), qHash(k.m_propertyMap),
                                               seed), seed), seed);
}

ScriptEngine::ScriptEngine(const Logger &logger, QObject *parent)
    : QScriptEngine(parent), m_propertyCacheEnabled(true), m_logger(logger)
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
}

void ScriptEngine::import(const FileContextBaseConstPtr &fileCtx, QScriptValue scope,
        QScriptValue targetObject)
{
    installImportFunctions();
    m_currentDirPathStack.push(FileInfo::path(fileCtx->filePath()));
    m_extensionSearchPathsStack.push(fileCtx->searchPaths());

    const JsImports jsImports = fileCtx->jsImports();
    for (JsImports::const_iterator it = jsImports.begin(); it != jsImports.end(); ++it) {
        import(*it, scope, targetObject);
    }

    m_currentDirPathStack.pop();
    m_extensionSearchPathsStack.pop();
    uninstallImportFunctions();
}

void ScriptEngine::import(const JsImport &jsImport, QScriptValue scope, QScriptValue targetObject)
{
    QBS_ASSERT(!scope.isValid() || scope.isObject(), return);
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
        foreach (const QString &filePath, jsImport.filePaths)
            importFile(filePath, scope, &jsImportValue);
        m_jsImportCache.insert(jsImport, jsImportValue);
    }
    targetObject.setProperty(jsImport.scopeName, jsImportValue);
}

void ScriptEngine::clearImportsCache()
{
    m_jsImportCache.clear();
}

void ScriptEngine::addPropertyRequestedFromArtifact(const Artifact *artifact,
                                                    const Property &property)
{
    m_propertiesRequestedFromArtifact[artifact->filePath()] << property;
}

void ScriptEngine::addToPropertyCache(const QString &moduleName, const QString &propertyName,
        bool oneValue, const PropertyMapConstPtr &propertyMap, const QVariant &value)
{
    m_propertyCache.insert(PropertyCacheKey(moduleName, propertyName, oneValue, propertyMap),
                           value);
}

QVariant ScriptEngine::retrieveFromPropertyCache(const QString &moduleName,
        const QString &propertyName, bool oneValue, const PropertyMapConstPtr &propertyMap)
{
    return m_propertyCache.value(PropertyCacheKey(moduleName, propertyName, oneValue, propertyMap));
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

static QScriptValue js_observedGet(QScriptContext *context, QScriptEngine *, void *arg)
{
    ScriptPropertyObserver * const observer = static_cast<ScriptPropertyObserver *>(arg);
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

QScriptValue ScriptEngine::importFile(const QString &filePath, const QScriptValue &scope,
                                      QScriptValue *targetObject)
{
    QFile file(filePath);
    if (Q_UNLIKELY(!file.open(QFile::ReadOnly)))
        throw ErrorInfo(tr("Cannot open '%1'.").arg(filePath));
    const QString sourceCode = QTextStream(&file).readAll();
    file.close();
    QScriptProgram program(sourceCode, filePath);
    QScriptValue obj;
    if (!targetObject)
        obj = newObject();
    m_currentDirPathStack.push(FileInfo::path(filePath));
    importProgram(program, scope, targetObject ? *targetObject : obj);
    m_currentDirPathStack.pop();
    return targetObject ? *targetObject : obj;
}

void ScriptEngine::importProgram(const QScriptProgram &program, const QScriptValue &scope,
                               QScriptValue &targetObject)
{
    QSet<QString> globalPropertyNames;
    {
        QScriptValueIterator it(globalObject());
        while (it.hasNext()) {
            it.next();
            globalPropertyNames += it.name();
        }
    }

    pushContext();
    if (scope.isObject())
        currentContext()->pushScope(scope);
    QScriptValue result = evaluate(program);
    QScriptValue activationObject = currentContext()->activationObject();
    if (scope.isObject())
        currentContext()->popScope();
    popContext();
    if (Q_UNLIKELY(hasErrorOrException(result)))
        throw ErrorInfo(tr("Error when importing '%1': %2").arg(program.fileName(), result.toString()));

    // If targetObject is already an object, it doesn't get overwritten but enhanced by the
    // contents of the .js file.
    // This is necessary for library imports that consist of multiple js files.
    if (!targetObject.isObject())
        targetObject = newObject();

    // Copy every property of the activation object to the target object.
    // We do not just save a reference to the activation object, because QScriptEngine contains
    // special magic for activation objects that leads to unanticipated results.
    {
        QScriptValueIterator it(activationObject);
        while (it.hasNext()) {
            it.next();
            if (debugJSImports)
                qDebug() << "[ENGINE] Copying property " << it.name();
            targetObject.setProperty(it.name(), it.value());
        }
    }

    // Copy new global properties to the target object and remove them from
    // the global object. This is to support direct variable assignments
    // without the 'var' keyword in JavaScript files.
    QScriptValueIterator it(globalObject());
    while (it.hasNext()) {
        it.next();
        if (globalPropertyNames.contains(it.name()))
            continue;

        if (debugJSImports) {
            qDebug() << "[ENGINE] inserting global property "
                     << it.name() << " " << it.value().toString();
        }

        targetObject.setProperty(it.name(), it.value());
        it.remove();
    }
}

static QString findExtensionDir(const QStringList &searchPaths, const QString &extensionPath)
{
    foreach (const QString &searchPath, searchPaths) {
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
    foreach (const QScriptValue &v, lst) {
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
    ScriptEngine *engine = static_cast<ScriptEngine *>(qtengine);
    if (context->argumentCount() < 1) {
        return context->throwError(
                    ScriptEngine::tr("The loadExtension function requires "
                                     "an extension name."));
    }

    if (engine->m_extensionSearchPathsStack.isEmpty())
        return context->throwError(
                    ScriptEngine::tr("loadExtension: internal error. No search paths."));

    const QString uri = context->argument(0).toVariant().toString();
    if (engine->m_logger.debugEnabled()) {
        engine->m_logger.qbsDebug()
                << "[loadExtension] loading extension " << uri;
    }

    QString uriAsPath = uri;
    uriAsPath.replace(QLatin1Char('.'), QLatin1Char('/'));
    const QStringList searchPaths = engine->m_extensionSearchPathsStack.top();
    const QString dirPath = findExtensionDir(searchPaths, uriAsPath);
    if (dirPath.isEmpty()) {
        if (uri.startsWith(QLatin1String("qbs.")))
            return loadInternalExtension(context, engine, uri);

        return context->throwError(
                    ScriptEngine::tr("loadExtension: Cannot find extension '%1'. "
                                     "Search paths: %2.").arg(uri, searchPaths.join(
                                                                  QLatin1String(", "))));
    }

    QDirIterator dit(dirPath, QDir::Files | QDir::Readable);
    QScriptValueList values;
    try {
        while (dit.hasNext()) {
            const QString filePath = dit.next();
            if (engine->m_logger.debugEnabled()) {
                engine->m_logger.qbsDebug()
                        << "[loadExtension] importing file " << filePath;
            }
            values << engine->importFile(filePath, QScriptValue());
        }
    } catch (const ErrorInfo &e) {
        return context->throwError(e.toString());
    }

    return mergeExtensionObjects(values);
}

QScriptValue ScriptEngine::js_loadFile(QScriptContext *context, QScriptEngine *qtengine)
{
    ScriptEngine *engine = static_cast<ScriptEngine *>(qtengine);
    if (context->argumentCount() < 1) {
        return context->throwError(
                    ScriptEngine::tr("The loadFile function requires a file path."));
    }

    if (engine->m_currentDirPathStack.isEmpty()) {
        return context->throwError(
            ScriptEngine::tr("loadFile: internal error. No current directory."));
    }

    QScriptValue result;
    const QString relativeFilePath = context->argument(0).toVariant().toString();
    try {
        const QString filePath = FileInfo::resolvePath(engine->m_currentDirPathStack.top(),
                                                       relativeFilePath);
        result = engine->importFile(filePath, QScriptValue());
    } catch (const ErrorInfo &e) {
        result = context->throwError(e.toString());
    }

    return result;
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

void ScriptEngine::addFileLastModifiedResult(const QString &filePath, const FileTime &fileTime)
{
    m_fileLastModifiedResult.insert(filePath, fileTime);
}

QSet<QString> ScriptEngine::imports() const
{
    QSet<QString> filePaths;
    foreach (const JsImport &jsImport, m_jsImportCache.keys()) {
        foreach (const QString &filePath, jsImport.filePaths)
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

void ScriptEngine::cancel()
{
    QMetaObject::invokeMethod(this, "abort", Qt::QueuedConnection);
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

void ScriptEngine::installQbsBuiltins()
{
    globalObject().setProperty(QLatin1String("qbs"), m_qbsObject = newObject());
    installQbsFunction(QLatin1String("getNativeSetting"),
                       EvaluatorScriptClass::js_getNativeSetting);
    installQbsFunction(QLatin1String("getEnv"),
                       EvaluatorScriptClass::js_getEnv);
    installQbsFunction(QLatin1String("currentEnv"),
                       EvaluatorScriptClass::js_currentEnv);
    installQbsFunction(QLatin1String("canonicalArchitecture"),
                       EvaluatorScriptClass::js_canonicalArchitecture);
    installQbsFunction(QLatin1String("rfc1034Identifier"),
                       EvaluatorScriptClass::js_rfc1034identifier);
    installQbsFunction(QLatin1String("getHash"),
                       EvaluatorScriptClass::js_getHash);
}

void ScriptEngine::extendJavaScriptBuiltins()
{
    JSTypeExtender arrayExtender(this, QLatin1String("Array"));
    arrayExtender.addFunction(QLatin1String("contains"),
        QLatin1String("(function(e){return this.indexOf(e) !== -1;})"));
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

void ScriptEngine::installFunction(const QString &name, QScriptValue *functionValue,
        FunctionSignature f, QScriptValue *targetObject = 0)
{
    if (!functionValue->isValid())
        *functionValue = newFunction(f);
    (targetObject ? *targetObject : globalObject()).setProperty(name, *functionValue);
}

void ScriptEngine::installQbsFunction(const QString &name, FunctionSignature f)
{
    QScriptValue functionValue;
    installFunction(name, &functionValue, f, &m_qbsObject);
}

void ScriptEngine::installImportFunctions()
{
    installFunction(QLatin1String("loadFile"), &m_loadFileFunction, js_loadFile);
    installFunction(QLatin1String("loadExtension"), &m_loadExtensionFunction, js_loadExtension);
}

void ScriptEngine::uninstallImportFunctions()
{
    globalObject().setProperty(QLatin1String("loadFile"), QScriptValue());
    globalObject().setProperty(QLatin1String("loadExtension"), QScriptValue());
}

ScriptEngine::PropertyCacheKey::PropertyCacheKey(const QString &moduleName,
        const QString &propertyName, bool oneValue, const PropertyMapConstPtr &propertyMap)
    : m_moduleName(moduleName), m_propertyName(propertyName), m_oneValue(oneValue),
      m_propertyMap(propertyMap)
{
}

} // namespace Internal
} // namespace qbs
