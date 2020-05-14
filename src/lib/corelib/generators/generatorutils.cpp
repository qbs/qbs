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

#include "generatorutils.h"

namespace qbs {
namespace gen {
namespace utils {

QString architectureName(Architecture arch)
{
    switch (arch) {
    case Architecture::Arm:
        return QStringLiteral("arm");
    case Architecture::Avr:
        return QStringLiteral("avr");
    case Architecture::Mcs51:
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
        return Architecture::Arm;
    if (qbsArch == QLatin1String("avr"))
        return Architecture::Avr;
    if (qbsArch == QLatin1String("mcs51"))
        return Architecture::Mcs51;
    if (qbsArch == QLatin1String("stm8"))
        return Architecture::Stm8;
    if (qbsArch == QLatin1String("msp430"))
        return Architecture::Msp430;
    return Architecture::Unknown;
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

QString binaryOutputDirectory(const QString &baseDirectory,
                              const ProductData &qbsProduct)
{
    return QDir(baseDirectory).relativeFilePath(
                qbsProduct.buildDirectory())
            + QLatin1String("/bin");
}

QString objectsOutputDirectory(const QString &baseDirectory,
                               const ProductData &qbsProduct)
{
    return QDir(baseDirectory).relativeFilePath(
                qbsProduct.buildDirectory())
            + QLatin1String("/obj");
}

QString listingOutputDirectory(const QString &baseDirectory,
                               const ProductData &qbsProduct)
{
    return QDir(baseDirectory).relativeFilePath(
                qbsProduct.buildDirectory())
            + QLatin1String("/lst");
}

std::vector<ProductData> dependenciesOf(const ProductData &qbsProduct,
                                        const GeneratableProject &genProject,
                                        const QString &configurationName)
{
    std::vector<ProductData> result;
    const auto &depsNames = qbsProduct.dependencies();
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
    const auto &type = qbsProduct.type();
    if (type.contains(QLatin1String("application"))) {
        return QFileInfo(qbsProduct.targetExecutable()).fileName();
    } else if (type.contains(QLatin1String("staticlibrary"))) {
        for (const auto &artifact : qbsProduct.targetArtifacts()) {
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

QString cppStringModuleProperty(const PropertyMap &qbsProps,
                                const QString &propertyName)
{
    return qbsProps.getModuleProperty(
                Internal::StringConstants::cppModule(),
                propertyName).toString().trimmed();
}

bool cppBooleanModuleProperty(const PropertyMap &qbsProps,
                              const QString &propertyName)
{
    return qbsProps.getModuleProperty(
                Internal::StringConstants::cppModule(),
                propertyName).toBool();
}

int cppIntegerModuleProperty(const PropertyMap &qbsProps,
                             const QString &propertyName)
{
    return qbsProps.getModuleProperty(
                Internal::StringConstants::cppModule(),
                propertyName).toInt();
}

QStringList cppStringModuleProperties(const PropertyMap &qbsProps,
                                      const QStringList &propertyNames)
{
    QStringList properties;
    for (const auto &propertyName : propertyNames) {
        const auto entries = qbsProps.getModuleProperty(
                    Internal::StringConstants::cppModule(),
                    propertyName).toStringList();
        for (const auto &entry : entries)
            properties.push_back(entry.trimmed());
    }
    return properties;
}

QVariantList cppVariantModuleProperties(const PropertyMap &qbsProps,
                                        const QStringList &propertyNames)
{
    QVariantList properties;
    for (const auto &propertyName : propertyNames) {
        properties << qbsProps.getModuleProperty(
                          Internal::StringConstants::cppModule(),
                          propertyName).toList();
    }
    return properties;
}

static QString parseFlagValue(const QString &flagKey,
                              QStringList::const_iterator &flagIt,
                              const QStringList::const_iterator &flagEnd)
{
    if (flagIt->contains(QLatin1Char('='))) {
        // In this case an option is in form of 'flagKey=<flagValue>'.
        const auto parts = flagIt->split(QLatin1Char('='));
        if (parts.count() == 2)
            return parts.at(1).trimmed();
    } else if (flagKey < *flagIt) {
        // In this case an option is in form of 'flagKey<flagValue>'.
        return flagIt->mid(flagKey.count()).trimmed();
    } else {
        // In this case an option is in form of 'flagKey <flagValue>'.
        ++flagIt;
        if (flagIt < flagEnd && !flagIt->startsWith(QLatin1Char('-')))
            return (*flagIt).trimmed();
    }
    return {};
}

QString firstFlagValue(const QStringList &flags, const QString &flagKey)
{
    const auto flagBegin = flags.cbegin();
    const auto flagEnd = flags.cend();
    auto flagIt = std::find_if(flagBegin, flagEnd, [flagKey](const QString &flag) {
        return flag == flagKey || flag.startsWith(flagKey);
    });
    if (flagIt == flagEnd)
        return {};
    return parseFlagValue(flagKey, flagIt, flagEnd);
}

QStringList allFlagValues(const QStringList &flags, const QString &flagKey)
{
    QStringList values;
    const auto flagEnd = flags.cend();
    for (auto flagIt = flags.cbegin(); flagIt < flagEnd; ++flagIt) {
        if (*flagIt == flagKey || flagIt->startsWith(flagKey)) {
            const QString value = parseFlagValue(flagKey, flagIt, flagEnd);
            if (!value.isEmpty())
                values.push_back(value);
        }
    }
    return values;
}

} // namespace utils
} // namespace gen
} // namespace qbs
