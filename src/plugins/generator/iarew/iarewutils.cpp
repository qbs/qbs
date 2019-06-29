/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "iarewutils.h"

namespace qbs {
namespace IarewUtils {

QString architectureName(Architecture arch)
{
    switch (arch) {
    case Architecture::ArmArchitecture:
        return QStringLiteral("arm");
    case Architecture::AvrArchitecture:
        return QStringLiteral("avr");
    case Architecture::Mcs51Architecture:
        return QStringLiteral("mcs51");
    default:
        return QStringLiteral("unknown");
    }
}

Architecture architecture(const Project &qbsProject)
{
    const auto qbsArch = qbsProject.projectConfiguration()
            .value(Internal::StringConstants::qbsModule()).toMap()
            .value(QStringLiteral("architecture")).toString();

    if (qbsArch == QLatin1String("arm"))
        return Architecture::ArmArchitecture;
    if (qbsArch == QLatin1String("avr"))
        return Architecture::AvrArchitecture;
    if (qbsArch == QLatin1String("mcs51"))
        return Architecture::Mcs51Architecture;
    return Architecture::UnknownArchitecture;
}

QString buildConfigurationName(const Project &qbsProject)
{
    return qbsProject.projectConfiguration()
            .value(Internal::StringConstants::qbsModule()).toMap()
            .value(QStringLiteral("configurationName")).toString();
}

int debugInformation(const ProductData &qbsProduct)
{
    return qbsProduct.moduleProperties().getModuleProperty(
                Internal::StringConstants::qbsModule(),
                QStringLiteral("debugInformation"))
            .toInt();
}

QString toolkitRootPath(const ProductData &qbsProduct)
{
    QDir dir(qbsProduct.moduleProperties()
             .getModuleProperty(Internal::StringConstants::cppModule(),
                                QStringLiteral("toolchainInstallPath"))
             .toString());
    dir.cdUp();
    return dir.absolutePath();
}

QString dlibToolkitRootPath(const ProductData &qbsProduct)
{
    return toolkitRootPath(qbsProduct) + QLatin1String("/lib/dlib");
}

QString clibToolkitRootPath(const ProductData &qbsProduct)
{
    return toolkitRootPath(qbsProduct) + QLatin1String("/lib/clib");
}

QString buildRootPath(const Project &qbsProject)
{
    QDir dir(qbsProject.projectData().buildDirectory());
    dir.cdUp();
    return dir.absolutePath();
}

QString relativeFilePath(const QString &baseDirectory,
                         const QString &fullFilePath)
{
    return QDir(baseDirectory).relativeFilePath(fullFilePath);
}

QString toolkitRelativeFilePath(const QString &basePath,
                                const QString &fullFilePath)
{
    return QLatin1String("$TOOLKIT_DIR$/")
            + IarewUtils::relativeFilePath(basePath, fullFilePath);
}

QString projectRelativeFilePath(const QString &basePath,
                                const QString &fullFilePath)
{
    return QLatin1String("$PROJ_DIR$/")
            + IarewUtils::relativeFilePath(basePath, fullFilePath);
}

QString binaryOutputDirectory(const QString &baseDirectory,
                              const ProductData &qbsProduct)
{
    return QDir(baseDirectory).relativeFilePath(qbsProduct.buildDirectory())
            + QLatin1String("/bin");
}

QString objectsOutputDirectory(const QString &baseDirectory,
                               const ProductData &qbsProduct)
{
    return QDir(baseDirectory).relativeFilePath(qbsProduct.buildDirectory())
            + QLatin1String("/obj");
}

QString listingOutputDirectory(const QString &baseDirectory,
                               const ProductData &qbsProduct)
{
    return QDir(baseDirectory).relativeFilePath(qbsProduct.buildDirectory())
            + QLatin1String("/lst");
}

std::vector<ProductData> dependenciesOf(const ProductData &qbsProduct,
                                        const GeneratableProject &genProject,
                                        const QString configurationName)
{
    std::vector<ProductData> result;
    const auto depsNames = qbsProduct.dependencies();
    for (const auto &product : qAsConst(genProject.products)) {
        const auto pt = product.type();
        if (!pt.contains(QLatin1String("staticlibrary")))
            continue;
        const auto pn = product.name();
        if (!depsNames.contains(pn))
            continue;
        result.push_back(product.data.value(configurationName));
    }
    return result;
}

QString targetBinary(const ProductData &qbsProduct)
{
    const auto type = qbsProduct.type();
    if (type.contains(QLatin1String("application"))) {
        return QFileInfo(qbsProduct.targetExecutable()).fileName();
    } else if (type.contains(QLatin1String("staticlibrary"))) {
        const auto artifacts = qbsProduct.targetArtifacts();
        for (const auto &artifact : artifacts) {
            if (artifact.fileTags().contains(QLatin1String("staticlibrary")))
                return QFileInfo(artifact.filePath()).fileName();
        }
    }

    return {};
}

QString targetBinaryPath(const QString &baseDirectory,
                         const ProductData &qbsProduct)
{
    return binaryOutputDirectory(baseDirectory, qbsProduct)
            + QLatin1Char('/') + targetBinary(qbsProduct);
}

OutputBinaryType outputBinaryType(const ProductData &qbsProduct)
{
    const auto qbsProductType = qbsProduct.type();
    if (qbsProductType.contains(QLatin1String("application")))
        return ApplicationOutputType;
    if (qbsProductType.contains(QLatin1String("staticlibrary")))
        return LibraryOutputType;
    return ApplicationOutputType;
}

QString cppStringModuleProperty(const PropertyMap &qbsProps,
                                const QString &propertyName)
{
    return qbsProps.getModuleProperty(Internal::StringConstants::cppModule(),
                                      propertyName).toString();
}

bool cppBooleanModuleProperty(const PropertyMap &qbsProps,
                              const QString &propertyName)
{
    return qbsProps.getModuleProperty(Internal::StringConstants::cppModule(),
                                      propertyName).toBool();
}

int cppIntegerModuleProperty(const PropertyMap &qbsProps,
                             const QString &propertyName)
{
    return qbsProps.getModuleProperty(Internal::StringConstants::cppModule(),
                                      propertyName).toInt();
}

QStringList cppStringModuleProperties(const PropertyMap &qbsProps,
                                      const QStringList &propertyNames)
{
    QStringList properties;
    for (const auto &propertyName : propertyNames) {
        properties << qbsProps.getModuleProperty(Internal::StringConstants::cppModule(),
                                                 propertyName).toStringList();
    }
    return properties;
}

QVariantList cppVariantModuleProperties(const PropertyMap &qbsProps,
                                        const QStringList &propertyNames)
{
    QVariantList properties;
    for (const auto &propertyName : propertyNames) {
        properties << qbsProps.getModuleProperty(Internal::StringConstants::cppModule(),
                                                 propertyName).toList();
    }
    return properties;
}

QString flagValue(const QStringList &flags, const QString &flagKey)
{
    // Seach for full 'flagKey' option matching.
    const auto flagBegin = flags.cbegin();
    const auto flagEnd = flags.cend();
    auto flagIt = std::find_if(flagBegin, flagEnd, [flagKey](const QString &flag) {
        return flag == flagKey;
    });
    if (flagIt == flagEnd) {
        // Search for start/end of 'flagKey' matching.
        flagIt = std::find_if(flagBegin, flagEnd, [flagKey](const QString &flag) {
            return flag.startsWith(flagKey) || flag.endsWith(flagKey);
        });
        if (flagIt == flagEnd)
            return {};
    }

    QString value;
    // Check that option is in form of 'flagKey=<flagValue>'.
    if (flagIt->contains(QLatin1Char('='))) {
        value = flagIt->split(QLatin1Char('=')).at(1).trimmed();
    } else if (flagKey.count() < flagIt->count()) {
        // In this case an option is in form of 'flagKey<flagValue>'.
        value = flagIt->mid(flagKey.count()).trimmed();
    } else {
        // In this case an option is in form of 'flagKey <flagValue>'.
        ++flagIt;
        if (flagIt < flagEnd)
            value = (*flagIt).trimmed();
        else
            return {};
    }
    return value;
}

QVariantList flagValues(const QStringList &flags, const QString &flagKey)
{
    QVariantList values;
    for (auto flagIt = flags.cbegin(); flagIt < flags.cend(); ++flagIt) {
        if (*flagIt != flagKey)
            continue;
        ++flagIt;
        values.push_back(*flagIt);
    }
    return values;
}

QStringList cppModuleCompilerFlags(const PropertyMap &qbsProps)
{
    return cppStringModuleProperties(
                qbsProps, {QStringLiteral("driverFlags"), QStringLiteral("cFlags"),
                           QStringLiteral("cppFlags"), QStringLiteral("cxxFlags"),
                           QStringLiteral("commonCompilerFlags")});
}

QStringList cppModuleAssemblerFlags(const PropertyMap &qbsProps)
{
    return cppStringModuleProperties(
                qbsProps, {QStringLiteral("assemblerFlags")});
}

QStringList cppModuleLinkerFlags(const PropertyMap &qbsProps)
{
    return cppStringModuleProperties(
                qbsProps, {QStringLiteral("driverFlags"),
                           QStringLiteral("driverLinkerFlags")});
}

} // namespace IarewUtils
} // namespace qbs
