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

#include "evaluator.h"

#include "filecontext.h"
#include "filetags.h"
#include "item.h"
#include "scriptengine.h"
#include "value.h"

#include <quickjs.h>

#include <buildgraph/buildgraph.h>
#include <jsextensions/jsextensions.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/scripttools.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/stringconstants.h>

#include <QtCore/qdebug.h>

namespace qbs {
namespace Internal {

class EvaluationData
{
public:
    Evaluator *evaluator = nullptr;
    const Item *item = nullptr;
    mutable QHash<QString, JSValue> valueCache;
};

static void convertToPropertyType_impl(
    ScriptEngine *engine, const QString &pathPropertiesBaseDir, const Item *item,
    const PropertyDeclaration& decl, const Value *value, const CodeLocation &location, JSValue &v);
static int getEvalPropertyNames(JSContext *ctx, JSPropertyEnum **ptab, uint32_t *plen,
                                JSValueConst obj);
static int getEvalProperty(JSContext *ctx, JSPropertyDescriptor *desc,
                           JSValueConst obj, JSAtom prop);
static int getEvalPropertySafe(JSContext *ctx, JSPropertyDescriptor *desc,
                               JSValueConst obj, JSAtom prop);

static bool debugProperties = false;

Evaluator::Evaluator(ScriptEngine *scriptEngine)
    : m_scriptEngine(scriptEngine)
    , m_scriptClass(scriptEngine->registerClass("Evaluator", nullptr, nullptr, JS_UNDEFINED,
                                                getEvalPropertyNames, getEvalPropertySafe))
{
    scriptEngine->registerEvaluator(this);
}

Evaluator::~Evaluator()
{
    Set<JSValue> valuesToFree;
    for (const auto &data : std::as_const(m_scriptValueMap)) {
        const auto evalData = attachedPointer<EvaluationData>(data, m_scriptClass);
        valuesToFree << data;
        for (const JSValue cachedValue : evalData->valueCache)
            JS_FreeValue(m_scriptEngine->context(), cachedValue);
        evalData->item->removeObserver(this);
        delete evalData;
    }
    for (const auto &scopes : std::as_const(m_fileContextScopesMap)) {
        valuesToFree << scopes.fileScope;
        valuesToFree << scopes.importScope;
    }
    for (const JSValue v : std::as_const(valuesToFree)) {
        JS_FreeValue(m_scriptEngine->context(), v);
    }
    m_scriptEngine->unregisterEvaluator(this);
}

JSValue Evaluator::property(const Item *item, const QString &name)
{
    return getJsProperty(m_scriptEngine->context(), scriptValue(item), name);
}

JSValue Evaluator::value(const Item *item, const QString &name, bool *propertyWasSet)
{
    JSValue v;
    evaluateProperty(&v, item, name, propertyWasSet);
    return v;
}

bool Evaluator::boolValue(const Item *item, const QString &name,
                          bool *propertyWasSet)
{
    const ScopedJsValue sv(m_scriptEngine->context(), value(item, name, propertyWasSet));
    return JS_ToBool(m_scriptEngine->context(), sv);
}

int Evaluator::intValue(const Item *item, const QString &name, int defaultValue,
                        bool *propertyWasSet)
{
    JSValue v;
    if (!evaluateProperty(&v, item, name, propertyWasSet))
        return defaultValue;
    qint32 n;
    JS_ToInt32(m_scriptEngine->context(), &n, v);
    return n;
}

FileTags Evaluator::fileTagsValue(const Item *item, const QString &name, bool *propertySet)
{
    return FileTags::fromStringList(stringListValue(item, name, propertySet));
}

QString Evaluator::stringValue(const Item *item, const QString &name,
                               const QString &defaultValue, bool *propertyWasSet)
{
    JSValue v;
    if (!evaluateProperty(&v, item, name, propertyWasSet))
        return defaultValue;
    QString str = getJsString(m_scriptEngine->context(), v);
    JS_FreeValue(m_scriptEngine->context(), v);
    return str;
}

static QStringList toStringList(ScriptEngine *engine, const JSValue &scriptValue)
{
    if (JS_IsString(scriptValue))
        return {getJsString(engine->context(), scriptValue)};
    if (JS_IsArray(engine->context(), scriptValue)) {
        QStringList lst;
        int i = 0;
        for (;;) {
            JSValue elem = JS_GetPropertyUint32(engine->context(), scriptValue, i++);
            if (JS_IsUndefined(elem))
                break;
            lst.push_back(getJsString(engine->context(), elem));
            JS_FreeValue(engine->context(), elem);
        }
        return lst;
    }
    return {};
}

QStringList Evaluator::stringListValue(const Item *item, const QString &name, bool *propertyWasSet)
{
    const auto result = optionalStringListValue(item, name, propertyWasSet);
    return result ? *result : QStringList();
}

std::optional<QStringList> Evaluator::optionalStringListValue(
        const Item *item, const QString &name, bool *propertyWasSet)
{
    const ScopedJsValue v(m_scriptEngine->context(), property(item, name));
    handleEvaluationError(item, name);
    if (propertyWasSet)
        *propertyWasSet = isNonDefaultValue(item, name);
    if (JS_IsUndefined(v))
        return std::nullopt;
    return toStringList(m_scriptEngine, v);
}

QVariant Evaluator::variantValue(const Item *item, const QString &name, bool *propertySet)
{
    const ScopedJsValue jsValue(m_scriptEngine->context(), property(item, name));
    handleEvaluationError(item, name);
    if (propertySet)
        *propertySet = isNonDefaultValue(item, name);
    return getJsVariant(m_scriptEngine->context(), jsValue);
}

bool Evaluator::isNonDefaultValue(const Item *item, const QString &name) const
{
    const ValueConstPtr v = item->property(name);
    return v && (v->type() != Value::JSSourceValueType
                 || !static_cast<const JSSourceValue *>(v.get())->isBuiltinDefaultValue());
}

void Evaluator::convertToPropertyType(const PropertyDeclaration &decl, const CodeLocation &loc,
                                      JSValue &v)
{
    convertToPropertyType_impl(engine(), QString(), nullptr, decl, nullptr, loc, v);
}

JSValue Evaluator::scriptValue(const Item *item)
{
    JSValue &scriptValue = m_scriptValueMap[item];
    if (JS_IsObject(scriptValue)) {
        // already initialized
        return scriptValue;
    }

    const auto edata = new EvaluationData;
    edata->evaluator = this;
    edata->item = item;
    edata->item->addObserver(this);

    scriptValue = JS_NewObjectClass(m_scriptEngine->context(), m_scriptClass);
    attachPointerTo(scriptValue, edata);
    return scriptValue;
}

void Evaluator::handleEvaluationError(const Item *item, const QString &name)
{
    throwOnEvaluationError(m_scriptEngine, [&item, &name] () {
        const ValueConstPtr &value = item->property(name);
        return value ? value->location() : CodeLocation();
    });
}

bool Evaluator::evaluateProperty(JSValue *result, const Item *item, const QString &name,
        bool *propertyWasSet)
{
    *result = property(item, name);
    ScopedJsValue valMgr(m_scriptEngine->context(), *result);
    handleEvaluationError(item, name);
    valMgr.release();
    if (propertyWasSet)
        *propertyWasSet = isNonDefaultValue(item, name);
    return !JS_IsUndefined(*result);
}

Evaluator::FileContextScopes Evaluator::fileContextScopes(const FileContextConstPtr &file)
{
    FileContextScopes &result = m_fileContextScopesMap[file];
    if (!JS_IsObject(result.fileScope)) {
        if (file->idScope())
            result.fileScope = scriptValue(file->idScope());
        else
            result.fileScope = m_scriptEngine->newObject();
        setJsProperty(m_scriptEngine->context(), result.fileScope,
                      StringConstants::filePathGlobalVar(), file->filePath());
        setJsProperty(m_scriptEngine->context(), result.fileScope,
                      StringConstants::pathGlobalVar(), file->dirPath());
    }
    if (!JS_IsObject(result.importScope)) {
        try {
            ScopedJsValue importScope(m_scriptEngine->context(), m_scriptEngine->newObject());
            setupScriptEngineForFile(m_scriptEngine, file, importScope, ObserveMode::Enabled);
            result.importScope = importScope.release();
        } catch (const ErrorInfo &e) {
            result.importScope = throwError(m_scriptEngine->context(), e.toString());
        }
    }
    return result;
}

// This is the only function in this class that can be called from a thread that is not
// the evaluating one. For this reason, we do not clear the cache here, as that would
// incur enourmous synchronization overhead. Instead, we mark the item's cache as invalidated
// and do the actual clearing only at the very few places where the cache is actually accessed.
void Evaluator::invalidateCache(const Item *item)
{
    std::lock_guard lock(m_cacheInvalidationMutex);
    m_invalidatedCaches << item;
}

void Evaluator::clearCache(const Item *item)
{
    std::lock_guard lock(m_cacheInvalidationMutex);
    if (const auto data = attachedPointer<EvaluationData>(m_scriptValueMap.value(item),
                                                          m_scriptEngine->dataWithPtrClass())) {
        clearCache(*data);
        m_invalidatedCaches.remove(data->item);
    }
}

void Evaluator::clearCacheIfInvalidated(EvaluationData &edata)
{
    std::lock_guard lock(m_cacheInvalidationMutex);
    if (const auto it = m_invalidatedCaches.find(edata.item); it != m_invalidatedCaches.end()) {
        clearCache(edata);
        m_invalidatedCaches.erase(it);
    }
}

void Evaluator::clearCache(EvaluationData &edata)
{
    for (const auto value : std::as_const(edata.valueCache))
        JS_FreeValue(m_scriptEngine->context(), value);
    edata.valueCache.clear();
}

void throwOnEvaluationError(ScriptEngine *engine,
                            const std::function<CodeLocation()> &provideFallbackCodeLocation)
{
    if (JsException ex = engine->checkAndClearException(provideFallbackCodeLocation()))
        throw ex.toErrorInfo();
}

static void makeTypeError(ScriptEngine *engine, const ErrorInfo &error, JSValue &v)
{
    v = throwError(engine->context(), error.toString());
}

static void makeTypeError(ScriptEngine *engine, const PropertyDeclaration &decl,
                          const CodeLocation &location, JSValue &v)
{
    const ErrorInfo error(Tr::tr("Value assigned to property '%1' does not have type '%2'.")
                          .arg(decl.name(), decl.typeString()), location);
    makeTypeError(engine, error, v);
}

static QString overriddenSourceDirectory(const Item *item, const QString &defaultValue)
{
    const VariantValuePtr v = item->variantProperty
            (StringConstants::qbsSourceDirPropertyInternal());
    return v ? v->value().toString() : defaultValue;
}

static void convertToPropertyType_impl(ScriptEngine *engine,
                                       const QString &pathPropertiesBaseDir, const Item *item,
                                       const PropertyDeclaration& decl,
                                       const Value *value, const CodeLocation &location, JSValue &v)
{
    JSContext * const ctx = engine->context();
    if (JS_IsUndefined(v) || JS_IsError(ctx, v) || JS_IsException(v))
        return;
    QString srcDir;
    QString actualBaseDir;
    const Item * const srcDirItem = value && value->scope() ? value->scope() : item;
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
        if (!JS_IsBool(v))
            v = JS_NewBool(ctx, JS_ToBool(ctx, v));
        break;
    case PropertyDeclaration::Integer:
        if (!JS_IsNumber(v))
            makeTypeError(engine, decl, location, v);
        break;
    case PropertyDeclaration::Path:
    {
        if (!JS_IsString(v)) {
            makeTypeError(engine, decl, location, v);
            break;
        }
        const QString srcDir = srcDirItem ? overriddenSourceDirectory(srcDirItem, actualBaseDir)
                                          : pathPropertiesBaseDir;
        if (!srcDir.isEmpty()) {
            v = engine->toScriptValue(QDir::cleanPath(FileInfo::resolvePath(srcDir,
                                                                            getJsString(ctx, v))));
            JS_FreeValue(ctx, v);
        }
        break;
    }
    case PropertyDeclaration::String:
        if (!JS_IsString(v))
            makeTypeError(engine, decl, location, v);
        break;
    case PropertyDeclaration::PathList:
        srcDir = srcDirItem ? overriddenSourceDirectory(srcDirItem, actualBaseDir)
                            : pathPropertiesBaseDir;
        // Fall-through.
    case PropertyDeclaration::StringList:
    {
        if (!JS_IsArray(ctx, v)) {
            JSValue x = engine->newArray(1, JsValueOwner::ScriptEngine);
            JS_SetPropertyUint32(ctx, x, 0, JS_DupValue(ctx, v));
            v = x;
        }
        const quint32 c = getJsIntProperty(ctx, v, StringConstants::lengthProperty());
        for (quint32 i = 0; i < c; ++i) {
            const ScopedJsValue elem(ctx, JS_GetPropertyUint32(ctx, v, i));
            if (JS_IsUndefined(elem)) {
                ErrorInfo error(Tr::tr("Element at index %1 of list property '%2' is undefined. "
                                       "String expected.").arg(i).arg(decl.name()), location);
                makeTypeError(engine, error, v);
                break;
            }
            if (JS_IsNull(elem)) {
                ErrorInfo error(Tr::tr("Element at index %1 of list property '%2' is null. "
                                       "String expected.").arg(i).arg(decl.name()), location);
                makeTypeError(engine, error, v);
                break;
            }
            if (!JS_IsString(elem)) {
                ErrorInfo error(Tr::tr("Element at index %1 of list property '%2' does not have "
                                       "string type.").arg(i).arg(decl.name()), location);
                makeTypeError(engine, error, v);
                break;
            }
            if (srcDir.isEmpty())
                continue;
            const JSValue newElem = engine->toScriptValue(
                        QDir::cleanPath(FileInfo::resolvePath(srcDir, getJsString(ctx, elem))));
            JS_SetPropertyUint32(ctx, v, i, newElem);
        }
        break;
    }
    case PropertyDeclaration::VariantList:
        if (!JS_IsArray(ctx, v)) {
            JSValue x = engine->newArray(1, JsValueOwner::ScriptEngine);
            JS_SetPropertyUint32(ctx, x, 0, JS_DupValue(ctx, v));
            v = x;
        }
        break;
    }
}

static int getEvalPropertyNames(JSContext *ctx, JSPropertyEnum **ptab, uint32_t *plen, JSValue obj)
{
    ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
    const Evaluator * const evaluator = engine->evaluator();
    const auto data = attachedPointer<EvaluationData>(obj, evaluator->classId());
    if (!data)
        return -1;
    const Item::PropertyMap &map = data->item->properties();
    *plen = map.size();
    if (!map.isEmpty()) {
        *ptab = reinterpret_cast<JSPropertyEnum *>(js_malloc(ctx, *plen * sizeof **ptab));
        JSPropertyEnum *entry = *ptab;
        for (auto it = map.cbegin(); it != map.cend(); ++it, ++entry) {
            entry->atom = JS_NewAtom(ctx, it.key().toUtf8().constData());
            entry->is_enumerable = 1;
        }
    } else {
        *ptab = nullptr;
    }
    return 0;
}

class PropertyStackManager
{
public:
    PropertyStackManager(const Item *itemOfProperty, const QString &name, const Value *value,
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
                    = QualifiedId::fromString(varValue->value().toString()) << name;
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

class SVConverter : ValueHandler
{
    ScriptEngine * const engine;
    const JSValue * const object;
    Value * const valuePtr;
    const Item * const itemOfProperty;
    const QString * const propertyName;
    const EvaluationData * const data;
    JSValue * const result;
    JSValueList scopeChain;
    char pushedScopesCount;

public:

    SVConverter(ScriptEngine *engine, const JSValue *obj, const ValuePtr &v,
                const Item *_itemOfProperty, const QString *propertyName,
                const EvaluationData *data, JSValue *result)
        : engine(engine)
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

    void setupConvenienceProperty(const QString &conveniencePropertyName, JSValue *extraScope,
                                  const JSValue &scriptValue)
    {
        if (!JS_IsObject(*extraScope))
            *extraScope = engine->newObject();
        const PropertyDeclaration::Type type
                = itemOfProperty->propertyDeclaration(*propertyName).type();
        const bool isArray = type == PropertyDeclaration::StringList
                || type == PropertyDeclaration::PathList
                || type == PropertyDeclaration::Variant // TODO: Why?
                || type == PropertyDeclaration::VariantList;
        JSValue valueToSet = JS_DupValue(engine->context(), scriptValue);
        if (isArray && JS_IsUndefined(valueToSet))
            valueToSet = engine->newArray(0, JsValueOwner::Caller);
        setJsProperty(engine->context(), *extraScope, conveniencePropertyName, valueToSet);
    }

    std::pair<JSValue, bool> createExtraScope(const JSSourceValue *value, Item *outerItem,
                                              JSValue *outerScriptValue)
    {
        std::pair<JSValue, bool> result;
        auto &extraScope = result.first;
        result.second = true;
        if (value->sourceUsesBase()) {
            JSValue baseValue = JS_UNDEFINED;
            if (value->baseValue()) {
                SVConverter converter(engine, object, value->baseValue(), itemOfProperty,
                                      propertyName, data, &baseValue);
                converter.start();
            }
            setupConvenienceProperty(StringConstants::baseVar(), &extraScope, baseValue);
        }
        if (value->sourceUsesOuter()) {
            JSValue v = JS_UNDEFINED;
            bool doSetup = false;
            if (outerItem) {
                v = data->evaluator->property(outerItem, *propertyName);
                if (JsException ex = engine->checkAndClearException({})) {
                    extraScope = engine->throwError(ex.toErrorInfo().toString());
                    result.second = false;
                    return result;
                }
                doSetup = true;
                JS_FreeValue(engine->context(), v);
            } else if (outerScriptValue) {
                doSetup = true;
                v = *outerScriptValue;
            }
            if (doSetup)
                setupConvenienceProperty(StringConstants::outerVar(), &extraScope, v);
        }
        if (value->sourceUsesOriginal()) {
            JSValue originalJs = JS_UNDEFINED;
            ScopedJsValue originalMgr(engine->context(), JS_UNDEFINED);
            if (data->item->propertyDeclaration(*propertyName).isScalar()) {
                const Item *item = itemOfProperty;

                if (item->type() != ItemType::ModuleInstance
                    && item->type() != ItemType::ModuleInstancePlaceholder) {
                    const QString errorMessage = Tr::tr("The special value 'original' can only "
                                                        "be used with module properties.");
                    extraScope = throwError(engine->context(), errorMessage);
                    result.second = false;
                    return result;
                }

                if (!value->scope()) {
                    const QString errorMessage = Tr::tr("The special value 'original' cannot "
                        "be used on the right-hand side of a property declaration.");
                    extraScope = throwError(engine->context(), errorMessage);
                    result.second = false;
                    return result;
                }

                ValuePtr original;
                for (const ValuePtr &v : value->candidates()) {
                    if (!v->scope()) {
                        original = v;
                        break;
                    }
                }

                // This can happen when resolving shadow products. The error will be ignored
                // in that case.
                if (!original) {
                    const QString errorMessage = Tr::tr("Error setting up 'original'.");
                    extraScope = throwError(engine->context(), errorMessage);
                    result.second = false;
                    return result;
                }

                SVConverter converter(engine, object, original, item, propertyName, data,
                                      &originalJs);
                converter.start();
            } else {
                originalJs = engine->newArray(0, JsValueOwner::Caller);
                originalMgr.setValue(originalJs);
            }
            setupConvenienceProperty(StringConstants::originalVar(), &extraScope, originalJs);
        }
        return result;
    }

    void pushScope(const JSValue &scope)
    {
        if (JS_IsObject(scope)) {
            scopeChain << scope;
            ++pushedScopesCount;
        }
    }

    void pushScopeRecursively(const Item *scope)
    {
        if (scope) {
            pushScopeRecursively(scope->scope());
            pushScope(data->evaluator->scriptValue(scope));
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
            scopeChain.pop_back();
    }

    void handle(JSSourceValue *value) override
    {
        JSValue outerScriptValue = JS_UNDEFINED;
        for (const JSSourceValue::Alternative &alternative : value->alternatives()) {
            if (alternative.value->sourceUsesOuter()
                    && !data->item->outerItem()
                    && JS_IsUndefined(outerScriptValue)) {
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
        JSValue scriptValue = JS_UNDEFINED;
        bool tryNextAlternative = true;
        bool hasError = false;
    };

    JSSourceValueEvaluationResult evaluateJSSourceValue(const JSSourceValue *value, Item *outerItem,
            const JSSourceValue::Alternative *alternative = nullptr,
            JSSourceValue *elseCaseValue = nullptr, JSValue *outerScriptValue = nullptr)
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
        const ScopedJsValue extraScopeMgr(engine->context(), maybeExtraScope.first);
        const Evaluator::FileContextScopes fileCtxScopes
                = data->evaluator->fileContextScopes(value->file());
        if (JsException ex = engine->checkAndClearException({})) {
            result.scriptValue = engine->throwError(ex.toErrorInfo().toString());
            result.hasError = true;
            return result;
        }
        pushScope(fileCtxScopes.fileScope);
        pushItemScopes(data->item);
        if ((itemOfProperty->type() != ItemType::ModuleInstance
             && itemOfProperty->type() != ItemType::ModuleInstancePlaceholder) || !value->scope()) {
            pushScope(*object);
        }
        pushScopeRecursively(value->scope());
        pushScope(maybeExtraScope.first);
        pushScope(fileCtxScopes.importScope);
        if (alternative) {
            ScopedJsValue sv(engine->context(), engine->evaluate(JsValueOwner::Caller,
                    alternative->condition.value, {}, 1, scopeChain));
            if (JsException ex = engine->checkAndClearException(alternative->condition.location)) {

                // This handles cases like the following:
                //   Depends { name: "cpp" }
                //   Properties {
                //     condition: qbs.targetOS.contains("darwin")
                //     bundle.isBundle: false
                //   }
                // On non-Darwin platforms, bundle never gets instantiated, and thus bundle.isBundle
                // has no scope, so the qbs item in the condition is not found.
                // TODO: Ideally, we would never evaluate values in placeholder items, but
                //       there are currently several contexts where we do that, e.g. Export
                //       and Group items. Perhaps change that, or try to collect all such
                //       exceptions and don't try to evaluate other cases.
                if (itemOfProperty->type() == ItemType::ModuleInstancePlaceholder) {
                    result.scriptValue = JS_UNDEFINED;
                    result.tryNextAlternative = false;
                    return result;
                }

                result.scriptValue = engine->throwError(ex.toErrorInfo().toString());
                //result.scriptValue = JS_Throw(engine->context(), ex.takeValue());
                //result.scriptValue = ex.takeValue();
                result.hasError = true;
                return result;
            }
            if (JS_ToBool(engine->context(), sv)) {
                // The condition is true. Continue evaluating the value.
                result.tryNextAlternative = false;
            } else {
                // The condition is false. Try the next alternative or the else value.
                result.tryNextAlternative = true;
                return result;
            }
            sv.reset(engine->evaluate(JsValueOwner::Caller,
                                  alternative->overrideListProperties.value, {}, 1, scopeChain));
            if (JsException ex = engine->checkAndClearException(
                        alternative->overrideListProperties.location)) {
                result.scriptValue = engine->throwError(ex.toErrorInfo().toString());
                //result.scriptValue = JS_Throw(engine->context(), ex.takeValue());
                result.hasError = true;
                return result;
            }
            if (JS_ToBool(engine->context(), sv))
                elseCaseValue->setIsExclusiveListValue();
        }
        result.scriptValue = engine->evaluate(JsValueOwner::ScriptEngine,
                value->sourceCodeForEvaluation(), value->file()->filePath(), value->line(),
                scopeChain);
        return result;
    }

    void handle(ItemValue *value) override
    {
        *result = data->evaluator->scriptValue(value->item());
        if (JS_IsUninitialized(*result))
            qDebug() << "SVConverter returned invalid script value.";
    }

    void handle(VariantValue *variantValue) override
    {
        *result = engine->toScriptValue(variantValue->value(), variantValue->id());
        engine->takeOwnership(*result);
    }
};

static void convertToPropertyType(ScriptEngine *engine, const Item *item,
                                  const PropertyDeclaration& decl, const Value *value, JSValue &v)
{
    if (value->type() == Value::VariantValueType && JS_IsUndefined(v) && !decl.isScalar()) {
        v = engine->newArray(0, JsValueOwner::ScriptEngine); // QTBUG-51237
        return;
    }
    convertToPropertyType_impl(engine, engine->evaluator()->pathPropertiesBaseDir(), item, decl,
                               value, value->location(), v);
}

static QString resultToString(JSContext *ctx, const JSValue &scriptValue)
{
    if (JS_IsUndefined(scriptValue))
        return QLatin1String("undefined");
    if (JS_IsArray(ctx, scriptValue))
        return getJsStringList(ctx, scriptValue).join(QLatin1Char(','));
    if (JS_IsObject(scriptValue)) {
        return QStringLiteral("[Object: ")
                + QString::number(jsObjectId(scriptValue)) + QLatin1Char(']');
    }
    return getJsVariant(ctx, scriptValue).toString();
}

static void collectValuesFromNextChain(
        ScriptEngine *engine, JSValue obj, const ValuePtr &value, const Item *itemOfProperty, const QString &name,
        const EvaluationData *data, JSValue *result)
{
    JSValueList lst;
    for (ValuePtr next = value; next; next = next->next()) {
        JSValue v = JS_UNDEFINED;
        SVConverter svc(engine, &obj, next, itemOfProperty, &name, data, &v);
        svc.start();
        if (JsException ex = engine->checkAndClearException({})) {
            const ScopedJsValueList l(engine->context(), lst);
            *result = engine->throwError(ex.toErrorInfo().toString());
            return;
        }
        if (JS_IsUndefined(v))
            continue;
        const PropertyDeclaration decl = data->item->propertyDeclaration(name);
        convertToPropertyType(engine, data->item, decl, next.get(), v);
        lst.push_back(JS_DupValue(engine->context(), v));
        if (next->type() == Value::JSSourceValueType
                && std::static_pointer_cast<JSSourceValue>(next)->isExclusiveListValue()) {
            // TODO: Why on earth do we keep the last _2_ elements?
            auto keepIt = lst.rbegin();
            for (int i = 0; i < 2 && keepIt != lst.rend(); ++i)
                ++keepIt;
            for (auto it = lst.begin(); it < keepIt.base(); ++it)
                JS_FreeValue(engine->context(), *it);
            lst.erase(lst.begin(), keepIt.base());
            break;
        }
    }

    *result = engine->newArray(int(lst.size()), JsValueOwner::ScriptEngine);
    quint32 k = 0;
    JSContext * const ctx = engine->context();
    for (const JSValue &v : std::as_const(lst)) {
        QBS_ASSERT(!JS_IsError(ctx, v), continue);
        if (JS_IsArray(ctx, v)) {
            const quint32 vlen = getJsIntProperty(ctx, v, StringConstants::lengthProperty());
            for (quint32 j = 0; j < vlen; ++j)
                JS_SetPropertyUint32(ctx, *result, k++, JS_GetPropertyUint32(ctx, v, j));
            JS_FreeValue(ctx, v);
        } else {
            JS_SetPropertyUint32(ctx, *result, k++, v);
        }
    }
    setJsProperty(ctx, *result, StringConstants::lengthProperty(), JS_NewInt32(ctx, k));
}

struct EvalResult { JSValue v = JS_UNDEFINED; bool found = false; };
static EvalResult getEvalProperty(ScriptEngine *engine, JSValue obj, const Item *item,
                                  const QString &name, EvaluationData *data)
{
    Evaluator * const evaluator = data->evaluator;
    const bool isModuleInstance = item->type() == ItemType::ModuleInstance
            || item->type() == ItemType::ModuleInstancePlaceholder;
    for (; item; item = item->prototype()) {
        if (isModuleInstance
            && (item->type() == ItemType::Module || item->type() == ItemType::Export)) {
            break; // There's no property in a prototype that's not also in the instance.
        }
        const ValuePtr value = item->ownProperty(name);
        if (!value)
            continue;
        const Item * const itemOfProperty = item;     // The item that owns the property.
        PropertyStackManager propStackmanager(itemOfProperty, name, value.get(),
                                              evaluator->requestedProperties(),
                                              evaluator->propertyDependencies());
        JSValue result;
        if (evaluator->cachingEnabled()) {
            data->evaluator->clearCacheIfInvalidated(*data);
            const auto result = data->valueCache.constFind(name);
            if (result != data->valueCache.constEnd()) {
                if (debugProperties)
                    qDebug() << "[SC] cache hit " << name << ": "
                             << resultToString(engine->context(), *result);
                return {*result, true};
            }
        }

        if (value->next()) {
            collectValuesFromNextChain(engine, obj, value, itemOfProperty, name, data, &result);
        } else {
            SVConverter converter(engine, &obj, value, itemOfProperty, &name, data, &result);
            converter.start();
            const PropertyDeclaration decl = data->item->propertyDeclaration(name);
            convertToPropertyType(engine, data->item, decl, value.get(), result);
        }

        if (debugProperties)
            qDebug() << "[SC] cache miss " << name << ": "
                     << resultToString(engine->context(), result);
        if (evaluator->cachingEnabled()) {
            data->evaluator->clearCacheIfInvalidated(*data);
            const auto it = data->valueCache.find(name);
            if (it != data->valueCache.end()) {
                JS_FreeValue(engine->context(), it.value());
                it.value() = JS_DupValue(engine->context(), result);
            } else {
                data->valueCache.insert(name, JS_DupValue(engine->context(), result));
            }
        }
        return {result, true};
    }
    return {JS_UNDEFINED, false};
}

static int getEvalProperty(JSContext *ctx, JSPropertyDescriptor *desc, JSValue obj, JSAtom prop)
{
    if (desc) {
        desc->getter = desc->setter = desc->value = JS_UNDEFINED;
        desc->flags = JS_PROP_ENUMERABLE;
    }
    ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
    Evaluator * const evaluator = engine->evaluator();
    const auto data = attachedPointer<EvaluationData>(obj, evaluator->classId());
    const QString name = getJsString(ctx, prop);
    if (debugProperties)
        qDebug() << "[SC] queryProperty " << jsObjectId(obj) << " " << name;

    if (name == QStringLiteral("parent")) {
        if (desc) {
            Item * const parent = data->item->parent();
            desc->value = parent
                    ? JS_DupValue(ctx, data->evaluator->scriptValue(data->item->parent()))
                    : JS_UNDEFINED;
        }
        return 1;
    }

    if (!data) {
        if (debugProperties)
            qDebug() << "[SC] queryProperty: no data attached";
        engine->setLastLookupStatus(false);
        return -1;
    }

    EvalResult result = getEvalProperty(engine, obj, data->item, name, data);
    if (!result.found && data->item->parent()) {
        if (debugProperties)
            qDebug() << "[SC] queryProperty: query parent";
        const Item * const parentItem = data->item->parent();
        result = getEvalProperty(engine, evaluator->scriptValue(parentItem), parentItem,
                                 name, data);
    }
    if (result.found) {
        if (desc)
            desc->value = JS_DupValue(ctx, result.v);
        engine->setLastLookupStatus(true);
        return 1;
    }

    if (debugProperties)
        qDebug() << "[SC] queryProperty: no such property";
    engine->setLastLookupStatus(false);
    return 0;
}

static int getEvalPropertySafe(JSContext *ctx, JSPropertyDescriptor *desc, JSValue obj, JSAtom prop)
{
    try {
        return getEvalProperty(ctx, desc, obj, prop);
    } catch (const ErrorInfo &e) {
        ScopedJsValue error(ctx, throwError(ctx, e.toString()));
        return -1;
    }
}

} // namespace Internal
} // namespace qbs
