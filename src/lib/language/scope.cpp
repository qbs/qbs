/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#include "scope.h"
#include "scopechain.h"
#include "projectfile.h"
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>    // ### remove
#include <tools/scripttools.h>
#include <QScriptEngine>

namespace qbs {
namespace Internal {

Scope::Scope(QScriptEngine *engine, ScopesCachePtr cache, const QString &name)
    : QScriptClass(engine)
    , m_scopesCache(cache)
    , m_name(name)
{
}

QSharedPointer<Scope> Scope::create(QScriptEngine *engine, ScopesCachePtr cache, const QString &name, ProjectFile *owner)
{
    QSharedPointer<Scope> obj(new Scope(engine, cache, name));
    obj->value = engine->newObject(obj.data());
    owner->registerScope(obj);
    return obj;
}

Scope::~Scope()
{
}

QString Scope::name() const
{
    return m_name;
}

static const bool debugProperties = false;

Scope::QueryFlags Scope::queryProperty(const QScriptValue &object, const QScriptString &name,
                                       QueryFlags flags, uint *id)
{
    const QString nameString = name.toString();
    if (properties.contains(nameString)) {
        *id = 0;
        return (HandlesReadAccess | HandlesWriteAccess) & flags;
    }
    if (fallbackScope && fallbackScope.data()->queryProperty(object, name, flags, id)) {
        *id = 1;
        return (HandlesReadAccess | HandlesWriteAccess) & flags;
    }

    QScriptValue proto = value.prototype();
    if (proto.isValid()) {
        QScriptValue v = proto.property(name);
        if (!v.isValid()) {
            *id = 2;
            return (HandlesReadAccess | HandlesWriteAccess) & flags;
        }
    }

    if (debugProperties)
        qbsTrace() << "PROPERTIES: we don't handle " << name.toString();
    return 0;
}

QScriptValue Scope::property(const QScriptValue &object, const QScriptString &name, uint id)
{
    if (id == 1)
        return fallbackScope.data()->property(object, name, 0);
    else if (id == 2) {
        QString msg = Tr::tr("Property %0.%1 is undefined.");
        return engine()->currentContext()->throwError(msg.arg(m_name, name));
    }

    const QString nameString = name.toString();

    Property property = properties.value(nameString);

    if (debugProperties)
        qbsTrace() << "PROPERTIES: evaluating " << nameString;

    if (!property.isValid()) {
        if (debugProperties)
            qbsTrace() << " : no such property";
        return QScriptValue(); // does this raise an error?
    }

    if (property.scope) {
        if (debugProperties)
            qbsTrace() << " : object property";
        return property.scope->value;
    }

    if (property.value.isValid()) {
        if (debugProperties)
            qbsTrace() << " : pre-evaluated property: " << property.value.toVariant();
        return property.value;
    }

    // evaluate now
    if (debugProperties)
        qbsTrace() << " : evaluating now: " << property.valueSource.sourceCode();
    QScriptContext *context = engine()->currentContext();
    const QScriptValue oldActivation = context->activationObject();

    // evaluate base properties
    QLatin1String baseValueName("base");
    if (property.valueSourceUsesBase) {
        foreach (const Property &baseProperty, property.baseProperties) {
            context->setActivationObject(baseProperty.scopeChain->value());
            QScriptValue baseValue = engine()->evaluate(baseProperty.valueSource);
            if (baseValue.isError())
                return baseValue;
            if (baseValue.isUndefined())
                baseValue = engine()->newArray();
            engine()->globalObject().setProperty(baseValueName, baseValue);
        }
    }

    context->setActivationObject(property.scopeChain->value());

    QLatin1String oldValueName("outer");
    const bool usesOldProperty = fallbackScope && property.valueSourceUsesOuter;
    if (usesOldProperty) {
        QScriptValue oldValue = fallbackScope.data()->value.property(name);
        if (oldValue.isValid() && !oldValue.isError())
            engine()->globalObject().setProperty(oldValueName, oldValue);
    }

    QScriptValue result = engine()->evaluate(property.valueSource);
    if (result.isError())
        return result;

    if (debugProperties) {
        qbsTrace() << "PROPERTIES: evaluated " << nameString << " to " << result.toVariant() << " " << result.toString();
        if (result.isError())
            qbsTrace() << "            was error!";
    }

    m_scopesCache->insert(this);
    property.value = result;
    properties.insert(nameString, property);

    if (usesOldProperty)
        engine()->globalObject().setProperty(oldValueName, engine()->undefinedValue());
    if (property.valueSourceUsesBase)
        engine()->globalObject().setProperty(baseValueName, engine()->undefinedValue());
    context->setActivationObject(oldActivation);

    return result;
}

QScriptValue Scope::property(const QString &name) const
{
    QScriptValue result = value.property(name);
    if (result.isError())
        throw Error(result.toString());
    return result;
}

bool Scope::boolValue(const QString &name, bool defaultValue) const
{
    QScriptValue scriptValue = property(name);
    if (scriptValue.isBool())
        return scriptValue.toBool();
    return defaultValue;
}

QString Scope::stringValue(const QString &name) const
{
    QScriptValue scriptValue = property(name);
    if (scriptValue.isString())
        return scriptValue.toString();
    QVariant v = scriptValue.toVariant();
    if (v.type() == QVariant::String) {
        return v.toString();
    } else if (v.type() == QVariant::StringList) {
        const QStringList lst = v.toStringList();
        if (lst.count() == 1)
            return lst.first();
    }
    return QString();
}

QStringList Scope::stringListValue(const QString &name) const
{
    return toStringList(property(name));
}

QString Scope::verbatimValue(const QString &name) const
{
    const Property &property = properties.value(name);
    return property.valueSource.sourceCode();
}

void Scope::dump(const QByteArray &aIndent) const
{
    QByteArray indent = aIndent;
    printf("%sScope: {\n", indent.constData());
    const QByteArray dumpIndent = "  ";
    indent.append(dumpIndent);
    printf("%sName: '%s'\n", indent.constData(), qPrintable(m_name));
    if (!properties.isEmpty()) {
        printf("%sProperties: [\n", indent.constData());
        indent.append(dumpIndent);
        foreach (const QString &propertyName, properties.keys()) {
            QScriptValue scriptValue = property(propertyName);
            QString propertyValue;
            if (scriptValue.isString())
                propertyValue = stringValue(propertyName);
            else if (scriptValue.isArray())
                propertyValue = stringListValue(propertyName).join(", ");
            else if (scriptValue.isBool())
                propertyValue = boolValue(propertyName) ? "true" : "false";
            else
                propertyValue = verbatimValue(propertyName);
            printf("%s'%s': %s\n", indent.constData(), qPrintable(propertyName), qPrintable(propertyValue));
        }
        indent.chop(dumpIndent.length());
        printf("%s]\n", indent.constData());
    }
    if (!declarations.isEmpty())
        printf("%sPropertyDeclarations: [%s]\n", indent.constData(), qPrintable(QStringList(declarations.keys()).join(", ")));

    indent.chop(dumpIndent.length());
    printf("%s}\n", indent.constData());
}

void Scope::insertAndDeclareProperty(const QString &propertyName, const Property &property, PropertyDeclaration::Type propertyType)
{
    properties.insert(propertyName, property);
    declarations.insert(propertyName, PropertyDeclaration(propertyName, propertyType));
}

} // namespace Internal
} // namespace qbs
