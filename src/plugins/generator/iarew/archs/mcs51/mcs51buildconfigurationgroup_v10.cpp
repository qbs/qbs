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

#include "mcs51archiversettingsgroup_v10.h"
#include "mcs51assemblersettingsgroup_v10.h"
#include "mcs51buildconfigurationgroup_v10.h"
#include "mcs51compilersettingsgroup_v10.h"
#include "mcs51generalsettingsgroup_v10.h"
#include "mcs51linkersettingsgroup_v10.h"

#include "../../iarewtoolchainpropertygroup.h"
#include "../../iarewutils.h"

namespace qbs {
namespace iarew {
namespace mcs51 {
namespace v10 {

Mcs51BuildConfigurationGroup::Mcs51BuildConfigurationGroup(
        const Project &qbsProject,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps)
    : gen::xml::PropertyGroup("configuration")
{
    // Append configuration name item.
    const QString cfgName = gen::utils::buildConfigurationName(qbsProject);
    appendProperty("name", cfgName);

    // Apend toolchain name group item.
    appendChild<IarewToolchainPropertyGroup>("8051");

    // Append debug info item.
    const int debugBuild = gen::utils::debugInformation(qbsProduct);
    appendProperty("debug", debugBuild);

    // Append settings group items.
    appendChild<Mcs51ArchiverSettingsGroup>(
                qbsProject, qbsProduct, qbsProductDeps);
    appendChild<Mcs51AssemblerSettingsGroup>(
                qbsProject, qbsProduct, qbsProductDeps);
    appendChild<Mcs51CompilerSettingsGroup>(
                qbsProject, qbsProduct, qbsProductDeps);
    appendChild<Mcs51GeneralSettingsGroup>(
                qbsProject, qbsProduct, qbsProductDeps);
    appendChild<Mcs51LinkerSettingsGroup>(
                qbsProject, qbsProduct, qbsProductDeps);
}

bool Mcs51BuildConfigurationGroupFactory::canCreate(
        gen::utils::Architecture arch,
        const Version &version) const
{
    return arch == gen::utils::Architecture::Mcs51
            && version.majorVersion() == 10;
}

std::unique_ptr<gen::xml::PropertyGroup>
Mcs51BuildConfigurationGroupFactory::create(
        const Project &qbsProject,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps) const
{
    const auto group = new Mcs51BuildConfigurationGroup(
                qbsProject, qbsProduct, qbsProductDeps);
    return std::unique_ptr<Mcs51BuildConfigurationGroup>(group);
}

} // namespace v10
} // namespace mcs51
} // namespace iarew
} // namespace qbs
