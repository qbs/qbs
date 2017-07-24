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
#include <language/language.h>
#include <language/propertymapinternal.h>
#include <language/qualifiedid.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/qttools.h>

#include <QtScript/qscriptclass.h>

namespace qbs {
namespace Internal {

enum ScriptValueCommonPropertyKeys : quint32
{
    ModuleNameKey,
    ProductPtrKey,
    ArtifactPtrKey
};

static QScriptValue getModuleProperty(const PropertyMapConstPtr &properties,
                                      const Artifact *artifact,
                                      ScriptEngine *engine, const QString &moduleName,
                                      const QString &propertyName)
{
    QVariant value;
    if (engine->isPropertyCacheEnabled())
        value = engine->retrieveFromPropertyCache(moduleName, propertyName, properties);
    if (!value.isValid()) {
        value = properties->moduleProperty(moduleName, propertyName);
        const Property p(moduleName, propertyName, value);
        if (artifact)
            engine->addPropertyRequestedFromArtifact(artifact, p);
        else
            engine->addPropertyRequestedInScript(p);

        // Cache the variant value. We must not cache the QScriptValue here, because it's a
        // reference and the user might change the actual object.
        if (engine->isPropertyCacheEnabled())
            engine->addToPropertyCache(moduleName, propertyName, properties, value);
    }
    return engine->toScriptValue(value);
}

class ModulePropertyScriptClass : public QScriptClass
{
public:
    ModulePropertyScriptClass(QScriptEngine *engine)
        : QScriptClass(engine)
    {
    }

    QueryFlags queryProperty(const QScriptValue &object, const QScriptString &name,
                             QueryFlags flags, uint *id) override
    {
        Q_UNUSED(object);
        Q_UNUSED(name);
        Q_UNUSED(flags);
        Q_UNUSED(id);
        return HandlesReadAccess;
    }

    QScriptValue property(const QScriptValue &object, const QScriptString &name, uint id) override
    {
        Q_UNUSED(id);
        if (m_lastObjectId != object.objectId()) {
            m_lastObjectId = object.objectId();
            const QScriptValue data = object.data();
            m_moduleName = data.property(ModuleNameKey).toString();
            m_product = reinterpret_cast<const ResolvedProduct *>(
                        data.property(ProductPtrKey).toVariant().value<quintptr>());
            m_artifact = reinterpret_cast<const Artifact *>(
                        data.property(ArtifactPtrKey).toVariant().value<quintptr>());
        }
        return getModuleProperty(m_artifact ? m_artifact->properties : m_product->moduleProperties,
                                 m_artifact, static_cast<ScriptEngine *>(engine()), m_moduleName,
                                 name);
    }

private:
    qint64 m_lastObjectId = 0;
    QString m_moduleName;
    const ResolvedProduct *m_product = nullptr;
    const Artifact *m_artifact = nullptr;
};

static QString ptrKey() { return QLatin1String("__internalPtr"); }
static QString typeKey() { return QLatin1String("__type"); }
static QString productType() { return QLatin1String("product"); }
static QString artifactType() { return QLatin1String("artifact"); }

void ModuleProperties::init(QScriptValue productObject,
                            const ResolvedProductConstPtr &product)
{
    init(productObject, product.get(), productType());
    setupModules(productObject, product, nullptr);
}

void ModuleProperties::init(QScriptValue artifactObject, const Artifact *artifact)
{
    init(artifactObject, artifact, artifactType());
    const auto product = artifact->product;
    const QVariantMap productProperties {
        {QStringLiteral("buildDirectory"), product->buildDirectory()},
        {QStringLiteral("destinationDirectory"), product->destinationDirectory},
        {QStringLiteral("name"), product->name},
        {QStringLiteral("sourceDirectory"), product->sourceDirectory},
        {QStringLiteral("targetName"), product->targetName},
        {QStringLiteral("type"), product->fileTags.toStringList()}
    };
    QScriptEngine * const engine = artifactObject.engine();
    artifactObject.setProperty(QStringLiteral("product"), engine->toScriptValue(productProperties));
    setupModules(artifactObject, artifact->product.lock(), artifact);
}

void ModuleProperties::init(QScriptValue objectWithProperties, const void *ptr,
                            const QString &type)
{
    QScriptEngine * const engine = objectWithProperties.engine();
    objectWithProperties.setProperty(QLatin1String("moduleProperty"),
                                     engine->newFunction(ModuleProperties::js_moduleProperty, 2));
    objectWithProperties.setProperty(ptrKey(), engine->toScriptValue(quintptr(ptr)));
    objectWithProperties.setProperty(typeKey(), type);
}

void ModuleProperties::setupModules(QScriptValue &object, const ResolvedProductConstPtr &product,
                                    const Artifact *artifact)
{
    ScriptEngine *engine = static_cast<ScriptEngine *>(object.engine());
    QScriptClass *modulePropertyScriptClass = engine->modulePropertyScriptClass();
    if (!modulePropertyScriptClass) {
        modulePropertyScriptClass = new ModulePropertyScriptClass(engine);
        engine->setModulePropertyScriptClass(modulePropertyScriptClass);
    }
    for (const auto &module : qAsConst(product->modules)) {
        QScriptValue data = engine->newObject();
        data.setProperty(ModuleNameKey, module->name);
        QVariant v;
        v.setValue<quintptr>(reinterpret_cast<quintptr>(product.get()));
        data.setProperty(ProductPtrKey, engine->newVariant(v));
        v.setValue<quintptr>(reinterpret_cast<quintptr>(artifact));
        data.setProperty(ArtifactPtrKey, engine->newVariant(v));
        QScriptValue moduleObject = engine->newObject(modulePropertyScriptClass);
        moduleObject.setData(data);
        const QualifiedId moduleName = QualifiedId::fromString(module->name);
        QScriptValue obj = object;
        for (int i = 0; i < moduleName.count() - 1; ++i) {
            QScriptValue tmp = obj.property(moduleName.at(i));
            if (!tmp.isObject())
                tmp = engine->newObject();
            obj.setProperty(moduleName.at(i), tmp);
            obj = tmp;
        }
        obj.setProperty(moduleName.last(), moduleObject);
        if (moduleName.count() > 1)
            object.setProperty(moduleName.toString(), moduleObject);
    }
}

QScriptValue ModuleProperties::js_moduleProperty(QScriptContext *context, QScriptEngine *engine)
{
    try {
        return moduleProperty(context, engine);
    } catch (const ErrorInfo &e) {
        return context->throwError(e.toString());
    }
}

QScriptValue ModuleProperties::moduleProperty(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() < 2)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("Function moduleProperty() expects 2 arguments"));
    }

    const QScriptValue objectWithProperties = context->thisObject();
    const QScriptValue typeScriptValue = objectWithProperties.property(typeKey());
    if (Q_UNLIKELY(!typeScriptValue.isString())) {
        return context->throwError(QScriptContext::TypeError,
                QLatin1String("Internal error: __type not set up"));
    }
    const QScriptValue ptrScriptValue = objectWithProperties.property(ptrKey());
    if (Q_UNLIKELY(!ptrScriptValue.isNumber())) {
        return context->throwError(QScriptContext::TypeError,
                QLatin1String("Internal error: __internalPtr not set up"));
    }

    const void *ptr = reinterpret_cast<const void *>(qscriptvalue_cast<quintptr>(ptrScriptValue));
    PropertyMapConstPtr properties;
    const Artifact *artifact = 0;
    if (typeScriptValue.toString() == productType()) {
        properties = static_cast<const ResolvedProduct *>(ptr)->moduleProperties;
    } else if (typeScriptValue.toString() == artifactType()) {
        artifact = static_cast<const Artifact *>(ptr);
        properties = artifact->properties;
    } else {
        return context->throwError(QScriptContext::TypeError,
                                   QLatin1String("Internal error: invalid type"));
    }

    ScriptEngine * const qbsEngine = static_cast<ScriptEngine *>(engine);
    const QString moduleName = context->argument(0).toString();
    const QString propertyName = context->argument(1).toString();
    return getModuleProperty(properties, artifact, qbsEngine, moduleName, propertyName);
}

} // namespace Internal
} // namespace qbs
