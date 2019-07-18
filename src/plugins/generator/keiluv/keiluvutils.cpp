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

#include "keiluvutils.h"

namespace qbs {
namespace KeiluvUtils {

OutputBinaryType outputBinaryType(const ProductData &qbsProduct)
{
    const auto qbsProductType = qbsProduct.type();
    if (qbsProductType.contains(QLatin1String("application")))
        return ApplicationOutputType;
    if (qbsProductType.contains(QLatin1String("staticlibrary")))
        return LibraryOutputType;
    return ApplicationOutputType;
}

QString architectureName(Architecture arch)
{
    switch (arch) {
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

QString binaryOutputDirectory(const QString &baseDirectory,
                              const ProductData &qbsProduct)
{
    const auto path = QDir(baseDirectory).relativeFilePath(
            qbsProduct.buildDirectory()) + QLatin1String("/bin");
    return QDir::toNativeSeparators(path);
}

QString objectsOutputDirectory(const QString &baseDirectory,
                               const ProductData &qbsProduct)
{
    const auto path = QDir(baseDirectory).relativeFilePath(
                qbsProduct.buildDirectory()) + QLatin1String("/obj");
    return QDir::toNativeSeparators(path);
}

QString listingOutputDirectory(const QString &baseDirectory,
                               const ProductData &qbsProduct)
{
    const auto path = QDir(baseDirectory).relativeFilePath(
                qbsProduct.buildDirectory()) + QLatin1String("/lst");
    return QDir::toNativeSeparators(path);
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

QString toolkitRootPath(const ProductData &qbsProduct)
{
    QDir dir(qbsProduct.moduleProperties()
             .getModuleProperty(Internal::StringConstants::cppModule(),
                                QStringLiteral("toolchainInstallPath"))
             .toString());
    dir.cdUp();
    const auto path = dir.absolutePath();
    return QDir::toNativeSeparators(path);
}

QString buildRootPath(const Project &qbsProject)
{
    QDir dir(qbsProject.projectData().buildDirectory());
    dir.cdUp();
    const auto path = dir.absolutePath();
    return QDir::toNativeSeparators(path);
}

QString relativeFilePath(const QString &baseDirectory,
                         const QString &fullFilePath)
{
    const auto path = QDir(baseDirectory).relativeFilePath(fullFilePath);
    return QDir::toNativeSeparators(path);
}

QString cppStringModuleProperty(const PropertyMap &qbsProps,
                                const QString &propertyName)
{
    return qbsProps.getModuleProperty(Internal::StringConstants::cppModule(),
                                      propertyName).toString().trimmed();
}

QStringList cppStringModuleProperties(const PropertyMap &qbsProps,
                                      const QStringList &propertyNames)
{
    QStringList properties;
    for (const auto &propertyName : propertyNames) {
        properties << qbsProps.getModuleProperty(
                          Internal::StringConstants::cppModule(),
                          propertyName).toStringList();
    }
    std::transform(properties.begin(), properties.end(), properties.begin(),
                   [](const auto &property) {
        return property.trimmed();
    });
    return properties;
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
                qbsProps, {QStringLiteral("driverLinkerFlags")});
}

QStringList includes(const PropertyMap &qbsProps)
{
    auto paths = qbs::KeiluvUtils::cppStringModuleProperties(
                qbsProps, {QStringLiteral("includePaths"),
                           QStringLiteral("systemIncludePaths")});
    // Transform include path separators to native.
    std::transform(paths.begin(), paths.end(), paths.begin(),
                   [](const auto &path) {
        return QDir::toNativeSeparators(path);
    });
    return paths;
}

QStringList defines(const PropertyMap &qbsProps)
{
    return qbs::KeiluvUtils::cppStringModuleProperties(
                qbsProps, {QStringLiteral("defines")});
}

QStringList staticLibraries(const PropertyMap &qbsProps)
{
    auto libs = qbs::KeiluvUtils::cppStringModuleProperties(
                qbsProps, {QStringLiteral("staticLibraries")});
    // Transform library path separators to native.
    std::transform(libs.begin(), libs.end(), libs.begin(),
                   [](const auto &path) {
        return QDir::toNativeSeparators(path);
    });
    return libs;
}

QStringList dependencies(const std::vector<ProductData> &qbsProductDeps)
{
    QStringList deps;
    for (const ProductData &qbsProductDep : qbsProductDeps) {
        const auto path = qbsProductDep.buildDirectory()
                + QLatin1String("/obj/")
                + qbs::KeiluvUtils::targetBinary(qbsProductDep);
        deps.push_back(QDir::toNativeSeparators(path));
    }
    return deps;
}

} // namespace KeiluvUtils
} // namespace qbs
