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

#include "armbuildtargetgroup_v5.h"
#include "armcommonpropertygroup_v5.h"
#include "armdebugoptiongroup_v5.h"
#include "armdlloptiongroup_v5.h"
#include "armtargetcommonoptionsgroup_v5.h"
#include "armtargetgroup_v5.h"
#include "armutilitiesgroup_v5.h"

#include "../../keiluvconstants.h"
#include "../../keiluvfilesgroupspropertygroup.h"

namespace qbs {
namespace keiluv {
namespace arm {
namespace v5 {

ArmBuildTargetGroup::ArmBuildTargetGroup(
        const qbs::Project &qbsProject,
        const qbs::ProductData &qbsProduct,
        const std::vector<qbs::ProductData> &qbsProductDeps)
    : gen::xml::PropertyGroup("Target")
{
    // Append target name item (it is a build configuration name).
    const QString targetName = gen::utils::buildConfigurationName(
                qbsProject);
    appendProperty(QByteArrayLiteral("TargetName"), targetName);
    // Append toolset number group item.
    appendChild<gen::xml::Property>(QByteArrayLiteral("ToolsetNumber"),
                                    QByteArrayLiteral("0x4"));
    // Append toolset name group item.
    appendChild<gen::xml::Property>(QByteArrayLiteral("ToolsetName"),
                                    QByteArrayLiteral("ARM-ADS"));

    // Append target option group item.
    const auto targetOptionGroup = appendChild<gen::xml::PropertyGroup>(
                QByteArrayLiteral("TargetOption"));

    targetOptionGroup->appendChild<ArmTargetCommonOptionsGroup>(
                qbsProject, qbsProduct);
    targetOptionGroup->appendChild<ArmCommonPropertyGroup>(
                qbsProject, qbsProduct);
    targetOptionGroup->appendChild<ArmDllOptionGroup>(
                qbsProject, qbsProduct);
    targetOptionGroup->appendChild<ArmDebugOptionGroup>(
                qbsProject, qbsProduct);
    targetOptionGroup->appendChild<ArmUtilitiesGroup>(
                qbsProject, qbsProduct);
    targetOptionGroup->appendChild<ArmTargetGroup>(
                qbsProject, qbsProduct);

    // Append files group.
    appendChild<KeiluvFilesGroupsPropertyGroup>(qbsProject, qbsProduct,
                                                qbsProductDeps);
}

bool ArmBuildTargetGroupFactory::canCreate(
        gen::utils::Architecture arch,
        const Version &version) const
{
    return arch == gen::utils::Architecture::Arm
            && version.majorVersion() == qbs::KeiluvConstants::v5::kUVisionVersion;
}

std::unique_ptr<gen::xml::PropertyGroup>
ArmBuildTargetGroupFactory::create(
        const qbs::Project &qbsProject,
        const qbs::ProductData &qbsProduct,
        const std::vector<qbs::ProductData> &qbsProductDeps) const
{
    const auto group = new ArmBuildTargetGroup(
                qbsProject, qbsProduct, qbsProductDeps);
    return std::unique_ptr<ArmBuildTargetGroup>(group);
}

} // namespace v5
} // namespace arm
} // namespace keiluv
} // namespace qbs
