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

#include "iarewfileversionproperty.h"
#include "iarewproject.h"
#include "iarewsourcefilespropertygroup.h"
#include "iarewutils.h"
#include "iarewversioninfo.h"

#include "archs/arm/armbuildconfigurationgroup_v8.h"
#include "archs/avr/avrbuildconfigurationgroup_v7.h"
#include "archs/mcs51/mcs51buildconfigurationgroup_v10.h"
#include "archs/stm8/stm8buildconfigurationgroup_v3.h"
#include "archs/msp430/msp430buildconfigurationgroup_v7.h"

#include <logging/translator.h>

namespace qbs {

IarewProject::IarewProject(const GeneratableProject &genProject,
                           const GeneratableProductData &genProduct,
                           const gen::VersionInfo &versionInfo)
{
    Q_ASSERT(genProject.projects.size() == genProject.commandLines.size());
    Q_ASSERT(genProject.projects.size() == genProduct.data.size());

    // Create available configuration group factories.
    m_factories.push_back(std::make_unique<
                          iarew::arm::v8::ArmBuildConfigurationGroupFactory>());
    m_factories.push_back(std::make_unique<
                          iarew::avr::v7::AvrBuildConfigurationGroupFactory>());
    m_factories.push_back(std::make_unique<
                          iarew::mcs51::v10::Mcs51BuildConfigurationGroupFactory>());
    m_factories.push_back(std::make_unique<
                          iarew::stm8::v3::Stm8BuildConfigurationGroupFactory>());
    m_factories.push_back(std::make_unique<
                          iarew::msp430::v7::Msp430BuildConfigurationGroupFactory>());

    // Construct file version item.
    appendChild<IarewFileVersionProperty>(versionInfo);

    // Construct all build configurations items.
    const int configsCount = std::max(genProject.projects.size(),
                                      genProduct.data.size());
    for (auto configIndex = 0; configIndex < configsCount; ++configIndex) {
        const qbs::Project qbsProject = genProject.projects
                .values().at(configIndex);
        const ProductData qbsProduct = genProduct.data.values().at(configIndex);
        const QString confName = gen::utils::buildConfigurationName(qbsProject);
        const std::vector<ProductData> qbsProductDeps = gen::utils::dependenciesOf
                (qbsProduct, genProject, confName);

        const auto arch = gen::utils::architecture(qbsProject);
        if (arch == gen::utils::Architecture::Unknown)
            throw ErrorInfo(Internal::Tr::tr("Target architecture is not set,"
                                             " please use the 'profile' option"));

        // Construct the build configuration item, which are depend from
        // the architecture and the version.
        const auto factoryEnd = m_factories.cend();
        const auto factoryIt = std::find_if(
                    m_factories.cbegin(), factoryEnd,
                    [arch, versionInfo](const auto &factory) {
            return factory->canCreate(arch, versionInfo.version());
        });
        if (factoryIt == factoryEnd) {
            throw ErrorInfo(Internal::Tr::tr("Incompatible target architecture '%1'"
                                             " for IAR EW version %2xxx")
                            .arg(gen::utils::architectureName(arch))
                            .arg(versionInfo.marketingVersion()));
        }
        auto configGroup = (*factoryIt)->create(
                    qbsProject, qbsProduct, qbsProductDeps);
        appendChild(std::move(configGroup));
    }

    // Construct all file groups items.
    QMapIterator<QString, qbs::ProductData> dataIt(genProduct.data);
    while (dataIt.hasNext()) {
        dataIt.next();
        const auto groups = dataIt.value().groups();
        for (const auto &group : groups) {
            // Ignore disabled groups (e.g. when its condition property is false).
            if (!group.isEnabled())
                continue;
            auto sourceArtifacts = group.sourceArtifacts();
            // Remove the linker script artifacts.
            sourceArtifacts.erase(std::remove_if(sourceArtifacts.begin(),
                                                 sourceArtifacts.end(),
                                      [](const auto &artifact){
                const auto tags = artifact.fileTags();
                return tags.contains(QLatin1String("linkerscript"));
            }), sourceArtifacts.end());

            if (sourceArtifacts.isEmpty())
                continue;
            appendChild<IarewSourceFilesPropertyGroup>(
                            genProject, group.name(), sourceArtifacts);
        }
    }
}

} // namespace qbs
