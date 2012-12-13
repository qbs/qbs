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

#include "scopechain.h"
#include "scope.h"
#include <logging/translator.h>
#include <QScriptEngine>

namespace qbs {
namespace Internal {

ScopeChain::ScopeChain(QScriptEngine *engine, const QSharedPointer<Scope> &root)
    : QScriptClass(engine)
{
    m_value = engine->newObject(this);
    if (root)
        m_scopes.append(root);
}

ScopeChain::~ScopeChain()
{
}

ScopeChain *ScopeChain::clone() const
{
    ScopeChain *s = new ScopeChain(engine());
    s->m_scopes = m_scopes;
    return s;
}

QScriptValue ScopeChain::value()
{
    return m_value;
}

Scope::Ptr ScopeChain::first() const
{
    return m_scopes.first();
}

Scope::Ptr ScopeChain::last() const
{
    return m_scopes.last();
}

ScopeChain *ScopeChain::prepend(const QSharedPointer<Scope> &newTop)
{
    if (!newTop)
        return this;
    m_scopes.prepend(newTop);
    return this;
}

QSharedPointer<Scope> ScopeChain::findNonEmpty(const QString &name) const
{
    foreach (const Scope::Ptr &scope, m_scopes) {
        if (scope->name() == name && !scope->properties.isEmpty())
            return scope;
    }
    return Scope::Ptr();
}

QSharedPointer<Scope> ScopeChain::find(const QString &name) const
{
    foreach (const Scope::Ptr &scope, m_scopes) {
        if (scope->name() == name)
            return scope;
    }
    return Scope::Ptr();
}

Property ScopeChain::lookupProperty(const QString &name) const
{
    foreach (const Scope::Ptr &scope, m_scopes) {
        Property p = scope->properties.value(name);
        if (p.isValid())
            return p;
    }
    return Property();
}

ScopeChain::QueryFlags ScopeChain::queryProperty(const QScriptValue &object, const QScriptString &name,
                                       QueryFlags flags, uint *id)
{
    Q_UNUSED(object);
    Q_UNUSED(name);
    Q_UNUSED(id);
    return (HandlesReadAccess | HandlesWriteAccess) & flags;
}

QScriptValue ScopeChain::property(const QScriptValue &object, const QScriptString &name, uint id)
{
    Q_UNUSED(object);
    Q_UNUSED(id);
    QScriptValue value;
    foreach (const Scope::Ptr &scope, m_scopes) {
        value = scope->value.property(name);
        if (value.isError()) {
            engine()->clearExceptions();
        } else if (value.isValid()) {
            return value;
        }
    }
    value = engine()->globalObject().property(name);
    if (!value.isValid() || (value.isUndefined() && name.toString() != QLatin1String("undefined"))) {
        QString msg = Tr::tr("Undefined property '%1'");
        value = engine()->currentContext()->throwError(msg.arg(name.toString()));
    }
    return value;
}

void ScopeChain::setProperty(QScriptValue &, const QScriptString &name, uint, const QScriptValue &)
{
    QString msg = Tr::tr("Removing or setting property '%1' in a binding is invalid.");
    engine()->currentContext()->throwError(msg.arg(name.toString()));
}
} // namespace Internal
} // namespace qbs
