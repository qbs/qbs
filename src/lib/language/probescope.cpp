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

#include "probescope.h"
#include "scopechain.h"
#include <logging/translator.h>
#include <tools/error.h>
#include <QScriptEngine>

namespace qbs {
namespace Internal {

ProbeScope::ProbeScope(QScriptEngine *engine, const Scope::Ptr &scope)
    : QScriptClass(engine), m_scope(scope)
{
    m_value = engine->newObject(this);
    Property property = m_scope->properties.value("configure");
    m_scopeChain = property.scopeChain.staticCast<QScriptClass>();
}

ProbeScope::Ptr ProbeScope::create(QScriptEngine *engine, const Scope::Ptr &scope)
{
    return ProbeScope::Ptr(new ProbeScope(engine, scope));
}

ProbeScope::~ProbeScope()
{
}

QScriptValue ProbeScope::value()
{
    return m_value;
}

QScriptClass::QueryFlags ProbeScope::queryProperty(const QScriptValue &object, const QScriptString &name,
                                                           QScriptClass::QueryFlags flags, uint *id)
{
    Q_UNUSED(object);
    Q_UNUSED(name);
    Q_UNUSED(flags);
    Q_UNUSED(id);
    return (HandlesReadAccess | HandlesWriteAccess) & flags;
}

QScriptValue ProbeScope::property(const QScriptValue &object, const QScriptString &name, uint id)
{
    const QString nameString = name.toString();
    if (m_scope->properties.contains(nameString))
        return m_scope->property(name);
    QScriptValue proto = m_value.prototype();
    if (proto.isValid()) {
        QScriptValue value = proto.property(name);
        if (value.isValid())
            return value;
    }
    return m_scopeChain->property(object, name, id);
}

void ProbeScope::setProperty(QScriptValue &object, const QScriptString &name, uint id, const QScriptValue &value)
{
    const QString nameString = name.toString();
    if (nameString == "configure") {
        throw Error(Tr::tr("Can not access 'configure' property from itself"));
    } else if (m_scope->properties.contains(nameString)) {
        Property &property = m_scope->properties[nameString];
        property.value = value;
    } else {
        m_scopeChain->setProperty(object, name, id, value);
    }
}

} // namespace Internal
} // namespace qbs
