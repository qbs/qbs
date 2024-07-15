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

#include "builtindeclarations.h"
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
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/scripttools.h>
#include <tools/span.h>
#include <tools/stringconstants.h>

#include <QtCore/qdebug.h>

namespace qbs {
namespace Internal {

class EvaluationData
{
public:
    const Item *item = nullptr;
    mutable QHash<QString, JSValue> valueCache;
};

enum class ConversionType { Full, ElementsOnly };
static void convertToPropertyType_impl(
    ScriptEngine *engine,
    const QString &pathPropertiesBaseDir,
    const Item *item,
    const PropertyDeclaration &decl,
    const Value *value,
    const CodeLocation &location,
    ConversionType conversionType,
    JSValue &v);

static int getEvalPropertyNames(JSContext *ctx, JSPropertyEnum **ptab, uint32_t *plen,
                                JSValueConst obj);
static int getEvalProperty(JSContext *ctx, JSPropertyDescriptor *desc,
                           JSValueConst obj, JSAtom prop);
static int getEvalPropertySafe(JSContext *ctx, JSPropertyDescriptor *desc,
                               JSValueConst obj, JSAtom prop);

static bool debugProperties = false;

static JSValue handleDeprecatedPathResolving(ScriptEngine &engine, const Value &val)
{
    const QString warning = Tr::tr(
        "Resolving path properties relative to the exporting product's location is "
        "deprecated.\nIn future versions of qbs, such properties will be resolved relative to the "
        "importing product's location.\n"
        "Explicitly use exportingProduct.sourceDirectory instead.");
    try {
        engine.handleDeprecation(Version(2, 7), warning, val.location());
    } catch (const ErrorInfo &e) {
        return engine.throwError(e.toString());
    }
    return JS_UNDEFINED;
}

Evaluator::Evaluator(ScriptEngine *scriptEngine)
    : m_scriptEngine(scriptEngine)
    , m_scriptClass(scriptEngine->registerClass("Evaluator", nullptr, nullptr, JS_UNDEFINED,
                                                getEvalPropertyNames, getEvalPropertySafe))
{
    scriptEngine->registerEvaluator(this);
    scriptEngine->logger().storeWarnings();
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
    return evaluateProperty(item, name, propertyWasSet);
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
    const JSValue v = evaluateProperty(item, name, propertyWasSet);
    if (JS_IsUndefined(v))
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
    const ScopedJsValue v(m_scriptEngine->context(), evaluateProperty(item, name, propertyWasSet));
    return JS_IsUndefined(v) ? defaultValue : getJsString(m_scriptEngine->context(), v);
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
    convertToPropertyType_impl(
        engine(), QString(), nullptr, decl, nullptr, loc, ConversionType::Full, v);
}

JSValue Evaluator::scriptValue(const Item *item)
{
    JSValue &scriptValue = m_scriptValueMap[item];
    if (JS_IsObject(scriptValue)) {
        // already initialized
        return scriptValue;
    }

    const auto edata = new EvaluationData;
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

JSValue Evaluator::evaluateProperty(const Item *item, const QString &name, bool *propertyWasSet)
{
    const JSValue result = property(item, name);
    ScopedJsValue valMgr(m_scriptEngine->context(), result);
    handleEvaluationError(item, name);
    valMgr.release();
    if (propertyWasSet)
        *propertyWasSet = isNonDefaultValue(item, name);
    return result;
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

static void convertToPropertyType_impl(
    ScriptEngine *engine,
    const QString &pathPropertiesBaseDir,
    const Item *item,
    const PropertyDeclaration &decl,
    const Value *value,
    const CodeLocation &location,
    ConversionType conversionType,
    JSValue &v)
{
    JSContext * const ctx = engine->context();
    if (JS_IsUninitialized(v) || JS_IsUndefined(v) || JS_IsError(ctx, v) || JS_IsException(v))
        return;

    if (!decl.isScalar() && !JS_IsArray(ctx, v) && conversionType == ConversionType::ElementsOnly) {
        const auto correspondingType = [](const PropertyDeclaration &decl) {
            switch (decl.type()) {
            case PropertyDeclaration::StringList:
                return PropertyDeclaration::String;
            case PropertyDeclaration::PathList:
                return PropertyDeclaration::Path;
            case PropertyDeclaration::VariantList:
                return PropertyDeclaration::Variant;
            default:
                QBS_CHECK(false);
            };
        };
        PropertyDeclaration elemDecl = decl;
        elemDecl.setType(correspondingType(decl));
        convertToPropertyType_impl(
            engine, pathPropertiesBaseDir, item, elemDecl, value, location, conversionType, v);
    }

    QString actualBaseDir;
    bool baseDirIsFromExport = false;
    if (decl.type() == PropertyDeclaration::Path || decl.type() == PropertyDeclaration::PathList) {
        actualBaseDir = pathPropertiesBaseDir;
        if (const Item * const baseDirItem = value && value->scope() ? value->scope() : item) {
            if (const VariantValueConstPtr v = baseDirItem->variantProperty(
                    StringConstants::qbsSourceDirPropertyInternal())) {
                actualBaseDir = v->value().toString();
                baseDirIsFromExport = true;
            }
            if (actualBaseDir.isEmpty() && baseDirItem->type() == ItemType::Product) {
                if (const VariantValueConstPtr itemSourceDir = baseDirItem->variantProperty(
                        StringConstants::sourceDirectoryProperty())) {
                    actualBaseDir = itemSourceDir->value().toString();
                }
            }
        }
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
        if (!JS_IsString(v)) {
            makeTypeError(engine, decl, location, v);
            break;
        }
        if (!actualBaseDir.isEmpty()) {
            const QString rawPath = getJsString(ctx, v);
            if (baseDirIsFromExport && !FileInfo::isAbsolute(rawPath)) {
                const JSValue error = handleDeprecatedPathResolving(*engine, *value);
                if (JS_IsError(engine->context(), error)) {
                    v = error;
                    return;
                }
            }
            v = engine->toScriptValue(
                QDir::cleanPath(FileInfo::resolvePath(actualBaseDir, rawPath)));
            JS_FreeValue(ctx, v);
        }
        break;
    case PropertyDeclaration::String:
        if (!JS_IsString(v))
            makeTypeError(engine, decl, location, v);
        break;
    case PropertyDeclaration::PathList:
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
            if (actualBaseDir.isEmpty())
                continue;
            const QString rawPath = getJsString(ctx, elem);
            if (baseDirIsFromExport && !FileInfo::isAbsolute(rawPath)) {
                const JSValue error = handleDeprecatedPathResolving(*engine, *value);
                if (JS_IsError(engine->context(), error)) {
                    v = error;
                    return;
                }
            }
            const JSValue newElem = engine->toScriptValue(
                QDir::cleanPath(FileInfo::resolvePath(actualBaseDir, rawPath)));
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

static void convertToPropertyType(
    ScriptEngine *engine,
    const Item *item,
    const PropertyDeclaration &decl,
    const Value *value,
    ConversionType conversionType,
    JSValue &v)
{
    if (value->type() == Value::VariantValueType && JS_IsUndefined(v) && !decl.isScalar()) {
        v = engine->newArray(0, JsValueOwner::ScriptEngine); // QTBUG-51237
        return;
    }
    convertToPropertyType_impl(
        engine,
        engine->evaluator()->pathPropertiesBaseDir(),
        item,
        decl,
        value,
        value->location(),
        conversionType,
        v);
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

class ValueEvaluator : ValueHandler<JSValue>
{
public:
    ValueEvaluator(
        Evaluator &evaluator,
        JSValue obj,
        const ValuePtr &v,
        const Item &item,
        const Item &itemOfProperty,
        const PropertyDeclaration &decl)
        : m_evaluator(evaluator)
        , m_engine(*evaluator.engine())
        , m_object(obj)
        , m_value(*v)
        , m_item(item)
        , m_itemOfProperty(itemOfProperty)
        , m_decl(decl)
    {
    }

    JSValue eval()
    {
        JSValue result = m_value.apply(this);
        if (JS_IsUninitialized(result))
            result = JS_UNDEFINED;
        convertToPropertyType(&m_engine, &m_item, m_decl, &m_value, ConversionType::Full, result);
        return result;
    }

private:
    class ScopeChain
    {
    public:
        ScopeChain(Evaluator &evaluator)
            : m_evaluator(evaluator)
        {}

        void pushScope(const JSValue &scope)
        {
            if (JS_IsObject(scope))
                m_chain << scope;
        }

        void pushScopeRecursively(const Item *scope)
        {
            if (scope) {
                pushScopeRecursively(scope->scope());
                pushScope(m_evaluator.scriptValue(scope));
            }
        }

        qbs::Internal::span<const JSValue> chain() const { return m_chain; }

    private:
        Evaluator &m_evaluator;
        QVarLengthArray<JSValue, 16> m_chain;
    };

    void setupConvenienceProperty(const QString &conveniencePropertyName, JSValue *extraScope,
                                  const JSValue &scriptValue)
    {
        if (!JS_IsObject(*extraScope))
            *extraScope = m_engine.newObject();
        const PropertyDeclaration::Type type = m_decl.type();
        const bool isArray = type == PropertyDeclaration::StringList
                || type == PropertyDeclaration::PathList
                || type == PropertyDeclaration::Variant // TODO: Why?
                || type == PropertyDeclaration::VariantList;
        JSValue valueToSet = JS_DupValue(m_engine.context(), scriptValue);
        if (isArray && (JS_IsUninitialized(valueToSet) || JS_IsUndefined(valueToSet)))
            valueToSet = m_engine.newArray(0, JsValueOwner::Caller);
        setJsProperty(m_engine.context(), *extraScope, conveniencePropertyName, valueToSet);
    }

    JSValue createExtraScope(const JSSourceValue *value, Item *outerItem, JSValue *outerScriptValue)
    {
        JSValue extraScope = JS_UNINITIALIZED;
        if (value->sourceUsesBase()) {
            JSValue baseValue = JS_UNINITIALIZED;
            if (value->baseValue()) {
                baseValue = ValueEvaluator(
                                m_evaluator,
                                m_object,
                                value->baseValue(),
                                m_item,
                                m_itemOfProperty,
                                m_decl)
                                .eval();
            }
            setupConvenienceProperty(StringConstants::baseVar(), &extraScope, baseValue);
        }
        if (value->sourceUsesOuter()) {
            JSValue v = JS_UNINITIALIZED;
            bool doSetup = false;
            if (outerItem) {
                v = m_evaluator.property(outerItem, m_decl.name());
                if (JsException ex = m_engine.checkAndClearException({}))
                    return m_engine.throwError(ex.toErrorInfo().toString());
                doSetup = true;
                JS_FreeValue(m_engine.context(), v);
            } else if (outerScriptValue) {
                doSetup = true;
                v = *outerScriptValue;
            }
            if (doSetup)
                setupConvenienceProperty(StringConstants::outerVar(), &extraScope, v);
        }
        if (value->sourceUsesOriginal()) {
            JSValue originalJs = JS_UNINITIALIZED;
            ScopedJsValue originalMgr(m_engine.context(), JS_UNDEFINED);
            if (m_decl.isScalar()) {
                const Item *item = &m_itemOfProperty;

                if (item->type() != ItemType::ModuleInstance
                    && item->type() != ItemType::ModuleInstancePlaceholder) {
                    const QString errorMessage = Tr::tr("The special value 'original' can only "
                                                        "be used with module properties.");
                    return throwError(m_engine.context(), errorMessage);
                }

                if (!value->scope()) {
                    const QString errorMessage = Tr::tr("The special value 'original' cannot "
                        "be used on the right-hand side of a property declaration.");
                    return throwError(m_engine.context(), errorMessage);
                }

                ValuePtr original;
                for (const ValuePtr &v : m_value.candidates()) {
                    if (!v->scope()) {
                        original = v;
                        break;
                    }
                }

                // This can happen when resolving shadow products. The error will be ignored
                // in that case.
                if (!original) {
                    const QString errorMessage = Tr::tr("Error setting up 'original'.");
                    return throwError(m_engine.context(), errorMessage);
                }
                originalJs
                    = ValueEvaluator(m_evaluator, m_object, original, m_item, *item, m_decl).eval();
            } else {
                originalJs = m_engine.newArray(0, JsValueOwner::Caller);
                originalMgr.setValue(originalJs);
            }
            setupConvenienceProperty(StringConstants::originalVar(), &extraScope, originalJs);
        }
        return extraScope;
    }

    JSValue mergeValues(const JSValueList &lst)
    {
        JSValue result = m_engine.newArray(int(lst.size()), JsValueOwner::ScriptEngine);
        quint32 k = 0;
        JSContext * const ctx = m_engine.context();
        for (const JSValue &v : std::as_const(lst)) {
            QBS_ASSERT(!JS_IsError(ctx, v), continue);
            if (JS_IsArray(ctx, v)) {
                const quint32 vlen = getJsIntProperty(ctx, v, StringConstants::lengthProperty());
                for (quint32 j = 0; j < vlen; ++j)
                    JS_SetPropertyUint32(ctx, result, k++, JS_GetPropertyUint32(ctx, v, j));
                JS_FreeValue(ctx, v);
            } else {
                JS_SetPropertyUint32(ctx, result, k++, v);
            }
        }
        setJsProperty(ctx, result, StringConstants::lengthProperty(), JS_NewInt32(ctx, k));
        return result;
    }

    JSValue handleAlternatives(JSSourceValue *value)
    {
        if (!m_decl.isScalar() && !value->createdByPropertiesBlock()
            && !value->alternatives().empty()) {
            const QString warning
                = Tr::tr("Using list properties as fallback values is deprecated.\n"
                         "In future versions of qbs, such properties will be considered "
                         "unconditionally.\n"
                         "If you want to keep the current semantics for this value, use a fallback "
                         "%1 item.")
                      .arg(BuiltinDeclarations::instance().nameForType(ItemType::Properties));
            try {
                m_engine.handleDeprecation(Version(2, 7), warning, value->location());
            } catch (const ErrorInfo &e) {
                return m_engine.throwError(e.toString());
            }
        }

        JSValue outerScriptValue = JS_UNDEFINED;
        JSValueList lst;
        for (const JSSourceValue::Alternative &alternative : value->alternatives()) {
            if (alternative.value->isFallback() && !lst.empty())
                break;
            if (alternative.value->sourceUsesOuter() && !m_item.outerItem()
                && JS_IsUndefined(outerScriptValue)) {
                outerScriptValue = evaluateJSSourceValue(value, nullptr);
                if (JS_IsError(m_engine.context(), outerScriptValue)) {
                    const ScopedJsValueList l(m_engine.context(), lst);
                    return outerScriptValue;
                }
            }
            const JSValue v = evaluateJSSourceValue(
                alternative.value.get(),
                m_item.outerItem(),
                &alternative,
                value,
                &outerScriptValue);
            if (JS_IsUninitialized(v))
                continue;
            if (m_decl.isScalar())
                return v;
            if (!JS_IsUndefined(v))
                lst << JS_DupValue(m_engine.context(), v);
        }

        return lst.empty() ? JS_UNINITIALIZED : mergeValues(lst);
    }

    JSValue mergeWithCandidates(const JSSourceValue *value, JSValue result)
    {
        if (value->candidates().empty())
            return result;

        JSValueList lst;
        if (!JS_IsUninitialized(result) && !JS_IsUndefined(result))
            lst << JS_DupValue(m_engine.context(), result);
        for (const ValuePtr &next : value->candidates()) {
            JSValue v = next->apply(this);
            if (JsException ex = m_engine.checkAndClearException({})) {
                const ScopedJsValueList l(m_engine.context(), lst);
                return m_engine.throwError(ex.toErrorInfo().toString());
            }
            if (JS_IsUninitialized(v) || JS_IsUndefined(v))
                continue;

            convertToPropertyType(
                &m_engine, &m_item, m_decl, next.get(), ConversionType::ElementsOnly, v);

            lst.push_back(JS_DupValue(m_engine.context(), v));
        }

        return lst.empty() ? result : mergeValues(lst);
    }

    JSValue doHandle(JSSourceValue *value) override
    {
        JSValue result = handleAlternatives(value);
        if (JS_IsUninitialized(result)) {
            result = evaluateJSSourceValue(value, m_item.outerItem());
            if (JS_IsError(m_engine.context(), result))
                return result;
        } else if (value->isExclusiveListValue()) {
            return result;
        }

        if (m_decl.isScalar()) {
            if (!JS_IsUninitialized(result) || !value->createdByPropertiesBlock())
                return result;
            for (const ValuePtr &candidate : value->candidates()) {
                const JSValue v = candidate->apply(this);
                if (!JS_IsUninitialized(v))
                    return v;
            }
            return JS_UNINITIALIZED;
        }

        return mergeWithCandidates(value, result);
    }

    JSValue evaluateJSSourceValue(
        const JSSourceValue *value,
        Item *outerItem,
        const JSSourceValue::Alternative *alternative = nullptr,
        JSSourceValue *elseCaseValue = nullptr,
        JSValue *outerScriptValue = nullptr)
    {
        QBS_ASSERT(!alternative || value == alternative->value.get(), return JS_UNINITIALIZED);

        for (Item *group = alternative ? alternative->value->scope() : nullptr;
             group && group->type() == ItemType::Group;
             group = group->parent()) {
            if (!m_evaluator.boolValue(group, StringConstants::conditionProperty()))
                return JS_UNINITIALIZED;
        }

        ScopeChain scopeChain(m_evaluator);
        const JSValue maybeExtraScope = createExtraScope(value, outerItem, outerScriptValue);
        if (JS_IsError(m_engine.context(), maybeExtraScope))
            return maybeExtraScope;
        const ScopedJsValue extraScopeMgr(m_engine.context(), maybeExtraScope);
        const Evaluator::FileContextScopes fileCtxScopes = m_evaluator.fileContextScopes(
            value->file());
        if (JsException ex = m_engine.checkAndClearException({}))
            return m_engine.throwError(ex.toErrorInfo().toString());
        scopeChain.pushScope(fileCtxScopes.fileScope);
        scopeChain.pushScopeRecursively(m_item.scope());
        if ((m_itemOfProperty.type() != ItemType::ModuleInstance
             && m_itemOfProperty.type() != ItemType::ModuleInstancePlaceholder)
            || !value->scope()) {
            scopeChain.pushScope(m_object);
        }
        scopeChain.pushScopeRecursively(value->scope());
        scopeChain.pushScope(maybeExtraScope);
        scopeChain.pushScope(fileCtxScopes.importScope);
        if (alternative) {
            ScopedJsValue sv(
                m_engine.context(),
                alternative->value->isFallback() ? JS_NewBool(m_engine.context(), 1)
                                                 : m_engine.evaluate(
                                                       JsValueOwner::Caller,
                                                       alternative->condition.value,
                                                       {},
                                                       1,
                                                       scopeChain.chain()));
            if (JsException ex = m_engine.checkAndClearException(alternative->condition.location)) {
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
                if (m_itemOfProperty.type() == ItemType::ModuleInstancePlaceholder)
                    return JS_UNDEFINED;

                return m_engine.throwError(ex.toErrorInfo().toString());
            }

            // The condition is false. Try the next alternative or the else value.
            if (!JS_ToBool(m_engine.context(), sv))
                return JS_UNINITIALIZED;

            sv.reset(m_engine.evaluate(
                JsValueOwner::Caller,
                alternative->overrideListProperties.value,
                {},
                1,
                scopeChain.chain()));
            if (JsException ex = m_engine.checkAndClearException(
                    alternative->overrideListProperties.location)) {
                return m_engine.throwError(ex.toErrorInfo().toString());
            }
            if (JS_ToBool(m_engine.context(), sv))
                elseCaseValue->setIsExclusiveListValue();
        }
        return m_engine.evaluate(
            JsValueOwner::ScriptEngine,
            value->sourceCodeForEvaluation(),
            value->file()->filePath(),
            value->line(),
            scopeChain.chain());
    }

    JSValue doHandle(ItemValue *value) override
    {
        const JSValue result = m_evaluator.scriptValue(value->item());
        if (JS_IsUninitialized(result))
            qDebug() << "SVConverter returned invalid script value.";
        return result;
    }

    JSValue doHandle(VariantValue *variantValue) override
    {
        const JSValue result = m_engine.toScriptValue(variantValue->value(), variantValue->id());
        m_engine.takeOwnership(result);
        return result;
    }

    Evaluator &m_evaluator;
    ScriptEngine &m_engine;
    const JSValue m_object;
    Value &m_value;
    const Item &m_item;
    const Item &m_itemOfProperty;
    const PropertyDeclaration m_decl;
};

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

struct EvalResult { JSValue v = JS_UNDEFINED; bool found = false; };
static EvalResult getEvalProperty(
    Evaluator &evaluator, JSValue obj, const Item *item, const QString &name, EvaluationData *data)
{
    ScriptEngine &engine = *evaluator.engine();
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
        PropertyStackManager propStackmanager(
            itemOfProperty,
            name,
            value.get(),
            evaluator.requestedProperties(),
            evaluator.propertyDependencies());
        if (evaluator.cachingEnabled()) {
            evaluator.clearCacheIfInvalidated(*data);
            const auto result = data->valueCache.constFind(name);
            if (result != data->valueCache.constEnd()) {
                if (debugProperties)
                    qDebug() << "[SC] cache hit " << name << ": "
                             << resultToString(engine.context(), *result);
                return {*result, true};
            }
        }

        const JSValue result = ValueEvaluator(
                                   evaluator,
                                   obj,
                                   value,
                                   *data->item,
                                   *itemOfProperty,
                                   data->item->propertyDeclaration(name))
                                   .eval();

        if (debugProperties)
            qDebug() << "[SC] cache miss " << name << ": "
                     << resultToString(engine.context(), result);
        if (evaluator.cachingEnabled()) {
            evaluator.clearCacheIfInvalidated(*data);
            const auto it = data->valueCache.find(name);
            if (it != data->valueCache.end()) {
                JS_FreeValue(engine.context(), it.value());
                it.value() = JS_DupValue(engine.context(), result);
            } else {
                data->valueCache.insert(name, JS_DupValue(engine.context(), result));
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
    ScriptEngine &engine = *ScriptEngine::engineForContext(ctx);
    Evaluator &evaluator = *engine.evaluator();
    const auto data = attachedPointer<EvaluationData>(obj, evaluator.classId());
    const QString name = getJsString(ctx, prop);
    if (debugProperties)
        qDebug() << "[SC] queryProperty " << jsObjectId(obj) << " " << name;

    if (name == QStringLiteral("parent")) {
        if (desc) {
            Item * const parent = data->item->parent();
            desc->value = parent ? JS_DupValue(ctx, evaluator.scriptValue(data->item->parent()))
                                 : JS_UNDEFINED;
        }
        return 1;
    }

    if (!data) {
        if (debugProperties)
            qDebug() << "[SC] queryProperty: no data attached";
        engine.setLastLookupStatus(false);
        return -1;
    }

    EvalResult result = getEvalProperty(evaluator, obj, data->item, name, data);
    if (!result.found && data->item->parent()) {
        if (debugProperties)
            qDebug() << "[SC] queryProperty: query parent";
        const Item * const parentItem = data->item->parent();
        result = getEvalProperty(
            evaluator, evaluator.scriptValue(parentItem), parentItem, name, data);
    }
    if (result.found) {
        if (desc)
            desc->value = JS_DupValue(ctx, result.v);
        engine.setLastLookupStatus(true);
        return 1;
    }

    if (debugProperties)
        qDebug() << "[SC] queryProperty: no such property";
    engine.setLastLookupStatus(false);
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
