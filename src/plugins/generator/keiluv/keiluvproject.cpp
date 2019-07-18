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

#include "ikeiluvnodevisitor.h"
#include "keiluvproject.h"
#include "keiluvutils.h"
#include "keiluvversioninfo.h"

#include "archs/mcs51/mcs51buildtargetgroup_v5.h"
#include "archs/mcs51/mcs51utils.h"

#include <logging/translator.h>

namespace qbs {

static QString keilProjectSchema(const KeiluvVersionInfo &info)
{
    const auto v = info.marketingVersion();
    switch (v) {
    case keiluv::mcs51::v5::KeiluvConstants::kUVisionVersion:
        return QStringLiteral("1.1");
    default:
        return {};
    }
}

KeiluvProject::KeiluvProject(const GeneratableProject &genProject,
                             const GeneratableProductData &genProduct,
                             const KeiluvVersionInfo &versionInfo)
{
    Q_ASSERT(genProject.projects.size() == genProject.commandLines.size());
    Q_ASSERT(genProject.projects.size() == genProduct.data.size());

    // Create available configuration group factories.
    m_factories.push_back(std::make_unique<
                          keiluv::mcs51::v5::Mcs51BuildTargetGroupFactory>());

    // Construct schema version item (depends on a project version).
    const auto schema = keilProjectSchema(versionInfo);
    appendChild<KeiluvProperty>(QByteArrayLiteral("SchemaVersion"),
                                schema);

    // Construct targets group.
    const auto targetsGroup = appendChild<KeiluvPropertyGroup>(
                QByteArrayLiteral("Targets"));

    // Construct all build target items.
    const int configsCount = std::max(genProject.projects.size(),
                                      genProduct.data.size());
    for (auto configIndex = 0; configIndex < configsCount; ++configIndex) {
        const Project qbsProject = genProject.projects.values().at(configIndex);
        const ProductData qbsProduct = genProduct.data.values().at(configIndex);
        const QString confName = KeiluvUtils::buildConfigurationName(qbsProject);
        const std::vector<ProductData> qbsProductDeps = KeiluvUtils::dependenciesOf
                (qbsProduct, genProject, confName);

        const auto arch = KeiluvUtils::architecture(qbsProject);
        if (arch == KeiluvUtils::Architecture::UnknownArchitecture)
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
                            .arg(KeiluvUtils::architectureName(arch))
                            .arg(versionInfo.marketingVersion()));
        }

        auto targetGroup = (*factoryIt)->create(
                    qbsProject, qbsProduct, qbsProductDeps);
        targetsGroup->appendChild(std::move(targetGroup));
    }
}

void KeiluvProject::accept(IKeiluvNodeVisitor *visitor) const
{
    visitor->visitStart(this);

    for (const auto &child : children())
        child->accept(visitor);

    visitor->visitEnd(this);
}

} // namespace qbs
