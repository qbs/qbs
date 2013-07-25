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

#include "evaluatorscriptclass.h"

#include "builtinvalue.h"
#include "evaluationdata.h"
#include "evaluator.h"
#include "filecontext.h"
#include "item.h"
#include "propertydeclaration.h"
#include <tools/qbsassert.h>

#include <QScriptEngine>
#include <QScriptString>
#include <QScriptValue>
#include <QDebug>

namespace qbs {
namespace Internal {

class SVConverter : ValueHandler
{
    EvaluatorScriptClass *const scriptClass;
    QScriptEngine *const engine;
    QScriptContext *const scriptContext;
    const QScriptValue *object;
    const ValuePtr &valuePtr;
    const bool inPrototype;
    char pushedScopesCount;

public:
    const QScriptString *propertyName;
    const EvaluationData *data;
    QScriptValue *result;

    SVConverter(EvaluatorScriptClass *esc, const QScriptValue *obj, const ValuePtr &v,
                bool _inPrototype)
        : scriptClass(esc)
        , engine(esc->engine())
        , scriptContext(esc->engine()->currentContext())
        , object(obj)
        , valuePtr(v)
        , inPrototype(_inPrototype)
        , pushedScopesCount(0)
    {
    }

    void start()
    {
        valuePtr->apply(this);
    }

private:
    void setupConvenienceProperty(const QString &conveniencePropertyName, QScriptValue *extraScope,
                                  const QScriptValue &scriptValue)
    {
        if (!extraScope->isObject())
            *extraScope = scriptClass->engine()->newObject();
        if (!scriptValue.isValid() || scriptValue.isUndefined()) {
            // If there's no such value, use an empty array to have the convenience property
            // still available.
            extraScope->setProperty(conveniencePropertyName, engine->newArray());
        } else {
            extraScope->setProperty(conveniencePropertyName, scriptValue);
        }
    }

    void pushScope(const QScriptValue &scope)
    {
        if (scope.isObject()) {
            scriptContext->pushScope(scope);
            ++pushedScopesCount;
        }
    }

    void pushItemScopes(const Item *item)
    {
        const Item *scope = item->scope();
        if (scope) {
            pushItemScopes(scope);
            pushScope(data->evaluator->scriptValue(scope));
        }
    }

    void popScopes()
    {
        for (; pushedScopesCount; --pushedScopesCount)
            scriptContext->popScope();
    }

    void handle(JSSourceValue *value)
    {
        const Item *conditionScopeItem = 0;
        QScriptValue conditionScope;
        QScriptValue conditionFileScope;
        Item *outerItem = data->item->outerItem();
        for (int i = 0; i < value->alternatives().count(); ++i) {
            const JSSourceValue::Alternative *alternative = 0;
            alternative = &value->alternatives().at(i);
            if (conditionScopeItem != alternative->conditionScopeItem) {
                conditionScopeItem = alternative->conditionScopeItem;
                conditionScope = data->evaluator->scriptValue(alternative->conditionScopeItem);
                QBS_ASSERT(conditionScope.isObject(), return);
                conditionFileScope = data->evaluator->fileScope(conditionScopeItem->file());
            }
            engine->currentContext()->pushScope(conditionFileScope);
            pushItemScopes(conditionScopeItem);
            engine->currentContext()->pushScope(conditionScope);
            const QScriptValue cr = engine->evaluate(alternative->condition);
            engine->currentContext()->popScope();
            engine->currentContext()->popScope();
            popScopes();
            if (cr.isError()) {
                *result = cr;
                return;
            }
            if (cr.toBool()) {
                // condition is true, let's use the value of this alternative
                if (alternative->value->sourceUsesOuter()) {
                    // Clone value but without alternatives.
                    JSSourceValuePtr outerValue = JSSourceValue::create();
                    outerValue->setFile(value->file());
                    outerValue->setSourceCode(value->sourceCode());
                    outerValue->setBaseValue(value->baseValue());
                    outerValue->setLocation(value->location());
                    outerItem = Item::create(data->item->pool());
                    outerItem->setProperty(propertyName->toString(), outerValue);
                }
                value = alternative->value.data();
                break;
            }
        }

        QScriptValue extraScope;
        if (value->sourceUsesBase()) {
            QScriptValue baseValue;
            if (value->baseValue()) {
                SVConverter converter(scriptClass, object, value->baseValue(), inPrototype);
                converter.propertyName = propertyName;
                converter.data = data;
                converter.result = &baseValue;
                converter.start();
            }
            setupConvenienceProperty(QLatin1String("base"), &extraScope, baseValue);
        }
        if (value->sourceUsesOuter() && outerItem)
            setupConvenienceProperty(QLatin1String("outer"), &extraScope,
                                     data->evaluator->property(outerItem, *propertyName));

        pushScope(data->evaluator->fileScope(value->file()));
        pushItemScopes(data->item);
        if (inPrototype || !data->item->isModuleInstance()) {
            // Own properties of module instances must not have the instance itself in the scope.
            pushScope(*object);
        }
        pushScope(extraScope);
        const CodeLocation valueLocation = value->location();
        *result = engine->evaluate(value->sourceCode(), valueLocation.fileName(),
                                   valueLocation.line());
        popScopes();
    }

    void handle(ItemValue *value)
    {
        Item *item = value->item();
        if (!item)
            qDebug() << "SVConverter got null item" << propertyName->toString();
        *result = data->evaluator->scriptValue(item);
        if (!result->isValid())
            qDebug() << "SVConverter returned invalid script value.";
    }

    void handle(VariantValue *variantValue)
    {
        *result = scriptClass->engine()->toScriptValue(variantValue->value());
    }

    void handle(BuiltinValue *builtinValue)
    {
        *result = scriptClass->scriptValueForBuiltin(builtinValue->builtin());
    }
};

bool debugProperties = false;

enum QueryPropertyType
{
    QPTDefault,
    QPTParentProperty
};

EvaluatorScriptClass::EvaluatorScriptClass(QScriptEngine *scriptEngine, const Logger &logger)
    : QScriptClass(scriptEngine)
    , m_logger(logger)
{
    m_getenvBuiltin = scriptEngine->newFunction(js_getenv, 1);
    m_getHostOSBuiltin = scriptEngine->newFunction(js_getHostOS, 1);
}

QScriptClass::QueryFlags EvaluatorScriptClass::queryProperty(const QScriptValue &object,
                                                             const QScriptString &name,
                                                             QScriptClass::QueryFlags flags,
                                                             uint *id)
{
    Q_UNUSED(flags);

    // We assume that it's safe to save the result of the query in a member of the scriptclass.
    // It must be cleared in the property method before doing any further lookup.
    QBS_ASSERT(m_queryResult.isNull(), return QueryFlags());

    if (debugProperties)
        m_logger.qbsTrace() << "[SC] queryProperty " << object.objectId() << " " << name;

    EvaluationData *const data = EvaluationData::get(object);
    const QString nameString = name.toString();
    if (nameString == QLatin1String("parent")) {
        *id = QPTParentProperty;
        m_queryResult.data = data;
        return QScriptClass::HandlesReadAccess;
    }

    *id = QPTDefault;
    if (!data) {
        if (debugProperties)
            m_logger.qbsTrace() << "[SC] queryProperty: no data attached";
        return QScriptClass::QueryFlags();
    }

    return queryItemProperty(data, nameString);
}

QScriptClass::QueryFlags EvaluatorScriptClass::queryItemProperty(const EvaluationData *data,
                                                                 const QString &name,
                                                                 bool ignoreParent)
{
    for (const Item *item = data->item; item; item = item->prototype()) {
        m_queryResult.value = item->properties().value(name);
        if (!m_queryResult.value.isNull()) {
            m_queryResult.data = data;
            if (data->item != item)
                m_queryResult.inPrototype = true;
            return HandlesReadAccess;
        }
    }

    if (!ignoreParent && data->item->parent()) {
        if (debugProperties)
            m_logger.qbsTrace() << "[SC] queryProperty: query parent";
        EvaluationData parentdata = *data;
        parentdata.item = data->item->parent();
        const QueryFlags qf = queryItemProperty(&parentdata, name, true);
        if (qf.testFlag(HandlesReadAccess)) {
            m_queryResult.data = data;
            return qf;
        }
    }

    if (debugProperties)
        m_logger.qbsTrace() << "[SC] queryProperty: no such property";
    return QScriptClass::QueryFlags();
}

QString EvaluatorScriptClass::resultToString(const QScriptValue &scriptValue)
{
    return (scriptValue.isObject()
        ? QLatin1String("[Object: ")
            + QString::number(scriptValue.objectId(), 16) + QLatin1Char(']')
        : scriptValue.toVariant().toString());
}

Item *EvaluatorScriptClass::findParentOfType(const Item *item, const QString &typeName)
{
    for (Item *it = item->parent(); it; it = it->parent()) {
        if (it->typeName() == typeName)
            return it;
    }
    return 0;
}

inline void convertToPropertyType(const PropertyDeclaration::Type t, QScriptValue &v)
{
    if (v.isUndefined())
        return;
    switch (t) {
    case PropertyDeclaration::UnknownType:
    case PropertyDeclaration::Variant:
    case PropertyDeclaration::Verbatim:
        break;
    case PropertyDeclaration::Boolean:
        if (!v.isBool())
            v = v.toBool();
        break;
    case PropertyDeclaration::Integer:
        if (!v.isNumber())
            v = v.toNumber();
        break;
    case PropertyDeclaration::Path:
    case PropertyDeclaration::String:
        if (!v.isString())
            v = v.toString();
        break;
    case PropertyDeclaration::PathList:
    case PropertyDeclaration::StringList:
        if (!v.isArray()) {
            QScriptValue x = v.engine()->newArray(1);
            x.setProperty(0, v.isString() ? v : v.toString());
            v = x;
        }
        break;
    }
}

QScriptValue EvaluatorScriptClass::property(const QScriptValue &object, const QScriptString &name,
                                            uint id)
{
    const EvaluationData *data = m_queryResult.data;
    const bool inPrototype = m_queryResult.inPrototype;
    m_queryResult.data = 0;
    m_queryResult.inPrototype = false;
    QBS_ASSERT(data, return QScriptValue());

    const QueryPropertyType qpt = static_cast<QueryPropertyType>(id);
    if (qpt == QPTParentProperty) {
        return data->item->parent()
                ? data->evaluator->scriptValue(data->item->parent())
                : engine()->undefinedValue();
    }

    ValuePtr value;
    m_queryResult.value.swap(value);
    QBS_ASSERT(value, return QScriptValue());
    QBS_ASSERT(m_queryResult.isNull(), return QScriptValue());

    if (debugProperties)
        m_logger.qbsTrace() << "[SC] property " << name;

    QScriptValue result = data->valueCache.value(name);
    if (result.isValid()) {
        if (debugProperties)
            m_logger.qbsTrace() << "[SC] cache hit " << name << ": " << resultToString(result);
        return result;
    }

    SVConverter converter(this, &object, value, inPrototype);
    converter.propertyName = &name;
    converter.data = data;
    converter.result = &result;
    converter.start();

    const PropertyDeclaration decl = data->item->propertyDeclarations().value(name.toString());
    convertToPropertyType(decl.type, result);

    if (debugProperties)
        m_logger.qbsTrace() << "[SC] cache miss " << name << ": " << resultToString(result);
    data->valueCache.insert(name, result);
    return result;
}

QScriptValue EvaluatorScriptClass::scriptValueForBuiltin(BuiltinValue::Builtin builtin) const
{
    switch (builtin) {
    case BuiltinValue::GetEnvFunction:
        return m_getenvBuiltin;
    case BuiltinValue::GetHostOSFunction:
        return m_getHostOSBuiltin;
    }
    QBS_ASSERT(!"unhandled builtin", ;);
    return QScriptValue();
}

QScriptValue EvaluatorScriptClass::js_getenv(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("getenv expects 1 argument"));
    }
    const QString name = context->argument(0).toString();
    ScriptEngine * const e = static_cast<ScriptEngine *>(engine);
    const QString value = e->environment().value(name);
    e->addEnvironmentVariable(name, value);
    return value.isNull() ? engine->undefinedValue() : value;
}

QScriptValue EvaluatorScriptClass::js_getHostOS(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context);
    QStringList hostSystem;

#if defined(Q_OS_AIX)
    hostSystem << "aix";
#endif
#if defined(Q_OS_ANDROID)
    hostSystem << "android";
#endif
#if defined(Q_OS_BLACKBERRY)
    hostSystem << "blackberry";
#endif
#if defined(Q_OS_BSD4)
    hostSystem << "bsd" << "bsd4";
#endif
#if defined(Q_OS_BSDI)
    hostSystem << "bsdi";
#endif
#if defined(Q_OS_CYGWIN)
    hostSystem << "cygwin";
#endif
#if defined(Q_OS_DARWIN)
    hostSystem << "darwin";
#endif
#if defined(Q_OS_DGUX)
    hostSystem << "dgux";
#endif
#if defined(Q_OS_DYNIX)
    hostSystem << "dynix";
#endif
#if defined(Q_OS_FREEBSD)
    hostSystem << "freebsd";
#endif
#if defined(Q_OS_HPUX)
    hostSystem << "hpux";
#endif
#if defined(Q_OS_HURD)
    hostSystem << "hurd";
#endif
#if defined(Q_OS_INTEGRITY)
    hostSystem << "integrity";
#endif
#if defined(Q_OS_IOS)
    hostSystem << "ios";
#endif
#if defined(Q_OS_IRIX)
    hostSystem << "irix";
#endif
#if defined(Q_OS_LINUX)
    hostSystem << "linux";
#endif
#if defined(Q_OS_LYNX)
    hostSystem << "lynx";
#endif
#if defined(Q_OS_MACX)
    hostSystem << "osx";
#endif
#if defined(Q_OS_MSDOS)
    hostSystem << "msdos";
#endif
#if defined(Q_OS_NACL)
    hostSystem << "nacl";
#endif
#if defined(Q_OS_NETBSD)
    hostSystem << "netbsd";
#endif
#if defined(Q_OS_OPENBSD)
    hostSystem << "openbsd";
#endif
#if defined(Q_OS_OS2)
    hostSystem << "os2";
#endif
#if defined(Q_OS_OS2EMX)
    hostSystem << "os2emx";
#endif
#if defined(Q_OS_OSF)
    hostSystem << "osf";
#endif
#if defined(Q_OS_QNX)
    hostSystem << "qnx";
#endif
#if defined(Q_OS_ONX6)
    hostSystem << "qnx6";
#endif
#if defined(Q_OS_RELIANT)
    hostSystem << "reliant";
#endif
#if defined(Q_OS_SCO)
    hostSystem << "sco";
#endif
#if defined(Q_OS_SOLARIS)
    hostSystem << "solaris";
#endif
#if defined(Q_OS_SYMBIAN)
    hostSystem << "symbian";
#endif
#if defined(Q_OS_ULTRIX)
    hostSystem << "ultrix";
#endif
#if defined(Q_OS_UNIX)
    hostSystem << "unix";
#endif
#if defined(Q_OS_UNIXWARE)
    hostSystem << "unixware";
#endif
#if defined(Q_OS_VXWORKS)
    hostSystem << "vxworks";
#endif
#if defined(Q_OS_WIN32)
    hostSystem << "windows";
#endif
#if defined(Q_OS_WINCE)
    hostSystem << "windowsce";
#endif
#if defined(Q_OS_WINPHONE)
    hostSystem << "windowsphone";
#endif
#if defined(Q_OS_WINRT)
    hostSystem << "winrt";
#endif

    return engine->toScriptValue(hostSystem);
}

} // namespace Internal
} // namespace qbs
