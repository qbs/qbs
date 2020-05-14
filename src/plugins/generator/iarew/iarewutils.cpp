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

#include <generators/generatorutils.h>

namespace qbs {
namespace IarewUtils {

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

QString libToolkitRootPath(const ProductData &qbsProduct)
{
    return toolkitRootPath(qbsProduct) + QLatin1String("/lib");
}

QString toolkitRelativeFilePath(const QString &basePath,
                                const QString &fullFilePath)
{
    return QLatin1String("$TOOLKIT_DIR$/")
            + gen::utils::relativeFilePath(basePath, fullFilePath);
}

QString projectRelativeFilePath(const QString &basePath,
                                const QString &fullFilePath)
{
    return QLatin1String("$PROJ_DIR$/")
            + gen::utils::relativeFilePath(basePath, fullFilePath);
}

OutputBinaryType outputBinaryType(const ProductData &qbsProduct)
{
    const auto &qbsProductType = qbsProduct.type();
    if (qbsProductType.contains(QLatin1String("application")))
        return ApplicationOutputType;
    if (qbsProductType.contains(QLatin1String("staticlibrary")))
        return LibraryOutputType;
    return ApplicationOutputType;
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
    return gen::utils::cppStringModuleProperties(
                qbsProps, {QStringLiteral("driverFlags"), QStringLiteral("cFlags"),
                           QStringLiteral("cppFlags"), QStringLiteral("cxxFlags"),
                           QStringLiteral("commonCompilerFlags")});
}

QStringList cppModuleAssemblerFlags(const PropertyMap &qbsProps)
{
    return gen::utils::cppStringModuleProperties(
                qbsProps, {QStringLiteral("assemblerFlags")});
}

QStringList cppModuleLinkerFlags(const PropertyMap &qbsProps)
{
    return gen::utils::cppStringModuleProperties(
                qbsProps, {QStringLiteral("driverFlags"),
                           QStringLiteral("driverLinkerFlags")});
}

} // namespace IarewUtils
} // namespace qbs
