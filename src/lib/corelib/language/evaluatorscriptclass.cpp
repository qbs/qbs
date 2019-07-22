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

#include "evaluatorscriptclass.h"

#include "evaluationdata.h"
#include "evaluator.h"
#include "filecontext.h"
#include "item.h"
#include "scriptengine.h"
#include "propertydeclaration.h"
#include "value.h"
#include <logging/translator.h>
#include <tools/fileinfo.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/scripttools.h>
#include <tools/stringconstants.h>

#include <QtCore/qbytearray.h>
#include <QtCore/qdebug.h>
#include <QtCore/qsettings.h>

#include <QtScript/qscriptclasspropertyiterator.h>
#include <QtScript/qscriptstring.h>
#include <QtScript/qscriptvalue.h>

#include <utility>

namespace qbs {
namespace Internal {

class SVConverter : ValueHandler
{
    EvaluatorScriptClass * const scriptClass;
    ScriptEngine * const engine;
    QScriptContext * const scriptContext;
    const QScriptValue * const object;
    Value * const valuePtr;
    const Item * const itemOfProperty;
    const QScriptString * const propertyName;
    const EvaluationData * const data;
    QScriptValue * const result;
    char pushedScopesCount;

public:

    SVConverter(EvaluatorScriptClass *esc, const QScriptValue *obj, const ValuePtr &v,
                const Item *_itemOfProperty, const QScriptString *propertyName, const EvaluationData *data,
                QScriptValue *result)
        : scriptClass(esc)
        , engine(static_cast<ScriptEngine *>(esc->engine()))
        , scriptContext(esc->engine()->currentContext())
        , object(obj)
        , valuePtr(v.get())
        , itemOfProperty(_itemOfProperty)
        , propertyName(propertyName)
        , data(data)
        , result(result)
        , pushedScopesCount(0)
    {
    }

    void start()
    {
        valuePtr->apply(this);
    }

private:
    friend class AutoScopePopper;

    class AutoScopePopper
    {
    public:
        AutoScopePopper(SVConverter *converter)
            : m_converter(converter)
        {
        }

        ~AutoScopePopper()
        {
            m_converter->popScopes();
        }

    private:
        SVConverter *m_converter;
    };

    void setupConvenienceProperty(const QString &conveniencePropertyName, QScriptValue *extraScope,
                                  const QScriptValue &scriptValue)
    {
        if (!extraScope->isObject())
            *extraScope = engine->newObject();
        const PropertyDeclaration::Type type
                = itemOfProperty->propertyDeclaration(propertyName->toString()).type();
        const bool isArray = type == PropertyDeclaration::StringList
                || type == PropertyDeclaration::PathList
                || type == PropertyDeclaration::Variant // TODO: Why?
                || type == PropertyDeclaration::VariantList;
        QScriptValue valueToSet = scriptValue;
        if (isArray) {
            if (!valueToSet.isValid() || valueToSet.isUndefined())
                valueToSet = engine->newArray();
        } else if (!valueToSet.isValid()) {
            valueToSet = engine->undefinedValue();
        }
        extraScope->setProperty(conveniencePropertyName, valueToSet);
    }

    std::pair<QScriptValue, bool> createExtraScope(const JSSourceValue *value, Item *outerItem,
                                                   QScriptValue *outerScriptValue)
    {
        std::pair<QScriptValue, bool> result;
        auto &extraScope = result.first;
        result.second = true;
        if (value->sourceUsesBase()) {
            QScriptValue baseValue;
            if (value->baseValue()) {
                SVConverter converter(scriptClass, object, value->baseValue(), itemOfProperty,
                                      propertyName, data, &baseValue);
                converter.start();
            }
            setupConvenienceProperty(StringConstants::baseVar(), &extraScope, baseValue);
        }
        if (value->sourceUsesOuter()) {
            QScriptValue v;
            if (outerItem) {
                v = data->evaluator->property(outerItem, *propertyName);
                if (engine->hasErrorOrException(v)) {
                    extraScope = engine->lastErrorValue(v);
                    result.second = false;
                    return result;
                }
            } else if (outerScriptValue) {
                v = *outerScriptValue;
            }
            if (v.isValid())
                setupConvenienceProperty(StringConstants::outerVar(), &extraScope, v);
        }
        if (value->sourceUsesOriginal()) {
            QScriptValue originalValue;
            if (data->item->propertyDeclaration(propertyName->toString()).isScalar()) {
                const Item *item = itemOfProperty;
                if (item->type() == ItemType::Module || item->type() == ItemType::Export) {
                    const QString errorMessage = Tr::tr("The special value 'original' cannot "
                            "be used on the right-hand side of a property declaration.");
                    extraScope = engine->currentContext()->throwError(errorMessage);
                    result.second = false;
                    return result;
                }

                // TODO: Provide a dedicated item type for not-yet-instantiated things that
                //       look like module instances in the AST visitor.
                if (item->type() == ItemType::ModuleInstance
                        && !item->hasProperty(StringConstants::presentProperty())) {
                    const QString errorMessage = Tr::tr("Trying to assign property '%1' "
                            "on something that is not a module.").arg(propertyName->toString());
                    extraScope = engine->currentContext()->throwError(errorMessage);
                    result.second = false;
                    return result;
                }

                while (item->type() == ItemType::ModuleInstance)
                    item = item->prototype();
                if (item->type() != ItemType::Module && item->type() != ItemType::Export) {
                    const QString errorMessage = Tr::tr("The special value 'original' can only "
                                                        "be used with module properties.");
                    extraScope = engine->currentContext()->throwError(errorMessage);
                    result.second = false;
                    return result;
                }
                const ValuePtr v = item->property(*propertyName);

                // This can happen when resolving shadow products. The error will be ignored
                // in that case.
                if (!v) {
                    const QString errorMessage = Tr::tr("Error setting up 'original'.");
                    extraScope = engine->currentContext()->throwError(errorMessage);
                    result.second = false;
                    return result;
                }

                SVConverter converter(scriptClass, object, v, item,
                                      propertyName, data, &originalValue);
                converter.start();
            } else {
                originalValue = engine->newArray(0);
            }
            setupConvenienceProperty(StringConstants::originalVar(), &extraScope, originalValue);
        }
        return result;
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

    void handle(JSSourceValue *value) override
    {
        QScriptValue outerScriptValue;
        for (const JSSourceValue::Alternative &alternative : value->alternatives()) {
            if (alternative.value->sourceUsesOuter()
                    && !data->item->outerItem()
                    && !outerScriptValue.isValid()) {
                JSSourceValueEvaluationResult sver = evaluateJSSourceValue(value, nullptr);
                if (sver.hasError) {
                    *result = sver.scriptValue;
                    return;
                }
                outerScriptValue = sver.scriptValue;
            }
            JSSourceValueEvaluationResult sver = evaluateJSSourceValue(alternative.value.get(),
                                                                       data->item->outerItem(),
                                                                       &alternative,
                                                                       value, &outerScriptValue);
            if (!sver.tryNextAlternative || sver.hasError) {
                *result = sver.scriptValue;
                return;
            }
        }
        *result = evaluateJSSourceValue(value, data->item->outerItem()).scriptValue;
    }

    struct JSSourceValueEvaluationResult
    {
        QScriptValue scriptValue;
        bool tryNextAlternative = true;
        bool hasError = false;
    };

    void injectErrorLocation(QScriptValue &sv, const CodeLocation &loc)
    {
        if (sv.isError() && !engine->lastErrorLocation(sv).isValid())
            sv = engine->currentContext()->throwError(engine->lastError(sv, loc).toString());
    }

    JSSourceValueEvaluationResult evaluateJSSourceValue(const JSSourceValue *value, Item *outerItem,
            const JSSourceValue::Alternative *alternative = nullptr,
            JSSourceValue *elseCaseValue = nullptr, QScriptValue *outerScriptValue = nullptr)
    {
        JSSourceValueEvaluationResult result;
        QBS_ASSERT(!alternative || value == alternative->value.get(), return result);
        AutoScopePopper autoScopePopper(this);
        auto maybeExtraScope = createExtraScope(value, outerItem, outerScriptValue);
        if (!maybeExtraScope.second) {
            result.scriptValue = maybeExtraScope.first;
            result.hasError = true;
            return result;
        }
        const Evaluator::FileContextScopes fileCtxScopes
                = data->evaluator->fileContextScopes(value->file());
        if (fileCtxScopes.importScope.isError()) {
            result.scriptValue = fileCtxScopes.importScope;
            result.hasError = true;
            return result;
        }
        pushScope(fileCtxScopes.fileScope);
        pushItemScopes(data->item);
        if (itemOfProperty->type() != ItemType::ModuleInstance) {
            // Own properties of module instances must not have the instance itself in the scope.
            pushScope(*object);
        }
        if (value->definingItem())
            pushItemScopes(value->definingItem());
        pushScope(maybeExtraScope.first);
        pushScope(fileCtxScopes.importScope);
        if (alternative) {
            QScriptValue sv = engine->evaluate(alternative->condition.value);
            if (engine->hasErrorOrException(sv)) {
                result.scriptValue = sv;
                result.hasError = true;
                injectErrorLocation(result.scriptValue, alternative->condition.location);
                return result;
            }
            if (sv.toBool()) {
                // The condition is true. Continue evaluating the value.
                result.tryNextAlternative = false;
            } else {
                // The condition is false. Try the next alternative or the else value.
                result.tryNextAlternative = true;
                return result;
            }
            sv = engine->evaluate(alternative->overrideListProperties.value);
            if (engine->hasErrorOrException(sv)) {
                result.scriptValue = sv;
                result.hasError = true;
                injectErrorLocation(result.scriptValue,
                                    alternative->overrideListProperties.location);
                return result;
            }
            if (sv.toBool())
                elseCaseValue->setIsExclusiveListValue();
        }
        result.scriptValue = engine->evaluate(value->sourceCodeForEvaluation(),
                                              value->file()->filePath(), value->line());
        return result;
    }

    void handle(ItemValue *value) override
    {
        *result = data->evaluator->scriptValue(value->item());
        if (!result->isValid())
            qDebug() << "SVConverter returned invalid script value.";
    }

    void handle(VariantValue *variantValue) override
    {
        *result = engine->toScriptValue(variantValue->value());
    }
};

bool debugProperties = false;

enum QueryPropertyType
{
    QPTDefault,
    QPTParentProperty
};

EvaluatorScriptClass::EvaluatorScriptClass(ScriptEngine *scriptEngine)
    : QScriptClass(scriptEngine)
    , m_valueCacheEnabled(false)
{
}

QScriptClass::QueryFlags EvaluatorScriptClass::queryProperty(const QScriptValue &object,
                                                             const QScriptString &name,
                                                             QScriptClass::QueryFlags flags,
                                                             uint *id)
{
    Q_UNUSED(flags);

    // We assume that it's safe to save the result of the query in a member of the scriptclass.
    // It must be cleared in the property method before doing any further lookup.
    QBS_ASSERT(m_queryResult.isNull(), return {});

    if (debugProperties)
        qDebug() << "[SC] queryProperty " << object.objectId() << " " << name;

    auto const data = attachedPointer<EvaluationData>(object);
    const QString nameString = name.toString();
    if (nameString == QStringLiteral("parent")) {
        *id = QPTParentProperty;
        m_queryResult.data = data;
        return QScriptClass::HandlesReadAccess;
    }

    *id = QPTDefault;
    if (!data) {
        if (debugProperties)
            qDebug() << "[SC] queryProperty: no data attached";
        return {};
    }

    return queryItemProperty(data, nameString);
}

QScriptClass::QueryFlags EvaluatorScriptClass::queryItemProperty(const EvaluationData *data,
                                                                 const QString &name,
                                                                 bool ignoreParent)
{
    for (const Item *item = data->item; item; item = item->prototype()) {
        m_queryResult.value = item->ownProperty(name);
        if (m_queryResult.value) {
            m_queryResult.data = data;
            m_queryResult.itemOfProperty = item;
            return HandlesReadAccess;
        }
    }

    if (!ignoreParent && data->item && data->item->parent()) {
        if (debugProperties)
            qDebug() << "[SC] queryProperty: query parent";
        EvaluationData parentdata = *data;
        parentdata.item = data->item->parent();
        const QueryFlags qf = queryItemProperty(&parentdata, name, true);
        if (qf.testFlag(HandlesReadAccess)) {
            m_queryResult.foundInParent = true;
            m_queryResult.data = data;
            return qf;
        }
    }

    if (debugProperties)
        qDebug() << "[SC] queryProperty: no such property";
    return {};
}

QString EvaluatorScriptClass::resultToString(const QScriptValue &scriptValue)
{
    return (scriptValue.isObject()
        ? QStringLiteral("[Object: ")
            + QString::number(scriptValue.objectId()) + QLatin1Char(']')
        : scriptValue.toVariant().toString());
}

void EvaluatorScriptClass::collectValuesFromNextChain(const EvaluationData *data, QScriptValue *result,
        const QString &propertyName, const ValuePtr &value)
{
    QScriptValueList lst;
    Set<Value *> oldNextChain = m_currentNextChain;
    for (ValuePtr next = value; next; next = next->next())
        m_currentNextChain.insert(next.get());

    for (ValuePtr next = value; next; next = next->next()) {
        QScriptValue v = data->evaluator->property(next->definingItem(), propertyName);
        const auto se = static_cast<const ScriptEngine *>(engine());
        if (se->hasErrorOrException(v)) {
            *result = se->lastErrorValue(v);
            return;
        }
        if (v.isUndefined())
            continue;
        lst << v;
        if (next->type() == Value::JSSourceValueType
                && std::static_pointer_cast<JSSourceValue>(next)->isExclusiveListValue()) {
            lst = lst.mid(lst.length() - 2);
            break;
        }
    }
    m_currentNextChain = oldNextChain;

    *result = engine()->newArray();
    quint32 k = 0;
    for (const QScriptValue &v : qAsConst(lst)) {
        QBS_ASSERT(!v.isError(), continue);
        if (v.isArray()) {
            const quint32 vlen = v.property(StringConstants::lengthProperty()).toInt32();
            for (quint32 j = 0; j < vlen; ++j)
                result->setProperty(k++, v.property(j));
        } else {
            result->setProperty(k++, v);
        }
    }
}

static QString overriddenSourceDirectory(const Item *item, const QString &defaultValue)
{
    const VariantValuePtr v = item->variantProperty
            (StringConstants::qbsSourceDirPropertyInternal());
    return v ? v->value().toString() : defaultValue;
}

static void makeTypeError(const ErrorInfo &error, QScriptValue &v)
{
    v = v.engine()->currentContext()->throwError(QScriptContext::TypeError,
                                                 error.toString());
}

static void makeTypeError(const PropertyDeclaration &decl, const CodeLocation &location,
                          QScriptValue &v)
{
    const ErrorInfo error(Tr::tr("Value assigned to property '%1' does not have type '%2'.")
                          .arg(decl.name(), decl.typeString()), location);
    makeTypeError(error, v);
}

static void convertToPropertyType_impl(const QString &pathPropertiesBaseDir, const Item *item,
                                       const PropertyDeclaration& decl,
                                       const CodeLocation &location, QScriptValue &v)
{
    if (v.isUndefined() || v.isError())
        return;
    QString srcDir;
    QString actualBaseDir;
    if (item && !pathPropertiesBaseDir.isEmpty()) {
        const VariantValueConstPtr itemSourceDir
                = item->variantProperty(QStringLiteral("sourceDirectory"));
        actualBaseDir = itemSourceDir ? itemSourceDir->value().toString() : pathPropertiesBaseDir;
    }
    switch (decl.type()) {
    case PropertyDeclaration::UnknownType:
    case PropertyDeclaration::Variant:
        break;
    case PropertyDeclaration::Boolean:
        if (!v.isBool())
            v = v.toBool();
        break;
    case PropertyDeclaration::Integer:
        if (!v.isNumber())
            makeTypeError(decl, location, v);
        break;
    case PropertyDeclaration::Path:
    {
        if (!v.isString()) {
            makeTypeError(decl, location, v);
            break;
        }
        const QString srcDir = item ? overriddenSourceDirectory(item, actualBaseDir)
                                    : pathPropertiesBaseDir;
        if (!srcDir.isEmpty())
            v = v.engine()->toScriptValue(QDir::cleanPath(
                                              FileInfo::resolvePath(srcDir, v.toString())));
        break;
    }
    case PropertyDeclaration::String:
        if (!v.isString())
            makeTypeError(decl, location, v);
        break;
    case PropertyDeclaration::PathList:
        srcDir = item ? overriddenSourceDirectory(item, actualBaseDir)
                      : pathPropertiesBaseDir;
        // Fall-through.
    case PropertyDeclaration::StringList:
    {
        if (!v.isArray()) {
            QScriptValue x = v.engine()->newArray(1);
            x.setProperty(0, v);
            v = x;
        }
        const quint32 c = v.property(StringConstants::lengthProperty()).toUInt32();
        for (quint32 i = 0; i < c; ++i) {
            QScriptValue elem = v.property(i);
            if (elem.isUndefined()) {
                ErrorInfo error(Tr::tr("Element at index %1 of list property '%2' is undefined. "
                                       "String expected.").arg(i).arg(decl.name()), location);
                makeTypeError(error, v);
                break;
            }
            if (elem.isNull()) {
                ErrorInfo error(Tr::tr("Element at index %1 of list property '%2' is null. "
                                       "String expected.").arg(i).arg(decl.name()), location);
                makeTypeError(error, v);
                break;
            }
            if (!elem.isString()) {
                ErrorInfo error(Tr::tr("Element at index %1 of list property '%2' does not have "
                                       "string type.").arg(i).arg(decl.name()), location);
                makeTypeError(error, v);
                break;
            }
            if (srcDir.isEmpty())
                continue;
            elem = v.engine()->toScriptValue(
                        QDir::cleanPath(FileInfo::resolvePath(srcDir, elem.toString())));
            v.setProperty(i, elem);
        }
        break;
    }
    case PropertyDeclaration::VariantList:
        if (!v.isArray()) {
            QScriptValue x = v.engine()->newArray(1);
            x.setProperty(0, v);
            v = x;
        }
        break;
    }
}

void EvaluatorScriptClass::convertToPropertyType(const PropertyDeclaration &decl,
                                                 const CodeLocation &loc, QScriptValue &v)
{
    convertToPropertyType_impl(QString(), nullptr, decl, loc, v);
}

void EvaluatorScriptClass::convertToPropertyType(const Item *item, const PropertyDeclaration& decl,
                                                 const Value *value, QScriptValue &v)
{
    if (value->type() == Value::VariantValueType && v.isUndefined() && !decl.isScalar()) {
        v = v.engine()->newArray(); // QTBUG-51237
        return;
    }
    convertToPropertyType_impl(m_pathPropertiesBaseDir, item, decl, value->location(), v);
}

class PropertyStackManager
{
public:
    PropertyStackManager(const Item *itemOfProperty, const QScriptString &name, const Value *value,
                         std::stack<QualifiedId> &requestedProperties,
                         PropertyDependencies &propertyDependencies)
        : m_requestedProperties(requestedProperties)
    {
        if (value->type() == Value::JSSourceValueType
                && (itemOfProperty->type() == ItemType::ModuleInstance
                    || itemOfProperty->type() == ItemType::Module
                    || itemOfProperty->type() == ItemType::Export)) {
            const VariantValueConstPtr varValue
                    = itemOfProperty->variantProperty(StringConstants::nameProperty());
            if (!varValue)
                return;
            m_stackUpdate = true;
            const QualifiedId fullPropName
                    = QualifiedId::fromString(varValue->value().toString()) << name.toString();
            if (!requestedProperties.empty())
                propertyDependencies[fullPropName].insert(requestedProperties.top());
            m_requestedProperties.push(fullPropName);
        }
    }

    ~PropertyStackManager()
    {
        if (m_stackUpdate)
            m_requestedProperties.pop();
    }

private:
    std::stack<QualifiedId> &m_requestedProperties;
    bool m_stackUpdate = false;
};

QScriptValue EvaluatorScriptClass::property(const QScriptValue &object, const QScriptString &name,
                                            uint id)
{
    const bool foundInParent = m_queryResult.foundInParent;
    const EvaluationData *data = m_queryResult.data;
    const Item * const itemOfProperty = m_queryResult.itemOfProperty;
    m_queryResult.foundInParent = false;
    m_queryResult.data = nullptr;
    m_queryResult.itemOfProperty = nullptr;
    QBS_ASSERT(data, {});

    const auto qpt = static_cast<QueryPropertyType>(id);
    if (qpt == QPTParentProperty) {
        return data->item->parent()
                ? data->evaluator->scriptValue(data->item->parent())
                : engine()->undefinedValue();
    }

    ValuePtr value;
    m_queryResult.value.swap(value);
    QBS_ASSERT(value, return {});
    QBS_ASSERT(m_queryResult.isNull(), return {});

    if (debugProperties)
        qDebug() << "[SC] property " << name;

    PropertyStackManager propStackmanager(itemOfProperty, name, value.get(),
                                          m_requestedProperties, m_propertyDependencies);

    QScriptValue result;
    if (m_valueCacheEnabled) {
        result = data->valueCache.value(name);
        if (result.isValid()) {
            if (debugProperties)
                qDebug() << "[SC] cache hit " << name << ": " << resultToString(result);
            return result;
        }
    }

    if (value->next() && !m_currentNextChain.contains(value.get())) {
        collectValuesFromNextChain(data, &result, name.toString(), value);
    } else {
        QScriptValue parentObject;
        if (foundInParent)
            parentObject = data->evaluator->scriptValue(data->item->parent());
        SVConverter converter(this, foundInParent ? &parentObject : &object, value, itemOfProperty,
                              &name, data, &result);
        converter.start();

        const PropertyDeclaration decl = data->item->propertyDeclaration(name.toString());
        convertToPropertyType(data->item, decl, value.get(), result);
    }

    if (debugProperties)
        qDebug() << "[SC] cache miss " << name << ": " << resultToString(result);
    if (m_valueCacheEnabled)
        data->valueCache.insert(name, result);
    return result;
}

class EvaluatorScriptClassPropertyIterator : public QScriptClassPropertyIterator
{
public:
    EvaluatorScriptClassPropertyIterator(const QScriptValue &object, EvaluationData *data)
        : QScriptClassPropertyIterator(object), m_it(data->item->properties())
    {
    }

    bool hasNext() const override
    {
        return m_it.hasNext();
    }

    void next() override
    {
        m_it.next();
    }

    bool hasPrevious() const override
    {
        return m_it.hasPrevious();
    }

    void previous() override
    {
        m_it.previous();
    }

    void toFront() override
    {
        m_it.toFront();
    }

    void toBack() override
    {
        m_it.toBack();
    }

    QScriptString name() const override
    {
        return object().engine()->toStringHandle(m_it.key());
    }

private:
    QMapIterator<QString, ValuePtr> m_it;
};

QScriptClassPropertyIterator *EvaluatorScriptClass::newIterator(const QScriptValue &object)
{
    auto const data = attachedPointer<EvaluationData>(object);
    return data ? new EvaluatorScriptClassPropertyIterator(object, data) : nullptr;
}

void EvaluatorScriptClass::setValueCacheEnabled(bool enabled)
{
    m_valueCacheEnabled = enabled;
}

} // namespace Internal
} // namespace qbs
