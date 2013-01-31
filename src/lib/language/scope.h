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

#ifndef SCOPE_H
#define SCOPE_H

#include "property.h"
#include "propertydeclaration.h"
#include <logging/logger.h>
#include <QHash>
#include <QScriptClass>
#include <set>

namespace qbs {
namespace Internal {

class ProjectFile;
class Scope;
typedef std::set<Scope *> ScopesCache;
typedef QSharedPointer<ScopesCache> ScopesCachePtr;

class Scope : public QScriptClass
{
    Q_DISABLE_COPY(Scope)
    Scope(QScriptEngine *engine, ScopesCachePtr cache, const QString &name, const Logger &logger);

    ScopesCachePtr m_scopesCache;

public:
    typedef QSharedPointer<Scope> Ptr;
    typedef QSharedPointer<const Scope> ConstPtr;

    static Ptr create(QScriptEngine *engine, ScopesCachePtr cache, const QString &name,
                      ProjectFile *owner, const Logger &logger);
    ~Scope();

    QString name() const;

protected:
    // QScriptClass interface
    QueryFlags queryProperty(const QScriptValue &object, const QScriptString &name,
                             QueryFlags flags, uint *id);
    QScriptValue property(const QScriptValue &object, const QScriptString &name, uint id);

public:
    QScriptValue property(const QString &name) const;
    bool boolValue(const QString &name, bool defaultValue = false) const;
    QString stringValue(const QString &name) const;
    QStringList stringListValue(const QString &name) const;
    QString verbatimValue(const QString &name) const;
    void dump(const QByteArray &indent) const;
    void insertAndDeclareProperty(const QString &propertyName, const Property &property,
                                  PropertyDeclaration::Type propertyType = PropertyDeclaration::Variant);

    QHash<QString, Property> properties;
    QHash<QString, PropertyDeclaration> declarations;

    QString m_name;
    QWeakPointer<Scope> fallbackScope;
    QScriptValue value;
    Logger m_logger;
};

} // namespace Internal
} // namespace qbs

#endif // SCOPE_H
