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

#ifndef QBS_PROPERTYDECLARATION_H
#define QBS_PROPERTYDECLARATION_H

#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>

QT_BEGIN_NAMESPACE
class QStringList;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {
class DeprecationInfo;
class PropertyDeclarationData;

class PropertyDeclaration
{
public:
    enum Type
    {
        UnknownType,
        Boolean,
        Integer,
        Path,
        PathList,
        String,
        StringList,
        Variant,
        VariantList,
    };

    enum Flag
    {
        DefaultFlags = 0,
        ReadOnlyFlag = 0x1,
        PropertyNotAvailableInConfig = 0x2     // Is this property part of a project, product or file configuration?
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    PropertyDeclaration();
    PropertyDeclaration(const QString &name, Type type, const QString &initialValue = QString(),
                        Flags flags = DefaultFlags);
    PropertyDeclaration(const PropertyDeclaration &other);
    ~PropertyDeclaration();

    PropertyDeclaration &operator=(const PropertyDeclaration &other);

    bool isValid() const;
    bool isScalar() const;

    static Type propertyTypeFromString(const QString &typeName);
    QString typeString() const;
    static QString typeString(Type t);

    const QString &name() const;
    void setName(const QString &name);

    Type type() const;
    void setType(Type t);

    Flags flags() const;
    void setFlags(Flags f);

    const QString &description() const;
    void setDescription(const QString &str);

    const QString &initialValueSource() const;
    void setInitialValueSource(const QString &str);

    const QStringList &functionArgumentNames() const;
    void setFunctionArgumentNames(const QStringList &lst);

    bool isDeprecated() const;
    bool isExpired() const;
    const DeprecationInfo &deprecationInfo() const;
    void setDeprecationInfo(const DeprecationInfo &deprecationInfo);

private:
    QSharedDataPointer<PropertyDeclarationData> d;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_PROPERTYDECLARATION_H
