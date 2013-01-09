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

#ifndef QBS_SCOPECHAIN_H
#define QBS_SCOPECHAIN_H

#include "property.h"
#include <QScriptClass>
#include <QSharedPointer>

namespace qbs {
namespace Internal {

class Scope;

class ScopeChain : public QScriptClass
{
    Q_DISABLE_COPY(ScopeChain)
public:
    typedef QSharedPointer<ScopeChain> Ptr;

    ScopeChain(QScriptEngine *engine, const QSharedPointer<Scope> &root = QSharedPointer<Scope>());
    ~ScopeChain();

    ScopeChain *clone() const;
    QScriptValue value();
    QSharedPointer<Scope> first() const;
    QSharedPointer<Scope> last() const;
    // returns this
    ScopeChain *prepend(const QSharedPointer<Scope> &newTop);

    QSharedPointer<Scope> findNonEmpty(const QString &name) const;
    QSharedPointer<Scope> find(const QString &name) const;

    Property lookupProperty(const QString &name) const;

protected:
    // QScriptClass interface
    QueryFlags queryProperty(const QScriptValue &object, const QScriptString &name,
                             QueryFlags flags, uint *id);
    QScriptValue property(const QScriptValue &object, const QScriptString &name, uint id);
    void setProperty(QScriptValue &object, const QScriptString &name, uint id, const QScriptValue &m_value);

private:
    QList<QWeakPointer<Scope> > m_scopes;
    QScriptValue m_value;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_SCOPECHAIN_H
