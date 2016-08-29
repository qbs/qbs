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
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/propertyfinder.h>

#include <QScriptEngine>

namespace qbs {
namespace Internal {

static QString ptrKey() { return QLatin1String("__internalPtr"); }
static QString typeKey() { return QLatin1String("__type"); }
static QString productType() { return QLatin1String("product"); }
static QString artifactType() { return QLatin1String("artifact"); }

void ModuleProperties::init(QScriptValue productObject,
                            const ResolvedProductConstPtr &product)
{
    init(productObject, product.data(), productType());
}

void ModuleProperties::init(QScriptValue artifactObject, const Artifact *artifact)
{
    init(artifactObject, artifact, artifactType());
}

void ModuleProperties::init(QScriptValue objectWithProperties, const void *ptr,
                            const QString &type)
{
    QScriptEngine * const engine = objectWithProperties.engine();
    objectWithProperties.setProperty(QLatin1String("moduleProperties"),
                                     engine->newFunction(ModuleProperties::js_moduleProperties, 2));
    objectWithProperties.setProperty(QLatin1String("moduleProperty"),
                                     engine->newFunction(ModuleProperties::js_moduleProperty, 2));
    objectWithProperties.setProperty(ptrKey(), engine->toScriptValue(quintptr(ptr)));
    objectWithProperties.setProperty(typeKey(), type);
}

QScriptValue ModuleProperties::js_moduleProperties(QScriptContext *context, QScriptEngine *engine)
{
    ErrorInfo deprWarning(Tr::tr("The moduleProperties() function is deprecated and will be "
                                 "removed in a future version of Qbs. Use moduleProperty() "
                                 "instead."), context->backtrace());
    static_cast<ScriptEngine *>(engine)->logger().printWarning(deprWarning);
    try {
        return moduleProperties(context, engine);
    } catch (const ErrorInfo &e) {
        return context->throwError(e.toString());
    }
}

QScriptValue ModuleProperties::js_moduleProperty(QScriptContext *context, QScriptEngine *engine)
{
    try {
        return moduleProperties(context, engine);
    } catch (const ErrorInfo &e) {
        return context->throwError(e.toString());
    }
}

QScriptValue ModuleProperties::moduleProperties(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() < 2)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("Function moduleProperties() expects 2 arguments"));
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

    QVariant value;
    if (qbsEngine->isPropertyCacheEnabled())
        value = qbsEngine->retrieveFromPropertyCache(moduleName, propertyName, properties);
    if (!value.isValid()) {
        value = PropertyFinder().propertyValue(properties->value(), moduleName, propertyName);
        const Property p(moduleName, propertyName, value);
        if (artifact)
            qbsEngine->addPropertyRequestedFromArtifact(artifact, p);
        else
            qbsEngine->addPropertyRequestedInScript(p);

        // Cache the variant value. We must not cache the QScriptValue here, because it's a
        // reference and the user might change the actual object.
        if (qbsEngine->isPropertyCacheEnabled())
            qbsEngine->addToPropertyCache(moduleName, propertyName, properties, value);
    }
    return engine->toScriptValue(value);
}

} // namespace Internal
} // namespace qbs
