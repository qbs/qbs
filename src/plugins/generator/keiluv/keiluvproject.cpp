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

#include "keiluvconstants.h"
#include "keiluvproject.h"
#include "keiluvutils.h"
#include "keiluvversioninfo.h"

#include "archs/mcs51/mcs51buildtargetgroup_v5.h"
#include "archs/arm/armbuildtargetgroup_v5.h"

#include <logging/translator.h>

namespace qbs {

static QString keilProjectSchema(const gen::VersionInfo &info)
{
    const auto v = info.marketingVersion();
    switch (v) {
    case KeiluvConstants::v5::kUVisionVersion:
        return QStringLiteral("2.1");
    default:
        return {};
    }
}

KeiluvProject::KeiluvProject(const qbs::GeneratableProject &genProject,
        const qbs::GeneratableProductData &genProduct,
        const gen::VersionInfo &versionInfo)
{
    Q_ASSERT(genProject.projects.size() == genProject.commandLines.size());
    Q_ASSERT(genProject.projects.size() == genProduct.data.size());

    // Create available configuration group factories.
    m_factories.push_back(std::make_unique<
                          keiluv::mcs51::v5::Mcs51BuildTargetGroupFactory>());
    m_factories.push_back(std::make_unique<
                          keiluv::arm::v5::ArmBuildTargetGroupFactory>());

    // Construct schema version item (is it depends on a project version?).
    const auto schema = keilProjectSchema(versionInfo);
    appendChild<gen::xml::Property>(QByteArrayLiteral("SchemaVersion"),
                                    schema);

    // Construct targets group.
    const auto targetsGroup = appendChild<gen::xml::PropertyGroup>(
                QByteArrayLiteral("Targets"));

    // Construct all build target items.
    const int configsCount = std::max(genProject.projects.size(),
                                      genProduct.data.size());
    for (auto configIndex = 0; configIndex < configsCount; ++configIndex) {
        const qbs::Project qbsProject = genProject.projects
                .values().at(configIndex);
        const qbs::ProductData qbsProduct = genProduct.data
                .values().at(configIndex);
        const QString confName = gen::utils::buildConfigurationName(qbsProject);
        const std::vector<ProductData> qbsProductDeps = gen::utils::dependenciesOf
                (qbsProduct, genProject, confName);

        const auto arch = gen::utils::architecture(qbsProject);
        if (arch == gen::utils::Architecture::Unknown)
            throw ErrorInfo(Internal::Tr::tr("Target architecture is not set,"
                                             " please use the 'profile' option"));

        // Construct the build target item, which are depend from
        // the architecture and the version.
        const auto factoryEnd = m_factories.cend();
        const auto factoryIt = std::find_if(m_factories.cbegin(), factoryEnd,
                                            [arch, versionInfo](const auto &factory) {
            return factory->canCreate(arch, versionInfo.version());
        });
        if (factoryIt == factoryEnd) {
            throw ErrorInfo(Internal::Tr::tr("Incompatible target architecture '%1'"
                                             " for KEIL UV version %2")
                            .arg(gen::utils::architectureName(arch))
                            .arg(versionInfo.marketingVersion()));
        }

        auto targetGroup = (*factoryIt)->create(
                    qbsProject, qbsProduct, qbsProductDeps);
        targetsGroup->appendChild(std::move(targetGroup));
    }
}

} // namespace qbs
