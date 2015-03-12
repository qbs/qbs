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

#include "evaluatorscriptclass.h"

#include "builtinvalue.h"
#include "evaluationdata.h"
#include "evaluator.h"
#include "filecontext.h"
#include "item.h"
#include "scriptengine.h"
#include "propertydeclaration.h"
#include <tools/architectures.h>
#include <tools/hostosinfo.h>
#include <tools/qbsassert.h>
#include <tools/scripttools.h>

#include <QByteArray>
#include <QCryptographicHash>
#include <QScriptString>
#include <QScriptValue>
#include <QDebug>
#include <QSettings>

namespace qbs {
namespace Internal {

template <class T>
class ScopedPopper
{
    QStack<T> *m_stack;
    bool m_mustPop;
public:
    ScopedPopper(QStack<T> *s)
        : m_stack(s), m_mustPop(false)
    {
    }

    ~ScopedPopper()
    {
        if (m_mustPop)
            m_stack->pop();
    }

    void mustPop()
    {
        m_mustPop = true;
    }
};

class SVConverter : ValueHandler
{
    EvaluatorScriptClass * const scriptClass;
    ScriptEngine * const engine;
    QScriptContext * const scriptContext;
    const QScriptValue * const object;
    const ValuePtr &valuePtr;
    const Item * const itemOfProperty;
    const QScriptString * const propertyName;
    const EvaluationData * const data;
    QScriptValue * const result;
    QStack<JSSourceValue *> * const sourceValueStack;
    char pushedScopesCount;

public:

    SVConverter(EvaluatorScriptClass *esc, const QScriptValue *obj, const ValuePtr &v,
                const Item *_itemOfProperty, const QScriptString *propertyName, const EvaluationData *data,
                QScriptValue *result, QStack<JSSourceValue *> *sourceValueStack)
        : scriptClass(esc)
        , engine(static_cast<ScriptEngine *>(esc->engine()))
        , scriptContext(esc->engine()->currentContext())
        , object(obj)
        , valuePtr(v)
        , itemOfProperty(_itemOfProperty)
        , propertyName(propertyName)
        , data(data)
        , result(result)
        , sourceValueStack(sourceValueStack)
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
        ScopedPopper<JSSourceValue *> popper(sourceValueStack);
        if (value->sourceUsesProduct()) {
            foreach (JSSourceValue *a, *sourceValueStack)
                a->setSourceUsesProductFlag();
        } else {
            sourceValueStack->push(value);
            popper.mustPop();
        }

        const Item *conditionScopeItem = 0;
        QScriptValue conditionScope;
        QScriptValue conditionFileScope;
        Item *outerItem = data->item->outerItem();
        for (int i = 0; i < value->alternatives().count(); ++i) {
            const JSSourceValue::Alternative *alternative = 0;
            alternative = &value->alternatives().at(i);
            if (!conditionScopeItem) {
                // We have to differentiate between module instances and normal items here.
                //
                // The module instance case:
                // Product {
                //     property bool something: true
                //     Properties {
                //         condition: something
                //         cpp.defines: ["ABC"]
                //     }
                // }
                //
                // data->item points to cpp and the condition's scope chain must contain cpp's
                // scope, which is the item where cpp is instantiated. The scope chain must not
                // contain data->item itself.
                //
                // The normal item case:
                // Product {
                //     property bool something: true
                //     property string value: "ABC"
                //     Properties {
                //         condition: something
                //         value: "DEF"
                //     }
                // }
                //
                // data->item points to the product and the condition's scope chain must contain
                // the product item.
                conditionScopeItem = data->item->isModuleInstance()
                        ? data->item->scope() : data->item;
                conditionScope = data->evaluator->scriptValue(conditionScopeItem);
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
            if (engine->hasErrorOrException(cr)) {
                *result = cr;
                return;
            }
            if (cr.toBool()) {
                // condition is true, let's use the value of this alternative
                if (alternative->value->sourceUsesOuter()) {
                    // Clone value but without alternatives.
                    JSSourceValuePtr outerValue = JSSourceValue::create();
                    outerValue->setFile(value->file());
                    outerValue->setHasFunctionForm(value->hasFunctionForm());
                    outerValue->setSourceCode(value->sourceCode());
                    outerValue->setBaseValue(value->baseValue());
                    outerValue->setLocation(value->line(), value->column());
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
                SVConverter converter(scriptClass, object, value->baseValue(), itemOfProperty,
                                      propertyName, data, &baseValue, sourceValueStack);
                converter.start();
            }
            setupConvenienceProperty(QLatin1String("base"), &extraScope, baseValue);
        }
        if (value->sourceUsesOuter() && outerItem)
            setupConvenienceProperty(QLatin1String("outer"), &extraScope,
                                     data->evaluator->property(outerItem, *propertyName));

        pushScope(data->evaluator->fileScope(value->file()));
        pushItemScopes(data->item);
        if (itemOfProperty && !itemOfProperty->isModuleInstance()) {
            // Own properties of module instances must not have the instance itself in the scope.
            pushScope(*object);
        }
        pushScope(extraScope);
        *result = engine->evaluate(value->sourceCodeForEvaluation(), value->file()->filePath(),
                                   value->line());
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

EvaluatorScriptClass::EvaluatorScriptClass(ScriptEngine *scriptEngine, const Logger &logger)
    : QScriptClass(scriptEngine)
    , m_logger(logger)
    , m_valueCacheEnabled(true)
{
    m_getNativeSettingBuiltin = scriptEngine->newFunction(js_getNativeSetting, 3);
    m_getEnvBuiltin = scriptEngine->newFunction(js_getEnv, 1);
    m_canonicalArchitectureBuiltin = scriptEngine->newFunction(js_canonicalArchitecture, 1);
    m_rfc1034identifierBuiltin = scriptEngine->newFunction(js_rfc1034identifier, 1);
    m_getHashBuiltin = scriptEngine->newFunction(js_getHash, 1);
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

    EvaluationData *const data = attachedPointer<EvaluationData>(object);
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
            m_queryResult.itemOfProperty = item;
            return HandlesReadAccess;
        }
    }

    if (!ignoreParent && data->item && data->item->parent()) {
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
    const Item * const itemOfProperty = m_queryResult.itemOfProperty;
    m_queryResult.data = 0;
    m_queryResult.itemOfProperty = 0;
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

    QScriptValue result;
    if (m_valueCacheEnabled) {
        result = data->valueCache.value(name);
        if (result.isValid()) {
            if (debugProperties)
                m_logger.qbsTrace() << "[SC] cache hit " << name << ": " << resultToString(result);
            return result;
        }
    }

    SVConverter converter(this, &object, value, itemOfProperty, &name, data, &result,
                          &m_sourceValueStack);
    converter.start();

    const PropertyDeclaration decl = data->item->propertyDeclarations().value(name.toString());
    convertToPropertyType(decl.type(), result);

    if (debugProperties)
        m_logger.qbsTrace() << "[SC] cache miss " << name << ": " << resultToString(result);
    if (m_valueCacheEnabled)
        data->valueCache.insert(name, result);
    return result;
}

void EvaluatorScriptClass::setValueCacheEnabled(bool enabled)
{
    m_valueCacheEnabled = enabled;
}

QScriptValue EvaluatorScriptClass::scriptValueForBuiltin(BuiltinValue::Builtin builtin) const
{
    switch (builtin) {
    case BuiltinValue::GetNativeSettingFunction:
        return m_getNativeSettingBuiltin;
    case BuiltinValue::GetEnvFunction:
        return m_getEnvBuiltin;
    case BuiltinValue::CanonicalArchitectureFunction:
        return m_canonicalArchitectureBuiltin;
    case BuiltinValue::Rfc1034IdentifierFunction:
        return m_rfc1034identifierBuiltin;
    }
    QBS_ASSERT(!"unhandled builtin", ;);
    return QScriptValue();
}

QScriptValue EvaluatorScriptClass::js_getNativeSetting(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() < 1 || context->argumentCount() > 3)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("getNativeSetting expects between 1 and 3 arguments"));
    }

    QString key = context->argumentCount() > 1 ? context->argument(1).toString() : QString();

    // We'll let empty string represent the default registry value
    if (HostOsInfo::isWindowsHost() && key.isEmpty())
        key = QLatin1String(".");

    QVariant defaultValue = context->argumentCount() > 2 ? context->argument(2).toVariant() : QVariant();

    QSettings settings(context->argument(0).toString(), QSettings::NativeFormat);
    QVariant value = settings.value(key, defaultValue);
    return value.isNull() ? engine->undefinedValue() : engine->toScriptValue(value);
}

QScriptValue EvaluatorScriptClass::js_getEnv(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("getEnv expects 1 argument"));
    }
    const QString name = context->argument(0).toString();
    ScriptEngine * const e = static_cast<ScriptEngine *>(engine);
    const QString value = e->environment().value(name);
    e->addEnvironmentVariable(name, value);
    return value.isNull() ? engine->undefinedValue() : value;
}

QScriptValue EvaluatorScriptClass::js_canonicalArchitecture(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("canonicalArchitecture expects 1 argument"));
    }
    const QString architecture = context->argument(0).toString();
    return engine->toScriptValue(canonicalArchitecture(architecture));
}

QScriptValue EvaluatorScriptClass::js_rfc1034identifier(QScriptContext *context,
                                                        QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("rfc1034Identifier expects 1 argument"));
    }
    const QString identifier = context->argument(0).toString();
    return engine->toScriptValue(HostOsInfo::rfc1034Identifier(identifier));
}

QScriptValue EvaluatorScriptClass::js_getHash(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("getHash expects 1 argument"));
    }
    const QByteArray input = context->argument(0).toString().toLatin1();
    const QByteArray hash
            = QCryptographicHash::hash(input, QCryptographicHash::Sha1).toHex().left(16);
    return engine->toScriptValue(QString::fromLatin1(hash));
}

} // namespace Internal
} // namespace qbs
