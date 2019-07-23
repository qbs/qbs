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

#include "armarchiversettingsgroup_v8.h"
#include "armassemblersettingsgroup_v8.h"
#include "armbuildconfigurationgroup_v8.h"
#include "armcompilersettingsgroup_v8.h"
#include "armgeneralsettingsgroup_v8.h"
#include "armlinkersettingsgroup_v8.h"

#include "../../iarewtoolchainpropertygroup.h"
#include "../../iarewutils.h"

namespace qbs {
namespace iarew {
namespace arm {
namespace v8 {

ArmBuildConfigurationGroup::ArmBuildConfigurationGroup(
        const qbs::Project &qbsProject,
        const qbs::ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps)
    : gen::xml::PropertyGroup("configuration")
{
    // Append configuration name item.
    const QString cfgName = gen::utils::buildConfigurationName(qbsProject);
    appendProperty("name", cfgName);

    // Apend toolchain name group item.
    appendChild<IarewToolchainPropertyGroup>("ARM");

    // Append debug info item.
    const int debugBuild = gen::utils::debugInformation(qbsProduct);
    appendProperty("debug", debugBuild);

    // Append settings group items.
    appendChild<ArmArchiverSettingsGroup>(
                qbsProject, qbsProduct, qbsProductDeps);
    appendChild<ArmAssemblerSettingsGroup>(
                qbsProject, qbsProduct, qbsProductDeps);
    appendChild<ArmCompilerSettingsGroup>(
                qbsProject, qbsProduct, qbsProductDeps);
    appendChild<ArmGeneralSettingsGroup>(
                qbsProject, qbsProduct, qbsProductDeps);
    appendChild<ArmLinkerSettingsGroup>(
                qbsProject, qbsProduct, qbsProductDeps);
}

bool ArmBuildConfigurationGroupFactory::canCreate(
        gen::utils::Architecture arch,
        const Version &version) const
{
    return arch == gen::utils::Architecture::Arm
            && version.majorVersion() == 8;
}

std::unique_ptr<gen::xml::PropertyGroup>
ArmBuildConfigurationGroupFactory::create(
        const qbs::Project &qbsProject,
        const qbs::ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps) const
{
    const auto group = new ArmBuildConfigurationGroup(
                qbsProject, qbsProduct, qbsProductDeps);
    return std::unique_ptr<ArmBuildConfigurationGroup>(group);
}

} // namespace v8
} // namespace arm
} // namespace iarew
} // namespace qbs
