/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "propertydeclaration.h"

#include <QSharedData>

namespace qbs {
namespace Internal {

class PropertyDeclarationData : public QSharedData
{
public:
    PropertyDeclarationData()
        : type(PropertyDeclaration::UnknownType)
        , flags(PropertyDeclaration::DefaultFlags)
    {
    }

    QString name;
    PropertyDeclaration::Type type;
    PropertyDeclaration::Flags flags;
    QScriptValue allowedValues;
    QString description;
    QString initialValueSource;
    QStringList functionArgumentNames;
};


PropertyDeclaration::PropertyDeclaration()
    : d(new PropertyDeclarationData)
{
}

PropertyDeclaration::PropertyDeclaration(const QString &name, Type type, Flags flags)
    : d(new PropertyDeclarationData)
{
    d->name = name;
    d->type = type;
    d->flags = flags;
}

PropertyDeclaration::PropertyDeclaration(const PropertyDeclaration &other)
    : d(other.d)
{
}

PropertyDeclaration::~PropertyDeclaration()
{
}

PropertyDeclaration &PropertyDeclaration::operator=(const PropertyDeclaration &other)
{
    d = other.d;
    return *this;
}

bool PropertyDeclaration::isValid() const
{
    return d && d->type != UnknownType;
}

PropertyDeclaration::Type PropertyDeclaration::propertyTypeFromString(const QString &typeName)
{
    if (typeName == QLatin1String("bool"))
        return PropertyDeclaration::Boolean;
    if (typeName == QLatin1String("int"))
        return PropertyDeclaration::Integer;
    if (typeName == QLatin1String("path"))
        return PropertyDeclaration::Path;
    if (typeName == QLatin1String("pathList"))
        return PropertyDeclaration::PathList;
    if (typeName == QLatin1String("string"))
        return PropertyDeclaration::String;
    if (typeName == QLatin1String("stringList"))
        return PropertyDeclaration::StringList;
    if (typeName == QLatin1String("var") || typeName == QLatin1String("variant"))
        return PropertyDeclaration::Variant;
    return PropertyDeclaration::UnknownType;
}

const QString &PropertyDeclaration::name() const
{
    return d->name;
}

void PropertyDeclaration::setName(const QString &name)
{
    d->name = name;
}

PropertyDeclaration::Type PropertyDeclaration::type() const
{
    return d->type;
}

void PropertyDeclaration::setType(PropertyDeclaration::Type t)
{
    d->type = t;
}

PropertyDeclaration::Flags PropertyDeclaration::flags() const
{
    return d->flags;
}

void PropertyDeclaration::setFlags(Flags f)
{
    d->flags = f;
}

const QScriptValue &PropertyDeclaration::allowedValues() const
{
    return d->allowedValues;
}

void PropertyDeclaration::setAllowedValues(const QScriptValue &v)
{
    d->allowedValues = v;
}

const QString &PropertyDeclaration::description() const
{
    return d->description;
}

void PropertyDeclaration::setDescripton(const QString &str)
{
    d->description = str;
}

const QString &PropertyDeclaration::initialValueSource() const
{
    return d->initialValueSource;
}

void PropertyDeclaration::setInitialValueSource(const QString &str)
{
    d->initialValueSource = str;
}

const QStringList &PropertyDeclaration::functionArgumentNames() const
{
    return d->functionArgumentNames;
}

void PropertyDeclaration::setFunctionArgumentNames(const QStringList &lst)
{
    d->functionArgumentNames = lst;
}

} // namespace Internal
} // namespace qbs
