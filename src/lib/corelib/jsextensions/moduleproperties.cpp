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

#include "moduleproperties.h"

#include <buildgraph/artifact.h>
#include <buildgraph/artifactsscriptvalue.h>
#include <buildgraph/buildgraph.h>
#include <buildgraph/dependencyparametersscriptvalue.h>
#include <language/language.h>
#include <language/propertymapinternal.h>
#include <language/qualifiedid.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/stlutils.h>
#include <tools/stringconstants.h>
#include <utility>

namespace qbs {
namespace Internal {

JSValue createDataForModuleScriptValue(ScriptEngine *engine, const Artifact *artifact)
{
    JSValue data = JS_NewObjectClass(engine->context(), engine->dataWithPtrClass());
    attachPointerTo(data, artifact);
    return data;
}

static JSValue getModuleProperty(const ResolvedProduct *product, const Artifact *artifact,
                                 ScriptEngine *engine, const QString &moduleName,
                                 const QString &propertyName, bool *isPresent = nullptr)
{
    const PropertyMapConstPtr &properties = artifact ? artifact->properties
                                                     : product->moduleProperties;
    QVariant value;
    if (engine->isPropertyCacheEnabled())
        value = engine->retrieveFromPropertyCache(moduleName, propertyName, properties);
    if (!value.isValid()) {
        value = properties->moduleProperty(moduleName, propertyName, isPresent);

        // Cache the variant value. We must not cache the script value here, because it's a
        // reference and the user might change the actual object.
        if (engine->isPropertyCacheEnabled())
            engine->addToPropertyCache(moduleName, propertyName, properties, value);
    } else if (isPresent) {
        *isPresent = true;
    }

    const Property p(product->uniqueName(), moduleName, propertyName, value,
                     Property::PropertyInModule);
    if (artifact)
        engine->addPropertyRequestedFromArtifact(artifact, p);
    else
        engine->addPropertyRequestedInScript(p);

    return engine->toScriptValue(value);
}

struct ModuleData {
    JSValue dependencyParameters = JS_UNDEFINED;
    const Artifact *artifact = nullptr;
};

ModuleData getModuleData(JSContext *ctx, JSValue obj)
{
    const ScopedJsValue jsData(ctx, getJsProperty(ctx, obj,
                                                  StringConstants::dataPropertyInternal()));
    ModuleData data;
    data.dependencyParameters = JS_GetPropertyUint32(ctx, jsData, DependencyParametersKey);
    data.artifact = attachedPointer<Artifact>(
                jsData, ScriptEngine::engineForContext(ctx)->dataWithPtrClass());
    return data;
}

static int getModulePropertyNames(JSContext *ctx, JSPropertyEnum **ptab, uint32_t *plen,
                                  JSValueConst obj)
{
    const auto engine = ScriptEngine::engineForContext(ctx);
    const ModuleData data = getModuleData(ctx, obj);
    const auto module = attachedPointer<ResolvedModule>(obj, engine->modulePropertyScriptClass());
    QBS_ASSERT(module, return -1);

    const PropertyMapInternal *propertyMap;
    QStringList additionalProperties;
    if (data.artifact) {
        propertyMap = data.artifact->properties.get();
    } else {
        propertyMap = module->product->moduleProperties.get();
        if (JS_IsObject(data.dependencyParameters))
            additionalProperties.push_back(StringConstants::parametersProperty());
    }
    JS_FreeValue(ctx, data.dependencyParameters);
    getPropertyNames(ctx, ptab, plen, propertyMap->value().value(module->name).toMap(),
                     additionalProperties, engine->baseModuleScriptValue(module));
    return 0;
}

static int getModuleProperty(JSContext *ctx, JSPropertyDescriptor *desc, JSValueConst obj,
                             JSAtom prop)
{
    if (desc) {
        desc->getter = desc->setter = desc->value = JS_UNDEFINED;
        desc->flags = JS_PROP_ENUMERABLE;
    }
    const auto engine = ScriptEngine::engineForContext(ctx);
    const QString name = getJsString(ctx, prop);
    const ModuleData data = getModuleData(ctx, obj);
    const auto module = attachedPointer<ResolvedModule>(obj, engine->modulePropertyScriptClass());
    QBS_ASSERT(module, return -1);

    ScopedJsValue parametersMgr(ctx, data.dependencyParameters);
    if (name == StringConstants::parametersProperty()) {
        if (desc)
            desc->value = parametersMgr.release();
        return 1;
    }

    bool isPresent;
    JSValue value = getModuleProperty(
                module->product, data.artifact, ScriptEngine::engineForContext(ctx),
                module->name, name, &isPresent);
    if (isPresent) {
        if (desc)
            desc->value = value;
        return 1;
    }

    ScopedJsValue v(ctx, JS_GetProperty(ctx, engine->baseModuleScriptValue(module), prop));
    const int ret = JS_IsUndefined(v) ? 0 : 1;
    if (desc)
        desc->value = v.release();
    return ret;
}

static JSValue js_moduleDependencies(JSContext *ctx, JSValueConst this_val, int , JSValueConst *)
{
    ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
    const auto module = attachedPointer<ResolvedModule>(this_val, engine->dataWithPtrClass());
    JSValue result = JS_NewArray(engine->context());
    quint32 idx = 0;
    for (const QString &depName : std::as_const(module->moduleDependencies)) {
        for (const auto &dep : module->product->modules) {
            if (dep->name != depName)
                continue;
            JSValue obj = JS_NewObjectClass(ctx, engine->modulePropertyScriptClass());
            attachPointerTo(obj, dep.get());
            JSValue data = createDataForModuleScriptValue(engine, nullptr);
            const QVariantMap &params = module->product->moduleParameters.value(dep);
            JS_SetPropertyUint32(ctx, data, DependencyParametersKey,
                                 dependencyParametersValue(module->product->uniqueName(),
                                                           dep->name, params, engine));
            defineJsProperty(ctx, obj, StringConstants::dataPropertyInternal(), data);
            JS_SetPropertyUint32(ctx, result, idx++, obj);
            break;
        }
    }
    QBS_ASSERT(idx == quint32(module->moduleDependencies.size()),;);
    return result;
}

template<class ProductOrArtifact>
static JSValue moduleProperty(JSContext *ctx, JSValue this_val, int argc, JSValue *argv)
{
    if (Q_UNLIKELY(argc < 2))
        return throwError(ctx, Tr::tr("Function moduleProperty() expects 2 arguments"));

    ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
    const ResolvedProduct *product = nullptr;
    const Artifact *artifact = nullptr;
    if constexpr (std::is_same_v<ProductOrArtifact, ResolvedProduct>) {
        product = attachedPointer<ResolvedProduct>(this_val, engine->productPropertyScriptClass());
    } else {
        artifact = attachedPointer<Artifact>(this_val, engine->dataWithPtrClass());
        product = artifact->product;
    }

    const QString moduleName = getJsString(ctx, argv[0]);
    const QString propertyName = getJsString(ctx, argv[1]);
    return getModuleProperty(product, artifact, engine, moduleName, propertyName);
}


template<class ProductOrArtifact>
static JSValue js_moduleProperty(JSContext *ctx, JSValue this_val, int argc, JSValue *argv)
{
    try {
        return moduleProperty<ProductOrArtifact>(ctx, this_val, argc, argv);
    } catch (const ErrorInfo &e) {
        return throwError(ctx, e.toString());
    }
}

template<class ProductOrArtifact>
static void initModuleProperties(ScriptEngine *engine, JSValue objectWithProperties)
{
    JSContext * const ctx = engine->context();
    JSValue func = JS_NewCFunction(ctx, js_moduleProperty<ProductOrArtifact>, "moduleProperty", 2);
    setJsProperty(ctx, objectWithProperties, QStringLiteral("moduleProperty"), func);
}

static JSValue setupBaseModuleScriptValue(ScriptEngine *engine, const ResolvedModule *module)
{
    JSValue &moduleScriptValue = engine->baseModuleScriptValue(module);
    if (JS_IsObject(moduleScriptValue))
        return moduleScriptValue;
    const ScopedJsValue proto(engine->context(), JS_NewObject(engine->context()));
    moduleScriptValue = JS_NewObjectProtoClass(engine->context(), proto,
                                               engine->dataWithPtrClass());
    attachPointerTo(moduleScriptValue, module);
    QByteArray name = StringConstants::dependenciesProperty().toUtf8();
    const ScopedJsValue depfunc(
                engine->context(),
                JS_NewCFunction(engine->context(), js_moduleDependencies, name.constData(), 0));
    const ScopedJsAtom depAtom(engine->context(), name);
    JS_DefineProperty(engine->context(), moduleScriptValue, depAtom,
                      JS_UNDEFINED, depfunc, JS_UNDEFINED, JS_PROP_HAS_GET | JS_PROP_ENUMERABLE);
    name = StringConstants::artifactsProperty().toUtf8();
    const ScopedJsValue artifactsFunc(
                engine->context(),
                JS_NewCFunction(engine->context(), &artifactsScriptValueForModule,
                                name.constData(), 0));
    const ScopedJsAtom artifactsAtom(engine->context(), name);
    JS_DefineProperty(engine->context(), moduleScriptValue, artifactsAtom,
                      JS_UNDEFINED, artifactsFunc, JS_UNDEFINED,
                      JS_PROP_HAS_GET | JS_PROP_ENUMERABLE);
    return moduleScriptValue;
}

void ModuleProperties::init(ScriptEngine *engine, JSValue productObject,
                            const ResolvedProduct *product)
{
    initModuleProperties<ResolvedProduct>(engine, productObject);
    setupModules(engine, productObject, product, nullptr);
}

void ModuleProperties::init(ScriptEngine *engine, JSValue artifactObject, const Artifact *artifact)
{
    initModuleProperties<Artifact>(engine, artifactObject);
    const auto product = artifact->product;
    const QVariantMap productProperties {
        {StringConstants::buildDirectoryProperty(), product->buildDirectory()},
        {StringConstants::destinationDirProperty(), product->destinationDirectory},
        {StringConstants::nameProperty(), product->name},
        {StringConstants::sourceDirectoryProperty(), product->sourceDirectory},
        {StringConstants::targetNameProperty(), product->targetName},
        {StringConstants::typeProperty(), sorted(product->fileTags.toStringList())}
    };
    setJsProperty(engine->context(), artifactObject, StringConstants::productVar(),
                  engine->toScriptValue(productProperties));
    setupModules(engine, artifactObject, artifact->product.get(), artifact);
}

void ModuleProperties::setModuleScriptValue(ScriptEngine *engine, JSValue targetObject,
                                            const JSValue &moduleObject, const QString &moduleName)
{
    const QualifiedId name = QualifiedId::fromString(moduleName);
    JSValue obj = targetObject;
    for (int i = 0; i < name.size() - 1; ++i) {
        JSValue tmp = getJsProperty(engine->context(), obj, name.at(i));
        if (!JS_IsObject(tmp)) {
            tmp = engine->newObject();
            setJsProperty(engine->context(), obj, name.at(i), tmp);
        } else {
            JS_FreeValue(engine->context(), tmp);
        }
        obj = tmp;
    }
    setJsProperty(engine->context(), obj, name.last(), moduleObject);
    if (name.size() > 1) {
        setJsProperty(engine->context(), targetObject, moduleName,
                      JS_DupValue(engine->context(), moduleObject));
    }
}

void ModuleProperties::setupModules(ScriptEngine *engine, JSValue &object,
                                    const ResolvedProduct *product, const Artifact *artifact)
{
    JSClassID modulePropertyScriptClass = engine->modulePropertyScriptClass();
    if (modulePropertyScriptClass == 0) {
        modulePropertyScriptClass = engine->registerClass("ModulePropertyScriptClass", nullptr,
                nullptr, JS_UNDEFINED, &getModulePropertyNames, &getModuleProperty);
        engine->setModulePropertyScriptClass(modulePropertyScriptClass);
    }
    for (const auto &module : product->modules) {
        setupBaseModuleScriptValue(engine, module.get());
        JSValue moduleObject = JS_NewObjectClass(engine->context(), modulePropertyScriptClass);
        attachPointerTo(moduleObject, module.get());
        defineJsProperty(engine->context(), moduleObject, StringConstants::dataPropertyInternal(),
                         createDataForModuleScriptValue(engine, artifact));
        setModuleScriptValue(engine, object, moduleObject, module->name);
    }
}

} // namespace Internal
} // namespace qbs
