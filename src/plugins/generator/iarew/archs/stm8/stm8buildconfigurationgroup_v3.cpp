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

#include "stm8archiversettingsgroup_v3.h"
#include "stm8assemblersettingsgroup_v3.h"
#include "stm8buildconfigurationgroup_v3.h"
#include "stm8compilersettingsgroup_v3.h"
#include "stm8generalsettingsgroup_v3.h"
#include "stm8linkersettingsgroup_v3.h"

#include "../../iarewtoolchainpropertygroup.h"
#include "../../iarewutils.h"

namespace qbs {
namespace iarew {
namespace stm8 {
namespace v3 {

Stm8BuildConfigurationGroup::Stm8BuildConfigurationGroup(
        const Project &qbsProject,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps)
    : gen::xml::PropertyGroup("configuration")
{
    // Append configuration name item.
    const QString cfgName = gen::utils::buildConfigurationName(qbsProject);
    appendProperty("name", cfgName);

    // Apend toolchain name group item.
    appendChild<IarewToolchainPropertyGroup>("STM8");

    // Append debug info item.
    const int debugBuild = gen::utils::debugInformation(qbsProduct);
    appendProperty("debug", debugBuild);

    // Append settings group items.
    appendChild<Stm8ArchiverSettingsGroup>(
                qbsProject, qbsProduct, qbsProductDeps);
    appendChild<Stm8AssemblerSettingsGroup>(
                qbsProject, qbsProduct, qbsProductDeps);
    appendChild<Stm8CompilerSettingsGroup>(
                qbsProject, qbsProduct, qbsProductDeps);
    appendChild<Stm8GeneralSettingsGroup>(
                qbsProject, qbsProduct, qbsProductDeps);
    appendChild<Stm8LinkerSettingsGroup>(
                qbsProject, qbsProduct, qbsProductDeps);
}

bool Stm8BuildConfigurationGroupFactory::canCreate(
        gen::utils::Architecture arch,
        const Version &version) const
{
    return arch == gen::utils::Architecture::Stm8
            && version.majorVersion() == 3;
}

std::unique_ptr<gen::xml::PropertyGroup>
Stm8BuildConfigurationGroupFactory::create(
        const Project &qbsProject,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps) const
{
    const auto group = new Stm8BuildConfigurationGroup(
                qbsProject, qbsProduct, qbsProductDeps);
    return std::unique_ptr<Stm8BuildConfigurationGroup>(group);
}

} // namespace v3
} // namespace stm8
} // namespace iarew
} // namespace qbs
