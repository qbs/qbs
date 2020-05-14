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
#include "projectdata.h"

#include "projectdata_p.h"
#include "propertymap_p.h"
#include <language/language.h>
#include <language/propertymapinternal.h>
#include <tools/fileinfo.h>
#include <tools/jsliterals.h>
#include <tools/qbsassert.h>
#include <tools/stringconstants.h>
#include <tools/qttools.h>
#include <tools/stringconstants.h>

#include <QtCore/qdir.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsonobject.h>

#include <algorithm>

namespace qbs {

using namespace Internal;

template<typename T> static QJsonArray toJsonArray(const QList<T> &list,
                                                   const QStringList &moduleProperties)
{
    QJsonArray jsonArray;
    std::transform(list.begin(), list.end(), std::back_inserter(jsonArray),
                   [&moduleProperties](const T &v) { return v.toJson(moduleProperties);});
    return jsonArray;
}

static QVariant getModuleProperty(const PropertyMap &properties, const QString &fullPropertyName)
{
    const int lastDotIndex = fullPropertyName.lastIndexOf(QLatin1Char('.'));
    if (lastDotIndex == -1)
        return QVariant();
    return properties.getModuleProperty(fullPropertyName.left(lastDotIndex),
                                        fullPropertyName.mid(lastDotIndex + 1));
}

static void addModuleProperties(QJsonObject &obj, const PropertyMap &properties,
                                const QStringList &propertyNames)
{
    QJsonObject propertyValues;
    for (const QString &prop : propertyNames) {
        const QVariant v = getModuleProperty(properties, prop);
        if (v.isValid())
            propertyValues.insert(prop, QJsonValue::fromVariant(v));
    }
    if (!propertyValues.isEmpty())
        obj.insert(StringConstants::modulePropertiesKey(), propertyValues);
}

/*!
 * \class GroupData
 * \brief The \c GroupData class corresponds to the Group item in a qbs source file.
 */

GroupData::GroupData() : d(new GroupDataPrivate)
{
}

GroupData::GroupData(const GroupData &other) = default;

GroupData::GroupData(GroupData &&) Q_DECL_NOEXCEPT = default;

GroupData &GroupData::operator=(const GroupData &other) = default;

GroupData &GroupData::operator=(GroupData &&) Q_DECL_NOEXCEPT = default;

GroupData::~GroupData() = default;

/*!
 * \brief Returns true if and only if the Group holds data that was initialized by Qbs.
 */
bool GroupData::isValid() const
{
    return d->isValid;
}

QJsonObject GroupData::toJson(const QStringList &moduleProperties) const
{
    QJsonObject obj;
    if (isValid()) {
        obj.insert(StringConstants::locationKey(), location().toJson());
        obj.insert(StringConstants::nameProperty(), name());
        obj.insert(StringConstants::prefixProperty(), prefix());
        obj.insert(StringConstants::isEnabledKey(), isEnabled());
        obj.insert(QStringLiteral("source-artifacts"), toJsonArray(sourceArtifacts(), {}));
        obj.insert(QStringLiteral("source-artifacts-from-wildcards"),
                   toJsonArray(sourceArtifactsFromWildcards(), {}));
        addModuleProperties(obj, properties(), moduleProperties);
    }
    return obj;
}

/*!
 * \brief The location at which the group is defined in the respective source file.
 */
CodeLocation GroupData::location() const
{
    return d->location;
}

/*!
 * \brief The name of the group.
 */
QString GroupData::name() const
{
    return d->name;
}

/*!
 * \brief The prefix of the group.
 */
QString GroupData::prefix() const
{
    return d->prefix;
}

/*!
 * \brief The files listed in the group item's "files" binding.
 * \note These do not include expanded wildcards.
 * \sa GroupData::sourceArtifactsFromWildcards
 */
QList<ArtifactData> GroupData::sourceArtifacts() const
{
    return d->sourceArtifacts;
}

/*!
 * \brief The list of files resulting from expanding all wildcard patterns in the group.
 */
QList<ArtifactData> GroupData::sourceArtifactsFromWildcards() const
{
    return d->sourceArtifactsFromWildcards;
}

/*!
 * \brief All files in this group, regardless of how whether they were given explicitly
 *        or via wildcards.
 * \sa GroupData::sourceArtifacts
 * \sa GroupData::sourceArtifactsFromWildcards
 */
QList<ArtifactData> GroupData::allSourceArtifacts() const
{
    return sourceArtifacts() + sourceArtifactsFromWildcards();
}

/*!
 * \brief The set of properties valid in this group.
 * Typically, most of them are inherited from the respective \c Product.
 */
PropertyMap GroupData::properties() const
{
    return d->properties;
}

/*!
 * \brief Returns true if this group is enabled in Qbs
 * This method returns the "condition" property of the \c Group definition. If the group is enabled
 * then the files in this group will be processed, provided the product it belongs to is also
 * enabled.
 *
 * Note that a group can be enabled, even if the product it belongs to is not. In this case
 * the files in the group will not be processed.
 * \sa ProductData::isEnabled()
 */
bool GroupData::isEnabled() const
{
    QBS_ASSERT(isValid(), return false);
    return d->isEnabled;
}

/*!
 * \brief The paths of all files in this group.
 * \sa GroupData::allSourceArtifacts
 */
QStringList GroupData::allFilePaths() const
{
    const QList<ArtifactData> &artifacts = allSourceArtifacts();
    QStringList paths;
    paths.reserve(artifacts.size());
    std::transform(artifacts.cbegin(), artifacts.cend(), std::back_inserter(paths),
                          [](const ArtifactData &sa) { return sa.filePath(); });
    return paths;
}

bool operator!=(const GroupData &lhs, const GroupData &rhs)
{
    return !(lhs == rhs);
}

bool operator==(const GroupData &lhs, const GroupData &rhs)
{
    if (!lhs.isValid() && !rhs.isValid())
        return true;

    return lhs.isValid() == rhs.isValid()
            && lhs.name() == rhs.name()
            && lhs.location() == rhs.location()
            && lhs.sourceArtifactsFromWildcards() == rhs.sourceArtifactsFromWildcards()
            && lhs.sourceArtifacts() == rhs.sourceArtifacts()
            && lhs.properties() == rhs.properties()
            && lhs.isEnabled() == rhs.isEnabled();
}

bool operator<(const GroupData &lhs, const GroupData &rhs)
{
    return lhs.name() < rhs.name();
}


/*!
 * \class ArtifactData
 * The \c ArtifactData class describes a file in a product. It is either a source file
 * or it gets generated during the build process.
 */

ArtifactData::ArtifactData() : d(new ArtifactDataPrivate)
{
}

ArtifactData::ArtifactData(const ArtifactData &other) = default;

ArtifactData::ArtifactData(ArtifactData &&) Q_DECL_NOEXCEPT = default;

ArtifactData &ArtifactData::operator=(const ArtifactData &other) = default;

ArtifactData &ArtifactData::operator=(ArtifactData &&) Q_DECL_NOEXCEPT = default;

ArtifactData::~ArtifactData() = default;

/*!
 * \brief Returns true if and only if this object holds data that was initialized by Qbs.
 */
bool ArtifactData::isValid() const
{
    return d->isValid;
}

QJsonObject ArtifactData::toJson(const QStringList &moduleProperties) const
{
    QJsonObject obj;
    if (isValid()) {
        obj.insert(StringConstants::filePathKey(), filePath());
        obj.insert(QStringLiteral("file-tags"), QJsonArray::fromStringList(fileTags()));
        obj.insert(QStringLiteral("is-generated"), isGenerated());
        obj.insert(QStringLiteral("is-executable"), isExecutable());
        obj.insert(QStringLiteral("is-target"), isTargetArtifact());
        obj.insert(QStringLiteral("install-data"), installData().toJson());
        addModuleProperties(obj, properties(), moduleProperties);
    }
    return obj;
}

/*!
 * \brief The full path of this file.
 */
QString ArtifactData::filePath() const
{
    return d->filePath;
}

/*!
 * \brief The tags of this file.
 * Typically, this list will contain just one element.
 */
QStringList ArtifactData::fileTags() const
{
    return d->fileTags;
}

bool ArtifactData::isGenerated() const
{
    return d->isGenerated;
}

/*!
 * \brief True if and only if this file is executable,
 * either natively or through an interpreter or shell.
 */
bool ArtifactData::isExecutable() const
{
    const bool isBundle = d->properties.getModuleProperty(
                QStringLiteral("bundle"), QStringLiteral("isBundle")).toBool();
    return isRunnableArtifact(
                FileTags::fromStringList(d->fileTags), isBundle);
}

/*!
 * \brief True if and only if this artifact is a target artifact of its product.
 */
bool ArtifactData::isTargetArtifact() const
{
    QBS_ASSERT(isValid(), return false);
    return d->isTargetArtifact;
}

/*!
 * \brief The properties of this file.
 */
PropertyMap ArtifactData::properties() const
{
    return d->properties;
}

/*!
    \brief The installation-related data of this artifact.
 */
InstallData ArtifactData::installData() const
{
    return d->installData;
}

bool operator==(const ArtifactData &ad1, const ArtifactData &ad2)
{
    return ad1.filePath() == ad2.filePath()
            && ad1.fileTags() == ad2.fileTags()
            && ad1.isGenerated() == ad2.isGenerated()
            && ad1.properties() == ad2.properties();
}

bool operator!=(const ArtifactData &ta1, const ArtifactData &ta2)
{
    return !(ta1 == ta2);
}

bool operator<(const ArtifactData &ta1, const ArtifactData &ta2)
{
    return ta1.filePath() < ta2.filePath();
}


/*!
 * \class InstallData
 * \brief The \c InstallData class provides the installation-related data of an artifact.
 */

InstallData::InstallData() : d(new InstallDataPrivate)
{
}

InstallData::InstallData(const InstallData &other) = default;

InstallData::InstallData(InstallData &&) Q_DECL_NOEXCEPT = default;

InstallData &InstallData::operator=(const InstallData &other) = default;

InstallData &InstallData::operator=(InstallData &&) Q_DECL_NOEXCEPT = default;

InstallData::~InstallData() = default;

/*!
 * \brief Returns true if and only if this object holds data that was initialized by Qbs.
 */
bool InstallData::isValid() const
{
    return d->isValid;
}

QJsonObject InstallData::toJson() const
{
    QJsonObject obj;
    if (isValid()) {
        obj.insert(QStringLiteral("is-installable"), isInstallable());
        if (isInstallable()) {
            obj.insert(QStringLiteral("install-file-path"), installFilePath());
            obj.insert(QStringLiteral("install-root"), installRoot());
        }
    }
    return obj;
}

/*!
  \brief Returns true if and only if \c{qbs.install} is \c true for the artifact.
 */
bool InstallData::isInstallable() const
{
    QBS_ASSERT(isValid(), return false);
    return d->isInstallable;
}

/*!
  \brief Returns the directory into which the artifact will be installed.
  \note This is not necessarily the same as \c{qbs.installDir}, because \c{qbs.installSourceBase}
        might have been used.
 */
QString InstallData::installDir() const
{
    QBS_ASSERT(isValid(), return {});
    return FileInfo::path(installFilePath());
}

/*!
  \brief Returns the installed file path of the artifact.
 */
QString InstallData::installFilePath() const
{
    QBS_ASSERT(isValid(), return {});
    return d->installFilePath;
}

/*!
  \brief Returns the value of \c{qbs.installRoot} for the artifact.
 */
QString InstallData::installRoot() const
{
    QBS_ASSERT(isValid(), return {});
    return d->installRoot;
}

/*!
  \brief Returns the local installation directory of the artifact, that is \c installDir()
         prepended by \c installRoot().
 */
QString InstallData::localInstallDir() const
{
    return QDir::cleanPath(installRoot() + QLatin1Char('/') + installDir());
}

/*!
  \brief Returns the local installed file path of the artifact, that is \c installFilePath()
         prepended by \c installRoot().
 */
QString InstallData::localInstallFilePath() const
{
    return QDir::cleanPath(installRoot() + QLatin1Char('/') + installFilePath());
}

/*!
 * \class ProductData
 * \brief The \c ProductData class corresponds to the Product item in a qbs source file.
 */

ProductData::ProductData() : d(new ProductDataPrivate)
{
}

ProductData::ProductData(const ProductData &other) = default;

ProductData::ProductData(ProductData &&) Q_DECL_NOEXCEPT = default;

ProductData &ProductData::operator=(const ProductData &other) = default;

ProductData &ProductData::operator=(ProductData &&) Q_DECL_NOEXCEPT = default;

ProductData::~ProductData() = default;

/*!
 * \brief Returns true if and only if the Product holds data that was initialized by Qbs.
 */
bool ProductData::isValid() const
{
    return d->isValid;
}

QJsonObject ProductData::toJson(const QStringList &propertyNames) const
{
    QJsonObject obj;
    if (!isValid())
        return obj;
    obj.insert(StringConstants::typeProperty(), QJsonArray::fromStringList(type()));
    obj.insert(StringConstants::dependenciesProperty(),
               QJsonArray::fromStringList(dependencies()));
    obj.insert(StringConstants::nameProperty(), name());
    obj.insert(StringConstants::fullDisplayNameKey(), fullDisplayName());
    obj.insert(QStringLiteral("target-name"), targetName());
    obj.insert(StringConstants::versionProperty(), version());
    obj.insert(QStringLiteral("multiplex-configuration-id"), multiplexConfigurationId());
    obj.insert(StringConstants::locationKey(), location().toJson());
    obj.insert(StringConstants::buildDirectoryKey(), buildDirectory());
    obj.insert(QStringLiteral("generated-artifacts"), toJsonArray(generatedArtifacts(),
                                                                  propertyNames));
    obj.insert(QStringLiteral("target-executable"), targetExecutable());
    QJsonArray groupArray;
    for (const GroupData &g : groups()) {
        const QStringList groupPropNames = g.properties() == moduleProperties()
                ? QStringList() : propertyNames;
        groupArray << g.toJson(groupPropNames);
    }
    obj.insert(QStringLiteral("groups"), groupArray);
    obj.insert(QStringLiteral("properties"), QJsonObject::fromVariantMap(properties()));
    obj.insert(StringConstants::isEnabledKey(), isEnabled());
    obj.insert(QStringLiteral("is-runnable"), isRunnable());
    obj.insert(QStringLiteral("is-multiplexed"), isMultiplexed());
    addModuleProperties(obj, moduleProperties(), propertyNames);
    return obj;
}

/*!
 * \brief The product type, which is the list of file tags matching the product's target artifacts.
 */
const QStringList &ProductData::type() const
{
    return d->type;
}

/*!
 * \brief The names of dependent products.
 */
const QStringList &ProductData::dependencies() const
{
    return d->dependencies;
}

/*!
 * \brief The name of the product as given in the qbs source file.
 */
const QString &ProductData::name() const
{
    return d->name;
}

/*!
  The name of the product as given in the qbs source file, plus information
  about which properties it was multiplexed on and the values of these properties.
  If the product was not multiplexed, the returned value is the same as \c name().
 */
QString ProductData::fullDisplayName() const
{
    return ResolvedProduct::fullDisplayName(name(), multiplexConfigurationId());
}

/*!
 * \brief The base name of the product's target file as given in the qbs source file.
 */
const QString &ProductData::targetName() const
{
    return d->targetName;
}

/*!
 * \brief The version number of the product.
 */
const QString &ProductData::version() const
{
    return d->version;
}

/*!
 * \brief The profile this product will be built for.
 */
QString ProductData::profile() const
{
    return d->moduleProperties.getModuleProperty(
                StringConstants::qbsModule(),
                StringConstants::profileProperty()).toString();
}

const QString &ProductData::multiplexConfigurationId() const
{
   return d->multiplexConfigurationId;
}

/*!
 * \brief The location at which the product is defined in the source file.
 */
const CodeLocation &ProductData::location() const
{
    return d->location;
}

/*!
 * \brief The directory under which the product's generated artifacts are located.
 */
const QString &ProductData::buildDirectory() const
{
    return d->buildDirectory;
}

/*!
 * \brief All artifacts that are generated when building this product.
 */
const QList<ArtifactData> &ProductData::generatedArtifacts() const
{
    return d->generatedArtifacts;
}

/*!
  \brief This product's target artifacts.
  This is a subset of \c generatedArtifacts()
 */
const QList<ArtifactData> ProductData::targetArtifacts() const
{
    QList<ArtifactData> list;
    std::copy_if(d->generatedArtifacts.cbegin(), d->generatedArtifacts.cend(),
                 std::back_inserter(list),
                 [](const ArtifactData &a) { return a.isTargetArtifact(); });
    return list;
}

/*!
 * \brief The list of artifacts in this product that are to be installed.
 */
const QList<ArtifactData> ProductData::installableArtifacts() const
{
    QList<ArtifactData> artifacts;
    for (const GroupData &g : qAsConst(d->groups)) {
        const auto sourceArtifacts = g.allSourceArtifacts();
        for (const ArtifactData &a : sourceArtifacts) {
            if (a.installData().isInstallable())
                artifacts << a;
        }
    }
    for (const ArtifactData &a : qAsConst(d->generatedArtifacts)) {
        if (a.installData().isInstallable())
            artifacts << a;
    }
    return artifacts;
}

/*!
 * \brief Returns the file path of the executable associated with this product.
 * If the product is not an application, an empty string is returned.
 */
QString ProductData::targetExecutable() const
{
    QBS_ASSERT(isValid(), return {});
    if (d->moduleProperties.getModuleProperty(QStringLiteral("bundle"),
                                              QStringLiteral("isBundle")).toBool()) {
        const auto artifacts = targetArtifacts();
        for (const ArtifactData &ta : artifacts) {
            if (ta.fileTags().contains(QLatin1String("bundle.application-executable"))) {
                if (ta.installData().isInstallable())
                    return ta.installData().localInstallFilePath();
                return ta.filePath();
            }
        }
    }
    const auto artifacts = targetArtifacts();
    for (const ArtifactData &ta : artifacts) {
        if (ta.isExecutable()) {
            if (ta.installData().isInstallable())
                return ta.installData().localInstallFilePath();
            return ta.filePath();
        }
    }
    return {};
}

/*!
 * \brief The list of \c GroupData in this product.
 */
const QList<GroupData> &ProductData::groups() const
{
    return d->groups;
}

/*!
 * \brief The product properties.
 */
const QVariantMap &ProductData::properties() const
{
    return d->properties;
}

/*!
 * \brief The set of properties inherited from dependent products and modules.
 */
const PropertyMap &ProductData::moduleProperties() const
{
    return d->moduleProperties;
}

/*!
 * \brief Returns true if this Product is enabled in Qbs.
 * This method returns the \c condition property of the \c Product definition. If a product is
 * enabled, then it will be built in the current configuration.
 * \sa GroupData::isEnabled()
 */
bool ProductData::isEnabled() const
{
    QBS_ASSERT(isValid(), return false);
    return d->isEnabled;
}

bool ProductData::isRunnable() const
{
    QBS_ASSERT(isValid(), return false);
    return d->isRunnable;
}

bool ProductData::isMultiplexed() const
{
    QBS_ASSERT(isValid(), return false);
    return d->isMultiplexed;
}

bool operator==(const ProductData &lhs, const ProductData &rhs)
{
    if (!lhs.isValid() && !rhs.isValid())
        return true;

    return lhs.isValid() == rhs.isValid()
            && lhs.name() == rhs.name()
            && lhs.targetName() == rhs.targetName()
            && lhs.type() == rhs.type()
            && lhs.version() == rhs.version()
            && lhs.dependencies() == rhs.dependencies()
            && lhs.profile() == rhs.profile()
            && lhs.multiplexConfigurationId() == rhs.multiplexConfigurationId()
            && lhs.location() == rhs.location()
            && lhs.groups() == rhs.groups()
            && lhs.generatedArtifacts() == rhs.generatedArtifacts()
            && lhs.properties() == rhs.properties()
            && lhs.moduleProperties() == rhs.moduleProperties()
            && lhs.isEnabled() == rhs.isEnabled()
            && lhs.isMultiplexed() == rhs.isMultiplexed();
}

bool operator!=(const ProductData &lhs, const ProductData &rhs)
{
    return !(lhs == rhs);
}

bool operator<(const ProductData &lhs, const ProductData &rhs)
{
    const int nameCmp = lhs.name().compare(rhs.name());
    if (nameCmp < 0)
        return true;
    if (nameCmp > 0)
        return false;
    return lhs.profile() < rhs.profile()
            && lhs.multiplexConfigurationId() < rhs.multiplexConfigurationId();
}

/*!
 * \class ProjectData
 * \brief The \c ProjectData class corresponds to the \c Project item in a qbs source file.
 */

/*!
 * \fn QList<ProductData> ProjectData::products() const
 * \brief The products in this project.
 */

ProjectData::ProjectData() : d(new ProjectDataPrivate)
{
}

ProjectData::ProjectData(const ProjectData &other) = default;

ProjectData::ProjectData(ProjectData &&) Q_DECL_NOEXCEPT = default;

ProjectData &ProjectData::operator =(const ProjectData &other) = default;

ProjectData &ProjectData::operator=(ProjectData &&) Q_DECL_NOEXCEPT = default;

ProjectData::~ProjectData() = default;

/*!
 * \brief Returns true if and only if the Project holds data that was initialized by Qbs.
 */
bool ProjectData::isValid() const
{
    return d->isValid;
}

QJsonObject ProjectData::toJson(const QStringList &moduleProperties) const
{
    QJsonObject obj;
    if (!isValid())
        return obj;
    obj.insert(StringConstants::nameProperty(), name());
    obj.insert(StringConstants::locationKey(), location().toJson());
    obj.insert(StringConstants::isEnabledKey(), isEnabled());
    obj.insert(StringConstants::productsKey(), toJsonArray(products(), moduleProperties));
    obj.insert(QStringLiteral("sub-projects"), toJsonArray(subProjects(), moduleProperties));
    return obj;
}

/*!
 * \brief The name of this project.
 */
const QString &ProjectData::name() const
{
    return d->name;
}

/*!
 * \brief The location at which the project is defined in a qbs source file.
 */
const CodeLocation &ProjectData::location() const
{
    return d->location;
}

/*!
 * \brief Whether the project is enabled.
 * \note Disabled projects never have any products or sub-projects.
 */
bool ProjectData::isEnabled() const
{
    QBS_ASSERT(isValid(), return false);
    return d->enabled;
}

/*!
 * \brief The base directory under which the build artifacts of this project will be created.
 * This is only valid for the top-level project.
 */
const QString &ProjectData::buildDirectory() const
{
    return d->buildDir;
}

/*!
 * The products in this project.
 * \note This also includes disabled products.
 */
const QList<ProductData> &ProjectData::products() const
{
    return d->products;
}

/*!
 * The sub-projects of this project.
 */
const QList<ProjectData> &ProjectData::subProjects() const
{
    return d->subProjects;
}

/*!
 * All products in this projects and its direct and indirect sub-projects.
 */
const QList<ProductData> ProjectData::allProducts() const
{
    QList<ProductData> productList = products();
    for (const ProjectData &pd : qAsConst(d->subProjects))
        productList << pd.allProducts();
    return productList;
}

/*!
 * The artifacts of all products in this project that are to be installed.
 */
const QList<ArtifactData> ProjectData::installableArtifacts() const
{
    QList<ArtifactData> artifacts;
    const auto products = allProducts();
    for (const ProductData &p : products)
        artifacts << p.installableArtifacts();
    return artifacts;
}

bool operator==(const ProjectData &lhs, const ProjectData &rhs)
{
    if (!lhs.isValid() && !rhs.isValid())
        return true;

    return lhs.isValid() == rhs.isValid()
            && lhs.isEnabled() == rhs.isEnabled()
            && lhs.name() == rhs.name()
            && lhs.buildDirectory() == rhs.buildDirectory()
            && lhs.location() == rhs.location()
            && lhs.subProjects() == rhs.subProjects()
            && lhs.products() == rhs.products();
}

bool operator!=(const ProjectData &lhs, const ProjectData &rhs)
{
    return !(lhs == rhs);
}

bool operator<(const ProjectData &lhs, const ProjectData &rhs)
{
    return lhs.name() < rhs.name();
}

/*!
 * \class PropertyMap
 * \brief The \c PropertyMap class represents the properties of a group or a product.
 */

PropertyMap::PropertyMap()
    : d(std::make_unique<PropertyMapPrivate>())
{
    static PropertyMapPtr defaultInternalMap = PropertyMapInternal::create();
    d->m_map = defaultInternalMap;
}

PropertyMap::PropertyMap(const PropertyMap &other)
    : d(std::make_unique<PropertyMapPrivate>(*other.d))
{
}

PropertyMap::PropertyMap(PropertyMap &&other) Q_DECL_NOEXCEPT = default;

PropertyMap::~PropertyMap() = default;

PropertyMap &PropertyMap::operator =(const PropertyMap &other)
{
    if (this != &other)
        d = std::make_unique<PropertyMapPrivate>(*other.d);
    return *this;
}

PropertyMap &PropertyMap::operator =(PropertyMap &&other) Q_DECL_NOEXCEPT = default;

/*!
 * \brief Returns the names of all properties.
 */
QStringList PropertyMap::allProperties() const
{
    QStringList properties;
    for (QVariantMap::ConstIterator it = d->m_map->value().constBegin();
            it != d->m_map->value().constEnd(); ++it) {
        if (!it.value().canConvert<QVariantMap>())
            properties << it.key();
    }
    return properties;
}

/*!
 * \brief Returns the value of the given property of a product or group.
 */
QVariant PropertyMap::getProperty(const QString &name) const
{
    return d->m_map->value().value(name);
}

/*!
 * \brief Convenience wrapper around \c PropertyMap::getModuleProperty for properties of list type.
 *
 */
QStringList PropertyMap::getModulePropertiesAsStringList(const QString &moduleName,
                                                          const QString &propertyName) const
{
    const QVariantList &vl = d->m_map->moduleProperty(moduleName, propertyName).toList();
    QStringList sl;
    for (const QVariant &v : vl) {
        QBS_ASSERT(v.canConvert<QString>(), continue);
        sl << v.toString();
    }
    return sl;
}

/*!
 * \brief Returns the value of the given module property.
 */
QVariant PropertyMap::getModuleProperty(const QString &moduleName,
                                        const QString &propertyName) const
{
    return d->m_map->moduleProperty(moduleName, propertyName);
}

static QString mapToString(const QVariantMap &map, const QString &prefix)
{
    QStringList keys(map.keys());
    std::sort(keys.begin(), keys.end());
    QString stringRep;
    for (const QString &key : qAsConst(keys)) {
        const QVariant &val = map.value(key);
        if (val.type() == QVariant::Map) {
            stringRep += mapToString(val.value<QVariantMap>(), prefix + key + QLatin1Char('.'));
        } else {
            stringRep += QStringLiteral("%1%2: %3\n")
                    .arg(prefix, key, toJSLiteral(val));
        }
    }
    return stringRep;
}

QString PropertyMap::toString() const
{
    return mapToString(d->m_map->value(), QString());
}

bool operator==(const PropertyMap &pm1, const PropertyMap &pm2)
{
    return *pm1.d->m_map == *pm2.d->m_map;
}

bool operator!=(const PropertyMap &pm1, const PropertyMap &pm2)
{
    return !(*pm1.d->m_map == *pm2.d->m_map);
}

} // namespace qbs
