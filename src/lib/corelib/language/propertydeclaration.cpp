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
#include "filecontext.h"
#include "item.h"
#include "qualifiedid.h"
#include "value.h"

#include <api/languageinfo.h>
#include <loader/loaderutils.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/setupprojectparameters.h>
#include <tools/qttools.h>
#include <tools/stringconstants.h>

#include <QtCore/qmetatype.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

namespace qbs {
namespace Internal {

// returns QMetaType::UnknownType for types that do not need conversion
static QMetaType::Type variantType(PropertyDeclaration::Type t)
{
    switch (t) {
    case PropertyDeclaration::UnknownType:
        break;
    case PropertyDeclaration::Boolean:
        return QMetaType::Bool;
    case PropertyDeclaration::Integer:
        return QMetaType::Int;
    case PropertyDeclaration::Path:
        return QMetaType::QString;
    case PropertyDeclaration::PathList:
        return QMetaType::QStringList;
    case PropertyDeclaration::String:
        return QMetaType::QString;
    case PropertyDeclaration::StringList:
        return QMetaType::QStringList;
    case PropertyDeclaration::VariantList:
        return QMetaType::QVariantList;
    case PropertyDeclaration::Variant:
        break;
    }
    return QMetaType::UnknownType;
}

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
    QStringList allowedValues;
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

PropertyDeclaration::PropertyDeclaration(const PropertyDeclaration &other) = default;

PropertyDeclaration::~PropertyDeclaration() = default;

PropertyDeclaration &PropertyDeclaration::operator=(const PropertyDeclaration &other) = default;

bool PropertyDeclaration::isValid() const
{
    return d && d->type != UnknownType;
}

bool PropertyDeclaration::isScalar() const
{
    // ### Should be determined by a PropertyOption in the future.
    return d->type != PathList && d->type != StringList && d->type != VariantList;
}

static QString boolString() { return QStringLiteral("bool"); }
static QString intString() { return QStringLiteral("int"); }
static QString pathListString() { return QStringLiteral("pathList"); }
static QString stringString() { return QStringLiteral("string"); }
static QString stringListString() { return QStringLiteral("stringList"); }
static QString varString() { return QStringLiteral("var"); }
static QString variantString() { return QStringLiteral("variant"); }
static QString varListString() { return QStringLiteral("varList"); }

PropertyDeclaration::Type PropertyDeclaration::propertyTypeFromString(const QString &typeName)
{
    if (typeName == boolString())
        return PropertyDeclaration::Boolean;
    if (typeName == intString())
        return PropertyDeclaration::Integer;
    if (typeName == StringConstants::pathType())
        return PropertyDeclaration::Path;
    if (typeName == pathListString())
        return PropertyDeclaration::PathList;
    if (typeName == stringString())
        return PropertyDeclaration::String;
    if (typeName == stringListString())
        return PropertyDeclaration::StringList;
    if (typeName == varString() || typeName == variantString())
        return PropertyDeclaration::Variant;
    if (typeName == varListString())
        return PropertyDeclaration::VariantList;
    return PropertyDeclaration::UnknownType;
}

QString PropertyDeclaration::typeString() const
{
    return typeString(type());
}

QString PropertyDeclaration::typeString(PropertyDeclaration::Type t)
{
    switch (t) {
    case Boolean: return boolString();
    case Integer: return intString();
    case Path: return StringConstants::pathType();
    case PathList: return pathListString();
    case String: return stringString();
    case StringList: return stringListString();
    case Variant: return variantString();
    case VariantList: return varListString();
    case UnknownType: return QStringLiteral("unknown");
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

const QStringList &PropertyDeclaration::allowedValues() const
{
    return d->allowedValues;
}

void PropertyDeclaration::setAllowedValues(const QStringList &v)
{
    d->allowedValues = v;
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
    return isDeprecated() && deprecationInfo().removalVersion() <= LanguageInfo::qbsVersion();
}

const DeprecationInfo &PropertyDeclaration::deprecationInfo() const
{
    return d->deprecationInfo;
}

void PropertyDeclaration::setDeprecationInfo(const DeprecationInfo &deprecationInfo)
{
    d->deprecationInfo = deprecationInfo;
}

ErrorInfo PropertyDeclaration::checkForDeprecation(DeprecationWarningMode mode,
                                                   const CodeLocation &loc, Logger &logger) const
{
    return deprecationInfo().checkForDeprecation(mode, name(), loc, false, logger);
}

QVariant PropertyDeclaration::convertToPropertyType(const QVariant &v, Type t,
    const QStringList &namePrefix, const QString &key)
{
    if (v.isNull() || !v.isValid())
        return v;
    const auto vt = variantType(t);
    if (vt == QMetaType::UnknownType)
        return v;

    // Handle the foo,bar,bla stringlist syntax.
    if (t == PropertyDeclaration::StringList && v.userType() == QMetaType::QString)
        return v.toString().split(QLatin1Char(','));

    QVariant c = v;
    if (!qVariantConvert(c, vt)) {
        QStringList name = namePrefix;
        name << key;
        throw ErrorInfo(Tr::tr("Value '%1' of property '%2' has incompatible type.")
                        .arg(v.toString(), name.join(QLatin1Char('.'))));
    }
    return c;
}

void PropertyDeclaration::checkAllowedValues(
    const QVariant &value,
    const CodeLocation &loc,
    const QString &key,
    LoaderState &loaderState) const
{
    const auto type = d->type;
    if (type != PropertyDeclaration::String && type != PropertyDeclaration::StringList)
        return;

    if (value.isNull())
        return;

    const auto &allowedValues = d->allowedValues;
    if (allowedValues.isEmpty())
        return;

    const auto checkValue = [&loc, &allowedValues, &key, &loaderState](const QString &value)
    {
        if (!allowedValues.contains(value)) {
            const auto message = Tr::tr("Value '%1' is not allowed for property '%2'.")
                                     .arg(value, key);
            ErrorInfo error(message, loc);
            handlePropertyError(error, loaderState.parameters(), loaderState.logger());
        }
    };

    if (type == PropertyDeclaration::StringList) {
        const auto strings = value.toStringList();
        for (const auto &string: strings) {
            checkValue(string);
        }
    } else if (type == PropertyDeclaration::String) {
        checkValue(value.toString());
    }
}

namespace {
class PropertyDeclarationCheck : public ValueHandler
{
public:
    PropertyDeclarationCheck(LoaderState &loaderState) : m_loaderState(loaderState) {}
    void operator()(Item *item)
    {
        m_checkingProject = item->type() == ItemType::Project;
        handleItem(item);
    }

private:
    void handle(JSSourceValue *value) override
    {
        if (!value->createdByPropertiesBlock()) {
            const ErrorInfo error(Tr::tr("Property '%1' is not declared.")
                                      .arg(m_currentName), value->location());
            handlePropertyError(error, m_loaderState.parameters(), m_loaderState.logger());
        }
    }
    void handle(ItemValue *value) override
    {
        if (checkItemValue(value))
            handleItem(value->item());
    }
    bool checkItemValue(ItemValue *value)
    {
        // TODO: Remove once QBS-1030 is fixed.
        if (parentItem()->type() == ItemType::Artifact)
            return false;

        if (parentItem()->type() == ItemType::Properties)
            return false;

        // TODO: Check where the in-between module instances come from.
        if (value->item()->type() == ItemType::ModuleInstancePlaceholder) {
            for (auto it = m_parentItems.rbegin(); it != m_parentItems.rend(); ++it) {
                if ((*it)->type() == ItemType::Group)
                    return false;
                if ((*it)->type() == ItemType::ModulePrefix)
                    continue;
                break;
            }
        }

        if (value->item()->type() != ItemType::ModuleInstance
            && value->item()->type() != ItemType::ModulePrefix
            && (!parentItem()->file() || !parentItem()->file()->idScope()
                || !parentItem()->file()->idScope()->hasProperty(m_currentName))
            && !value->createdByPropertiesBlock()) {
            CodeLocation location = value->location();
            for (int i = int(m_parentItems.size() - 1); !location.isValid() && i >= 0; --i)
                location = m_parentItems.at(i)->location();
            const ErrorInfo error(Tr::tr("Item '%1' is not declared. "
                                         "Did you forget to add a Depends item?")
                                      .arg(m_currentModuleName.toString()), location);
            handlePropertyError(error, m_loaderState.parameters(), m_loaderState.logger());
            return false;
        }

        return true;
    }
    void handleItem(Item *item)
    {
        if (m_checkingProject && item->type() == ItemType::Product)
            return;
        if (!m_handledItems.insert(item).second)
            return;
        if (item->type() == ItemType::Module
            || item->type() == ItemType::Export
            || (item->type() == ItemType::ModuleInstance && !item->isPresentModule())
            || item->type() == ItemType::Properties

            // The Properties child of a SubProject item is not a regular item.
            || item->type() == ItemType::PropertiesInSubProject

            || m_loaderState.topLevelProject().isDisabledItem(item)) {
            return;
        }

        m_parentItems.push_back(item);
        for (Item::PropertyMap::const_iterator it = item->properties().constBegin();
             it != item->properties().constEnd(); ++it) {
            if (item->type() == ItemType::Product && it.key() == StringConstants::moduleProviders()
                && it.value()->type() == Value::ItemValueType)
                continue;
            const PropertyDeclaration decl = item->propertyDeclaration(it.key());
            if (decl.isValid()) {
                const ErrorInfo deprecationError = decl.checkForDeprecation(
                    m_loaderState.parameters().deprecationWarningMode(), it.value()->location(),
                            m_loaderState.logger());
                if (deprecationError.hasError()) {
                    handlePropertyError(deprecationError, m_loaderState.parameters(),
                                        m_loaderState.logger());
                }
                continue;
            }
            m_currentName = it.key();
            const QualifiedId oldModuleName = m_currentModuleName;
            if (parentItem()->type() != ItemType::ModulePrefix)
                m_currentModuleName.clear();
            m_currentModuleName.push_back(m_currentName);
            it.value()->apply(this);
            m_currentModuleName = oldModuleName;
        }
        m_parentItems.pop_back();
        for (Item * const child : item->children()) {
            switch (child->type()) {
            case ItemType::Export:
            case ItemType::Depends:
            case ItemType::Parameter:
            case ItemType::Parameters:
                break;
            case ItemType::Group:
                if (item->type() == ItemType::Module || item->type() == ItemType::ModuleInstance)
                    break;
                Q_FALLTHROUGH();
            default:
                handleItem(child);
            }
        }
    }
    void handle(VariantValue *) override { /* only created internally - no need to check */ }

    Item *parentItem() const { return m_parentItems.back(); }

    LoaderState &m_loaderState;
    Set<Item *> m_handledItems;
    std::vector<Item *> m_parentItems;
    QualifiedId m_currentModuleName;
    QString m_currentName;
    bool m_checkingProject = false;
};
} // namespace

void checkPropertyDeclarations(Item *topLevelItem, LoaderState &loaderState)
{
    (PropertyDeclarationCheck(loaderState))(topLevelItem);
}

} // namespace Internal
} // namespace qbs
