/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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
#include "artifactsscriptvalue.h"

#include "artifact.h"
#include "productbuilddata.h"
#include "transformer.h"

#include <language/language.h>
#include <language/scriptengine.h>

#include <tools/stlutils.h>
#include <tools/stringconstants.h>

namespace qbs {
namespace Internal {

template<class ProductOrModule>
static bool isRelevantArtifact(const ProductOrModule *productOrModule, const Artifact *artifact)
{
    if constexpr (std::is_same_v<ProductOrModule, ResolvedProduct>) {
        Q_UNUSED(productOrModule)
        return !artifact->isTargetOfModule();
    } else {
        return artifact->targetOfModule == productOrModule->name;
    }
}

template<class ProductOrModule>
static ArtifactSetByFileTag artifactsMap(const ProductOrModule *productOrModule)
{
    if constexpr (std::is_same_v<ProductOrModule, ResolvedProduct>)
        return productOrModule->buildData->artifactsByFileTag();
    else
        return artifactsMap(productOrModule->product);
}

template<class ProductOrModule> static int scriptClassIndex()
{
    if constexpr (std::is_same_v<ProductOrModule, ResolvedProduct>)
        return 0;
    return 1;
}

template<class ProductOrModule>
std::unique_lock<std::mutex> getArtifactsMapLock(ProductOrModule *productOrModule)
{
    if constexpr (std::is_same_v<ProductOrModule, ResolvedProduct>)
        return productOrModule->buildData->getArtifactsMapLock();
    else
        return getArtifactsMapLock(productOrModule->product);
}

template<class ProductOrModule>
static bool checkAndSetArtifactsMapUpToDateFlag(const ProductOrModule *productOrModule)
{
    if constexpr (std::is_same_v<ProductOrModule, ResolvedProduct>)
        return productOrModule->buildData->checkAndSetJsArtifactsMapUpToDateFlag();
    else
        return checkAndSetArtifactsMapUpToDateFlag(productOrModule->product);
}

// Must be called with artifacts map lock held!
template<class ProductOrModule>
void registerArtifactsMapAccess(ScriptEngine *engine, ProductOrModule *productOrModule)
{
    if constexpr (std::is_same_v<ProductOrModule, ResolvedProduct>) {
        if (!checkAndSetArtifactsMapUpToDateFlag(productOrModule))
            engine->setArtifactsMapRequested(productOrModule, true);
        else
            engine->setArtifactsMapRequested(productOrModule, false);
    } else {
        registerArtifactsMapAccess(engine, productOrModule->product);
    }
}

template<class ProductOrModule>
static int getArtifactsPropertyNames(JSContext *ctx, JSPropertyEnum **ptab, uint32_t *plen,
                                     JSValueConst obj)
{
    ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
    const auto productOrModule = attachedPointer<ProductOrModule>(
                obj, engine->artifactsScriptClass(scriptClassIndex<ProductOrModule>()));
    const std::unique_lock lock = getArtifactsMapLock(productOrModule);
    registerArtifactsMapAccess(engine, productOrModule);
    if constexpr (std::is_same_v<ProductOrModule, ResolvedProduct>)
        engine->setArtifactsEnumerated(productOrModule);
    const auto &map = artifactsMap(productOrModule);
    const auto filter = [productOrModule](const Artifact *a) {
        return isRelevantArtifact(productOrModule, a);
    };
    QStringList tags;
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        if (any_of(it.value(), filter)) {
            tags << it.key().toString();
        }
    }
    *plen = tags.size();
    if (!tags.isEmpty()) {
        *ptab = reinterpret_cast<JSPropertyEnum *>(js_malloc(ctx, *plen * sizeof **ptab));
        JSPropertyEnum *entry = *ptab;
        for (const QString &tag : std::as_const(tags)) {
            entry->atom = JS_NewAtom(ctx, tag.toUtf8().constData());
            entry->is_enumerable = 1;
            ++entry;
        }
    } else {
        *ptab = nullptr;
    }
    return 0;
}

template<class ProductOrModule>
static int getArtifactsProperty(JSContext *ctx, JSPropertyDescriptor *desc,
                                JSValueConst obj, JSAtom prop)
{
    if (!desc)
        return 1;

    desc->flags = JS_PROP_ENUMERABLE;
    desc->value = desc->getter = desc->setter = JS_UNDEFINED;
    ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
    const auto productOrModule = attachedPointer<ProductOrModule>(
                obj, engine->artifactsScriptClass(scriptClassIndex<ProductOrModule>()));
    const std::unique_lock lock = getArtifactsMapLock(productOrModule);
    registerArtifactsMapAccess(engine, productOrModule);
    const QString tagString = getJsString(ctx, prop);
    const FileTag fileTag(tagString.toUtf8());
    const auto &map = artifactsMap(productOrModule);
    const auto it = map.constFind(fileTag);
    if (it == map.constEnd()) {
        if constexpr (std::is_same_v<ProductOrModule, ResolvedProduct>)
            engine->setNonExistingArtifactSetRequested(productOrModule, tagString);
        return 1;
    }
    if constexpr (std::is_same_v<ProductOrModule, ResolvedProduct>)
        engine->setArtifactSetRequestedForTag(productOrModule, fileTag);
    ArtifactSet artifacts = it.value();
    removeIf(artifacts, [productOrModule](const Artifact *a) {
        return !isRelevantArtifact(productOrModule, a);
    });
    if (!artifacts.empty()) {
        desc->value = JS_NewArray(ctx); // TODO: Also cache this list?
        int k = 0;
        for (Artifact * const artifact : artifacts) {
            JS_SetPropertyUint32(ctx, desc->value, k++,
                                 Transformer::translateFileConfig(engine, artifact, QString()));
        }
    }
    return 1;
}

template<class ProductOrModule> static JSValue js_artifacts(JSContext *ctx,
                                                            JSValue jsProductOrModule)
{
    ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
    const auto productOrModule = attachedPointer<ProductOrModule>(jsProductOrModule,
                                                                  engine->dataWithPtrClass());
    JSValue artifactsObj = engine->artifactsMapScriptValue(productOrModule);
    if (!JS_IsUndefined(artifactsObj))
        return artifactsObj;
    const int classIndex = scriptClassIndex<ProductOrModule>();
    JSClassID scriptClass = engine->artifactsScriptClass(classIndex);
    if (scriptClass == 0) {
        const QByteArray className = "ArtifactsScriptClass" + QByteArray::number(classIndex);
        scriptClass = engine->registerClass(className.constData(), nullptr, nullptr, JS_UNDEFINED,
                                            &getArtifactsPropertyNames<ProductOrModule>,
                                            &getArtifactsProperty<ProductOrModule>);
        engine->setArtifactsScriptClass(classIndex, scriptClass);
    }
    artifactsObj = JS_NewObjectClass(engine->context(), scriptClass);
    attachPointerTo(artifactsObj, productOrModule);
    return artifactsObj;
}

JSValue artifactsScriptValueForProduct(JSContext *ctx, JSValue this_val, int, JSValue *)
{
    return js_artifacts<ResolvedProduct>(ctx, this_val);
}

JSValue artifactsScriptValueForModule(JSContext *ctx, JSValueConst this_val, int, JSValueConst *)
{
    return js_artifacts<ResolvedModule>(ctx, this_val);
}

} // namespace Internal
} // namespace qbs
