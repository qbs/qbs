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
#include <buildgraph/dependencyparametersscriptvalue.h>
#include <buildgraph/scriptclasspropertyiterator.h>
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

#include <QtScript/qscriptclass.h>

namespace qbs {
namespace Internal {

QScriptValue getDataForModuleScriptValue(QScriptEngine *engine, const ResolvedProduct *product,
                                         const Artifact *artifact, const ResolvedModule *module)
{
    QScriptValue data = engine->newObject();
    data.setProperty(ModuleNameKey, module->name);
    QVariant v;
    v.setValue<quintptr>(reinterpret_cast<quintptr>(product));
    data.setProperty(ProductPtrKey, engine->newVariant(v));
    v.setValue<quintptr>(reinterpret_cast<quintptr>(artifact));
    data.setProperty(ArtifactPtrKey, engine->newVariant(v));
    return data;
}

static QScriptValue getModuleProperty(const ResolvedProduct *product, const Artifact *artifact,
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

        // Cache the variant value. We must not cache the QScriptValue here, because it's a
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

class ModulePropertyScriptClass : public QScriptClass
{
public:
    ModulePropertyScriptClass(QScriptEngine *engine)
        : QScriptClass(engine)
    {
    }

private:
    QueryFlags queryProperty(const QScriptValue &object, const QScriptString &name,
                             QueryFlags flags, uint *id) override
    {
        Q_UNUSED(flags);
        Q_UNUSED(id);

        if (name == StringConstants::dependenciesProperty()
                || name == StringConstants::artifactsProperty()) {
            // The prototype is not backed by a QScriptClass.
            m_result = object.prototype().property(name);
            return HandlesReadAccess;
        }

        if (name == StringConstants::parametersProperty()) {
            m_result = object.data().property(DependencyParametersKey);
            return HandlesReadAccess;
        }

        setup(object);
        QBS_ASSERT(m_product, return {});
        bool isPresent;
        m_result = getModuleProperty(m_product, m_artifact, static_cast<ScriptEngine *>(engine()),
                                     m_moduleName, name, &isPresent);

        // It is important that we reject unknown property names. Otherwise QtScript will forward
        // *everything* to us, including built-in stuff like the hasOwnProperty function.
        return isPresent ? HandlesReadAccess : QueryFlags();
    }

    QScriptValue property(const QScriptValue &, const QScriptString &, uint) override
    {
        return m_result;
    }

    QScriptClassPropertyIterator *newIterator(const QScriptValue &object) override
    {
        setup(object);
        QBS_ASSERT(m_artifact || m_product, return nullptr);
        const PropertyMapInternal *propertyMap;
        std::vector<QString> additionalProperties({StringConstants::artifactsProperty(),
                                                   StringConstants::dependenciesProperty()});
        if (m_artifact) {
            propertyMap = m_artifact->properties.get();
        } else {
            propertyMap = m_product->moduleProperties.get();
            if (object.data().property(DependencyParametersKey).isValid())
                additionalProperties.push_back(StringConstants::parametersProperty());
        }
        return new ScriptClassPropertyIterator(object,
                                               propertyMap->value().value(m_moduleName).toMap(),
                                               additionalProperties);
    }

    void setup(const QScriptValue &object)
    {
        if (m_lastObjectId != object.objectId()) {
            m_lastObjectId = object.objectId();
            const QScriptValue data = object.data();
            QBS_ASSERT(data.isValid(), return);
            m_moduleName = data.property(ModuleNameKey).toString();
            m_product = reinterpret_cast<const ResolvedProduct *>(
                        data.property(ProductPtrKey).toVariant().value<quintptr>());
            m_artifact = reinterpret_cast<const Artifact *>(
                        data.property(ArtifactPtrKey).toVariant().value<quintptr>());
        }
    }

    qint64 m_lastObjectId = 0;
    QString m_moduleName;
    const ResolvedProduct *m_product = nullptr;
    const Artifact *m_artifact = nullptr;
    QScriptValue m_result;
};

static QString ptrKey() { return QStringLiteral("__internalPtr"); }
static QString typeKey() { return QStringLiteral("__type"); }
static QString artifactType() { return QStringLiteral("artifact"); }

static QScriptValue js_moduleDependencies(QScriptContext *, ScriptEngine *engine,
                                          const ResolvedModule *module)
{
    QScriptValue result = engine->newArray();
    quint32 idx = 0;
    for (const QString &depName : qAsConst(module->moduleDependencies)) {
        for (const auto &dep : module->product->modules) {
            if (dep->name != depName)
                continue;
            QScriptValue obj = engine->newObject(engine->modulePropertyScriptClass());
            obj.setPrototype(engine->moduleScriptValuePrototype(dep.get()));
            QScriptValue data = getDataForModuleScriptValue(engine, module->product, nullptr,
                                                            dep.get());
            const QVariantMap &params = module->product->moduleParameters.value(dep);
            data.setProperty(DependencyParametersKey, dependencyParametersValue(
                                 module->product->uniqueName(), dep->name, params, engine));
            obj.setData(data);
            result.setProperty(idx++, obj);
            break;
        }
    }
    QBS_ASSERT(idx == quint32(module->moduleDependencies.size()),;);
    return result;
}

static QScriptValue setupModuleScriptValue(ScriptEngine *engine,
                                           const ResolvedModule *module)
{
    QScriptValue &moduleScriptValue = engine->moduleScriptValuePrototype(module);
    if (moduleScriptValue.isValid())
        return moduleScriptValue;
    moduleScriptValue = engine->newObject();
    QScriptValue depfunc = engine->newFunction<const ResolvedModule *>(&js_moduleDependencies,
                                                                       module);
    moduleScriptValue.setProperty(StringConstants::dependenciesProperty(), depfunc,
                                  QScriptValue::ReadOnly | QScriptValue::Undeletable
                                  | QScriptValue::PropertyGetter);
    QScriptValue artifactsFunc = engine->newFunction(&artifactsScriptValueForModule, module);
    moduleScriptValue.setProperty(StringConstants::artifactsProperty(), artifactsFunc,
                                   QScriptValue::ReadOnly | QScriptValue::Undeletable
                                   | QScriptValue::PropertyGetter);
    return moduleScriptValue;
}

void ModuleProperties::init(QScriptValue productObject,
                            const ResolvedProduct *product)
{
    init(productObject, product, StringConstants::productValue());
    setupModules(productObject, product, nullptr);
}

void ModuleProperties::init(QScriptValue artifactObject, const Artifact *artifact)
{
    init(artifactObject, artifact, artifactType());
    const auto product = artifact->product;
    const QVariantMap productProperties {
        {StringConstants::buildDirectoryProperty(), product->buildDirectory()},
        {StringConstants::destinationDirProperty(), product->destinationDirectory},
        {StringConstants::nameProperty(), product->name},
        {StringConstants::sourceDirectoryProperty(), product->sourceDirectory},
        {StringConstants::targetNameProperty(), product->targetName},
        {StringConstants::typeProperty(), sorted(product->fileTags.toStringList())}
    };
    QScriptEngine * const engine = artifactObject.engine();
    artifactObject.setProperty(StringConstants::productVar(),
                               engine->toScriptValue(productProperties));
    setupModules(artifactObject, artifact->product.get(), artifact);
}

void ModuleProperties::setModuleScriptValue(QScriptValue targetObject,
        const QScriptValue &moduleObject, const QString &moduleName)
{
    auto const e = static_cast<ScriptEngine *>(targetObject.engine());
    const QualifiedId name = QualifiedId::fromString(moduleName);
    QScriptValue obj = targetObject;
    for (int i = 0; i < name.size() - 1; ++i) {
        QScriptValue tmp = obj.property(name.at(i));
        if (!tmp.isObject())
            tmp = e->newObject();
        obj.setProperty(name.at(i), tmp);
        obj = tmp;
    }
    obj.setProperty(name.last(), moduleObject);
    if (moduleName.size() > 1)
        targetObject.setProperty(moduleName, moduleObject);
}

void ModuleProperties::init(QScriptValue objectWithProperties, const void *ptr,
                            const QString &type)
{
    QScriptEngine * const engine = objectWithProperties.engine();
    objectWithProperties.setProperty(QStringLiteral("moduleProperty"),
                                     engine->newFunction(ModuleProperties::js_moduleProperty, 2));
    objectWithProperties.setProperty(ptrKey(), engine->toScriptValue(quintptr(ptr)));
    objectWithProperties.setProperty(typeKey(), type);
}

void ModuleProperties::setupModules(QScriptValue &object, const ResolvedProduct *product,
                                    const Artifact *artifact)
{
    const auto engine = static_cast<ScriptEngine *>(object.engine());
    QScriptClass *modulePropertyScriptClass = engine->modulePropertyScriptClass();
    if (!modulePropertyScriptClass) {
        modulePropertyScriptClass = new ModulePropertyScriptClass(engine);
        engine->setModulePropertyScriptClass(modulePropertyScriptClass);
    }
    for (const auto &module : product->modules) {
        QScriptValue moduleObjectPrototype = setupModuleScriptValue(engine, module.get());
        QScriptValue moduleObject = engine->newObject(modulePropertyScriptClass);
        moduleObject.setPrototype(moduleObjectPrototype);
        moduleObject.setData(getDataForModuleScriptValue(engine, product, artifact, module.get()));
        setModuleScriptValue(object, moduleObject, module->name);
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
                QStringLiteral("Internal error: __type not set up"));
    }
    const QScriptValue ptrScriptValue = objectWithProperties.property(ptrKey());
    if (Q_UNLIKELY(!ptrScriptValue.isNumber())) {
        return context->throwError(QScriptContext::TypeError,
                QStringLiteral("Internal error: __internalPtr not set up"));
    }

    const void *ptr = reinterpret_cast<const void *>(qscriptvalue_cast<quintptr>(ptrScriptValue));
    const ResolvedProduct *product = nullptr;
    const Artifact *artifact = nullptr;
    if (typeScriptValue.toString() == StringConstants::productValue()) {
        QBS_ASSERT(ptr, return {});
        product = static_cast<const ResolvedProduct *>(ptr);
    } else if (typeScriptValue.toString() == artifactType()) {
        QBS_ASSERT(ptr, return {});
        artifact = static_cast<const Artifact *>(ptr);
        product = artifact->product.get();
    } else {
        return context->throwError(QScriptContext::TypeError,
                                   QStringLiteral("Internal error: invalid type"));
    }

    const auto qbsEngine = static_cast<ScriptEngine *>(engine);
    const QString moduleName = context->argument(0).toString();
    const QString propertyName = context->argument(1).toString();
    return getModuleProperty(product, artifact, qbsEngine, moduleName, propertyName);
}

} // namespace Internal
} // namespace qbs
