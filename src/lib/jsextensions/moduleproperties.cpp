/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "moduleproperties.h"

#include <buildgraph/artifact.h>
#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
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
    objectWithProperties.setProperty("moduleProperties",
                                     engine->newFunction(ModuleProperties::js_moduleProperties, 2));
    objectWithProperties.setProperty("moduleProperty",
                                     engine->newFunction(ModuleProperties::js_moduleProperty, 2));
    objectWithProperties.setProperty(ptrKey(), engine->toScriptValue(quintptr(ptr)));
    objectWithProperties.setProperty(typeKey(), type);
}

QScriptValue ModuleProperties::js_moduleProperties(QScriptContext *context, QScriptEngine *engine)
{
    return moduleProperties(context, engine, false);
}

QScriptValue ModuleProperties::js_moduleProperty(QScriptContext *context, QScriptEngine *engine)
{
    return moduleProperties(context, engine, true);
}

QScriptValue ModuleProperties::moduleProperties(QScriptContext *context, QScriptEngine *engine,
                                                bool oneValue)
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
    if (typeScriptValue.toString() == productType()) {
        properties = static_cast<const ResolvedProduct *>(ptr)->properties;
    } else if (typeScriptValue.toString() == artifactType()) {
        properties = static_cast<const Artifact *>(ptr)->properties;
    } else {
        return context->throwError(QScriptContext::TypeError,
                                   QLatin1String("Internal error: invalid type"));
    }

    ScriptEngine * const qbsEngine = static_cast<ScriptEngine *>(engine);
    const QString moduleName = context->argument(0).toString();
    const QString propertyName = context->argument(1).toString();

    QVariant value = qbsEngine->retrieveFromPropertyCache(moduleName, propertyName, properties);
    if (!value.isValid()) {
        if (oneValue)
            value = PropertyFinder().propertyValue(properties->value(), moduleName, propertyName);
        else
            value = PropertyFinder().propertyValues(properties->value(), moduleName, propertyName);
        const Property p(moduleName, propertyName, value);
        qbsEngine->addProperty(p);

        // Cache the variant value. We must not cache the QScriptValue here, because it's a
        // reference and the user might change the actual object.
        qbsEngine->addToPropertyCache(moduleName, propertyName, properties, value);
    }
    return engine->toScriptValue(value);
}

} // namespace Internal
} // namespace qbs
