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

#ifndef GENERATORS_UTILS_H
#define GENERATORS_UTILS_H

#include <qbs.h>

#include <tools/qbs_export.h>
#include <tools/stringconstants.h>

namespace qbs {
namespace gen {
namespace utils {

enum class Architecture {
    Unknown = 0,
    Arm = 1 << 1,
    Avr = 1 << 2,
    Mcs51 = 1 << 3,
    Stm8 = 1 << 4,
    Msp430 = 1 << 5
};

Q_DECLARE_FLAGS(ArchitectureFlags, Architecture)
Q_DECLARE_OPERATORS_FOR_FLAGS(ArchitectureFlags)

QBS_EXPORT QString architectureName(Architecture arch);
QBS_EXPORT Architecture architecture(const Project &qbsProject);
QBS_EXPORT QString buildConfigurationName(const Project &qbsProject);
QBS_EXPORT int debugInformation(const ProductData &qbsProduct);
QBS_EXPORT QString buildRootPath(const Project &qbsProject);
QBS_EXPORT QString relativeFilePath(const QString &baseDirectory,
                                    const QString &fullFilePath);
QBS_EXPORT QString binaryOutputDirectory(const QString &baseDirectory,
                                         const ProductData &qbsProduct);
QBS_EXPORT QString objectsOutputDirectory(const QString &baseDirectory,
                                          const ProductData &qbsProduct);
QBS_EXPORT QString listingOutputDirectory(const QString &baseDirectory,
                                          const ProductData &qbsProduct);
QBS_EXPORT std::vector<ProductData> dependenciesOf(const ProductData &qbsProduct,
                                                   const GeneratableProject &genProject,
                                                   const QString &configurationName);
QBS_EXPORT QString targetBinary(const ProductData &qbsProduct);
QBS_EXPORT QString targetBinaryPath(const QString &baseDirectory,
                                    const ProductData &qbsProduct);
QBS_EXPORT QString cppStringModuleProperty(const PropertyMap &qbsProps,
                                           const QString &propertyName);
QBS_EXPORT bool cppBooleanModuleProperty(const PropertyMap &qbsProps,
                                         const QString &propertyName);
QBS_EXPORT int cppIntegerModuleProperty(const PropertyMap &qbsProps,
                                        const QString &propertyName);
QBS_EXPORT QStringList cppStringModuleProperties(const PropertyMap &qbsProps,
                                                 const QStringList &propertyNames);
QBS_EXPORT QVariantList cppVariantModuleProperties(const PropertyMap &qbsProps,
                                                   const QStringList &propertyNames);
QBS_EXPORT QString firstFlagValue(const QStringList &flags,
                                  const QString &flagKey);
QBS_EXPORT QStringList allFlagValues(const QStringList &flags,
                                     const QString &flagKey);

template <typename T>
bool inBounds(const T &value, const T &low, const T &high)
{
    return !(value < low) && !(high < value);
}

} // namespace utils
} // namespace gen
} // namespace qbs

#endif // GENERATORS_UTILS_H
