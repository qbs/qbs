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

#include <QtScript/qscriptclass.h>
#include <QtScript/qscriptcontext.h>

namespace qbs {
namespace Internal {

enum BuildGraphScriptValueCommonPropertyKeys : quint32 {
    CachedValueKey,
    FileTagKey,
    ProductPtrKey,
};

class ArtifactsScriptClass : public QScriptClass
{
public:
    ArtifactsScriptClass(QScriptEngine *engine) : QScriptClass(engine) { }

private:
    QueryFlags queryProperty(const QScriptValue &object, const QScriptString &name,
                             QueryFlags flags, uint *id) override
    {
        getProduct(object);
        qbsEngine()->setNonExistingArtifactSetRequested(m_product, name.toString());
        return QScriptClass::queryProperty(object, name, flags, id);
    }

    QScriptClassPropertyIterator *newIterator(const QScriptValue &object) override
    {
        getProduct(object);
        qbsEngine()->setArtifactsEnumerated(m_product);
        return QScriptClass::newIterator(object);
    }

    void getProduct(const QScriptValue &object)
    {
        if (m_lastObjectId != object.objectId()) {
            m_lastObjectId = object.objectId();
            m_product = reinterpret_cast<const ResolvedProduct *>(
                        object.data().property(ProductPtrKey).toVariant().value<quintptr>());
        }
    }

    ScriptEngine *qbsEngine() const { return static_cast<ScriptEngine *>(engine()); }

    qint64 m_lastObjectId = 0;
    const ResolvedProduct *m_product = nullptr;
};

static bool isRelevantArtifact(const ResolvedProduct *, const Artifact *artifact)
{
    return !artifact->isTargetOfModule();
}
static bool isRelevantArtifact(const ResolvedModule *module, const Artifact *artifact)
{
    return artifact->targetOfModule == module->name;
}

static ArtifactSetByFileTag artifactsMap(const ResolvedProduct *product)
{
    return product->buildData->artifactsByFileTag();
}

static ArtifactSetByFileTag artifactsMap(const ResolvedModule *module)
{
    return artifactsMap(module->product);
}

static QScriptValue createArtifactsObject(const ResolvedProduct *product, ScriptEngine *engine)
{
    QScriptClass *scriptClass = engine->artifactsScriptClass();
    if (!scriptClass) {
        scriptClass = new ArtifactsScriptClass(engine);
        engine->setArtifactsScriptClass(scriptClass);
    }
    QScriptValue artifactsObj = engine->newObject(scriptClass);
    QScriptValue data = engine->newObject();
    QVariant v;
    v.setValue<quintptr>(reinterpret_cast<quintptr>(product));
    data.setProperty(ProductPtrKey, engine->newVariant(v));
    artifactsObj.setData(data);
    return artifactsObj;
}

static QScriptValue createArtifactsObject(const ResolvedModule *, ScriptEngine *engine)
{
    return engine->newObject();
}

static bool checkAndSetArtifactsMapUpToDateFlag(const ResolvedProduct *p)
{
    return p->buildData->checkAndSetJsArtifactsMapUpToDateFlag();
}
static bool checkAndSetArtifactsMapUpToDateFlag(const ResolvedModule *) { return true; }

static void registerArtifactsMapAccess(const ResolvedProduct *p, ScriptEngine *e, bool forceUpdate)
{
    e->setArtifactsMapRequested(p, forceUpdate);
}
static void registerArtifactsMapAccess(const ResolvedModule *, ScriptEngine *, bool) {}
static void registerArtifactsSetAccess(const ResolvedProduct *p, const FileTag &t, ScriptEngine *e)
{
    e->setArtifactSetRequestedForTag(p, t);
}
static void registerArtifactsSetAccess(const ResolvedModule *, const FileTag &, ScriptEngine *) {}

template<class ProductOrModule> static QScriptValue js_artifactsForFileTag(
        QScriptContext *ctx, ScriptEngine *engine, const ProductOrModule *productOrModule)
{
    const FileTag fileTag = FileTag(ctx->callee().property(FileTagKey).toString().toUtf8());
    registerArtifactsSetAccess(productOrModule, fileTag, engine);
    QScriptValue result = ctx->callee().property(CachedValueKey);
    if (result.isArray())
        return result;
    auto artifacts = artifactsMap(productOrModule).value(fileTag);
    const auto filter = [productOrModule](const Artifact *a) {
        return !isRelevantArtifact(productOrModule, a);
    };
    artifacts.erase(std::remove_if(artifacts.begin(), artifacts.end(), filter), artifacts.end());
    result = engine->newArray(uint(artifacts.size()));
    ctx->callee().setProperty(CachedValueKey, result);
    int k = 0;
    for (const Artifact * const artifact : artifacts)
        result.setProperty(k++, Transformer::translateFileConfig(engine, artifact, QString()));
    return result;
}

template<class ProductOrModule> static QScriptValue js_artifacts(
        QScriptContext *ctx, ScriptEngine *engine, const ProductOrModule *productOrModule)
{
    QScriptValue artifactsObj = ctx->callee().property(CachedValueKey);
    if (artifactsObj.isObject() && checkAndSetArtifactsMapUpToDateFlag(productOrModule)) {
        registerArtifactsMapAccess(productOrModule, engine, false);
        return artifactsObj;
    }
    registerArtifactsMapAccess(productOrModule, engine, true);
    artifactsObj = createArtifactsObject(productOrModule, engine);
    ctx->callee().setProperty(CachedValueKey, artifactsObj);
    const auto &map = artifactsMap(productOrModule);
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        const auto filter = [productOrModule](const Artifact *a) {
            return isRelevantArtifact(productOrModule, a);
        };
        if (std::none_of(it.value().cbegin(), it.value().cend(), filter))
            continue;
        QScriptValue fileTagFunc = engine->newFunction(&js_artifactsForFileTag<ProductOrModule>,
                                                       productOrModule);
        const QString fileTag = it.key().toString();
        fileTagFunc.setProperty(FileTagKey, fileTag);
        artifactsObj.setProperty(fileTag, fileTagFunc,
                                 QScriptValue::ReadOnly | QScriptValue::Undeletable
                                 | QScriptValue::PropertyGetter);
    }
    return artifactsObj;
}

QScriptValue artifactsScriptValueForProduct(QScriptContext *ctx, ScriptEngine *engine,
                                            const ResolvedProduct *product)
{
    return js_artifacts(ctx, engine, product);
}

QScriptValue artifactsScriptValueForModule(QScriptContext *ctx, ScriptEngine *engine,
                                           const ResolvedModule *module)
{
    return js_artifacts(ctx, engine, module);
}

} // namespace Internal
} // namespace qbs
