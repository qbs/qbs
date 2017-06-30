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

#include "propertydeclaration.h"

#include "deprecationinfo.h"

#include <QtCore/qshareddata.h>
#include <QtCore/qstringlist.h>

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
    QString description;
    QString initialValueSource;
    QStringList functionArgumentNames;
    DeprecationInfo deprecationInfo;
};


PropertyDeclaration::PropertyDeclaration()
    : d(new PropertyDeclarationData)
{
}

PropertyDeclaration::PropertyDeclaration(const QString &name, Type type,
                                         const QString &initialValue, Flags flags)
    : d(new PropertyDeclarationData)
{
    d->name = name;
    d->type = type;
    d->initialValueSource = initialValue;
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

bool PropertyDeclaration::isScalar() const
{
    // ### Should be determined by a PropertyOption in the future.
    return d->type != PathList && d->type != StringList;
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

QString PropertyDeclaration::typeString() const
{
    switch (type()) {
    case Boolean: return QLatin1String("bool");
    case Integer: return QLatin1String("int");
    case Path: return QLatin1String("path");
    case PathList: return QLatin1String("pathList");
    case String: return QLatin1String("string");
    case StringList: return QLatin1String("stringList");
    case Variant: return QLatin1String("variant");
    case UnknownType: return QLatin1String("unknown");
    }
    Q_UNREACHABLE(); // For stupid compilers.
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

const QString &PropertyDeclaration::description() const
{
    return d->description;
}

void PropertyDeclaration::setDescription(const QString &str)
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

bool PropertyDeclaration::isDeprecated() const
{
    return d->deprecationInfo.isValid();
}

bool PropertyDeclaration::isExpired() const
{
    return isDeprecated() && deprecationInfo().removalVersion() <= Version::qbsVersion();
}

const DeprecationInfo &PropertyDeclaration::deprecationInfo() const
{
    return d->deprecationInfo;
}

void PropertyDeclaration::setDeprecationInfo(const DeprecationInfo &deprecationInfo)
{
    d->deprecationInfo = deprecationInfo;
}

} // namespace Internal
} // namespace qbs
