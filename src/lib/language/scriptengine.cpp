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

#include "scriptengine.h"

#include "item.h"
#include "filecontext.h"
#include "propertymapinternal.h"
#include "scriptpropertyobserver.h"
#include <tools/error.h>
#include <tools/qbsassert.h>

#include <QDebug>
#include <QFile>
#include <QScriptProgram>
#include <QScriptValueIterator>
#include <QSet>
#include <QTextStream>

namespace qbs {
namespace Internal {

const bool debugJSImports = false;

ScriptEngine::ScriptEngine(const Logger &logger, QObject *parent)
    : QScriptEngine(parent), m_logger(logger)
{
    QScriptValue objectProto = globalObject().property(QLatin1String("Object"));
    m_definePropertyFunction = objectProto.property(QLatin1String("defineProperty"));
    QBS_ASSERT(m_definePropertyFunction.isFunction(), /* ignore */);
    m_emptyFunction = evaluate("(function(){})");
    QBS_ASSERT(m_emptyFunction.isFunction(), /* ignore */);
    // Initially push a new context to turn off scope chain insanity mode.
    QScriptEngine::pushContext();
    extendJavaScriptBuiltins();
}

ScriptEngine::~ScriptEngine()
{
}

void ScriptEngine::import(const JsImports &jsImports, QScriptValue scope, QScriptValue targetObject)
{
    for (JsImports::const_iterator it = jsImports.begin(); it != jsImports.end(); ++it)
        import(*it, scope, targetObject);
}

void ScriptEngine::import(const JsImport &jsImport, QScriptValue scope, QScriptValue targetObject)
{
    QBS_ASSERT(!scope.isValid() || scope.isObject(), return);
    QBS_ASSERT(targetObject.isObject(), return);
    QBS_ASSERT(targetObject.engine() == this, return);

    if (debugJSImports)
        m_logger.qbsDebug() << "[ENGINE] import into " << jsImport.scopeName;

    foreach (const QString &fileName, jsImport.fileNames) {
        QScriptValue jsImportValue;
        jsImportValue = m_jsImportCache.value(fileName);
        if (jsImportValue.isValid()) {
            if (debugJSImports)
                m_logger.qbsDebug() << "[ENGINE] " << fileName << " (cache hit)";
            targetObject.setProperty(jsImport.scopeName, jsImportValue);
        } else {
            if (debugJSImports)
                m_logger.qbsDebug() << "[ENGINE] " << fileName << " (cache miss)";
            QFile file(fileName);
            if (Q_UNLIKELY(!file.open(QFile::ReadOnly)))
                throw ErrorInfo(tr("Cannot open '%1'.").arg(fileName));
            const QString sourceCode = QTextStream(&file).readAll();
            file.close();
            QScriptProgram program(sourceCode, fileName);
            importProgram(program, scope, jsImportValue);
            targetObject.setProperty(jsImport.scopeName, jsImportValue);
            m_jsImportCache.insert(fileName, jsImportValue);
        }
    }
}

void ScriptEngine::clearImportsCache()
{
    m_jsImportCache.clear();
}

void ScriptEngine::addToPropertyCache(const QString &moduleName, const QString &propertyName,
        const PropertyMapConstPtr &propertyMap, const QVariant &value)
{
    m_propertyCache.insert(qMakePair(moduleName + QLatin1Char('.') + propertyName, propertyMap),
                           value);
}

QVariant ScriptEngine::retrieveFromPropertyCache(const QString &moduleName,
        const QString &propertyName, const PropertyMapConstPtr &propertyMap)
{
    return m_propertyCache.value(qMakePair(moduleName + QLatin1Char('.') + propertyName,
                                           propertyMap));
}

void ScriptEngine::defineProperty(QScriptValue &object, const QString &name,
                                  const QScriptValue &descriptor)
{
    QScriptValue arguments = newArray();
    arguments.setProperty(0, object);
    arguments.setProperty(1, name);
    arguments.setProperty(2, descriptor);
    QScriptValue result = m_definePropertyFunction.call(QScriptValue(), arguments);
    QBS_ASSERT(!result.isError(), qDebug() << name << result.toString());
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
    QScriptValue descriptor = newObject();
    descriptor.setProperty(QLatin1String("get"), getterFunc);
    descriptor.setProperty(QLatin1String("set"), m_emptyFunction);
    defineProperty(object, name, descriptor);
}

QProcessEnvironment ScriptEngine::environment() const
{
    return m_environment;
}

void ScriptEngine::setEnvironment(const QProcessEnvironment &env)
{
    m_environment = env;
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
    if (Q_UNLIKELY(result.isError()))
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
                m_logger.qbsDebug() << "[ENGINE] Copying property " << it.name();
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
            m_logger.qbsDebug() << "[ENGINE] inserting global property "
                                << it.name() << " " << it.value().toString();
        }

        targetObject.setProperty(it.name(), it.value());
        it.remove();
    }
}

void ScriptEngine::addEnvironmentVariable(const QString &name, const QString &value)
{
    m_usedEnvironment.insert(name, value);
}

void ScriptEngine::addFileExistsResult(const QString &filePath, bool exists)
{
    m_fileExistsResult.insert(filePath, exists);
}

QSet<QString> ScriptEngine::imports() const
{
    return QSet<QString>::fromList(m_jsImportCache.keys());
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

void ScriptEngine::extendJavaScriptBuiltins()
{
    JSTypeExtender arrayExtender(this, QLatin1String("Array"));
    arrayExtender.addFunction(QLatin1String("contains"),
        QLatin1String("(function(e){return this.indexOf(e) !== -1;})"));

    JSTypeExtender stringExtender(this, QLatin1String("String"));
    stringExtender.addFunction(QLatin1String("contains"),
        QLatin1String("(function(e){return this.indexOf(e) !== -1;})"));
    stringExtender.addFunction(QLatin1String("startsWith"),
        QLatin1String("(function(e){return this.slice(0, e.length) === e;})"));
    stringExtender.addFunction(QLatin1String("endsWith"),
        QLatin1String("(function(e){return this.slice(-e.length) === e;})"));
}

} // namespace Internal
} // namespace qbs
