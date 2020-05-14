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

#include "armtargetlinkergroup_v5.h"

#include "../../keiluvutils.h"

#include <generators/generatorutils.h>

namespace qbs {
namespace keiluv {
namespace arm {
namespace v5 {

namespace {

struct LinkerPageOptions final
{
    explicit LinkerPageOptions(const Project &qbsProject,
                               const ProductData &qbsProduct)
    {
        Q_UNUSED(qbsProject)

        const auto &qbsProps = qbsProduct.moduleProperties();
        const auto flags = qbs::KeiluvUtils::cppModuleLinkerFlags(qbsProps);

        // Read-only position independent.
        enableRopi = flags.contains(QLatin1String("--ropi"));
        // Read-write position independent.
        enableRwpi = flags.contains(QLatin1String("--rwpi"));
        // Don't search standard libraries.
        dontSearchLibs = flags.contains(QLatin1String("--noscanlib"));
        // Report 'might fail' conditions as errors.
        enableReportMightFail = flags.contains(QLatin1String("--strict"));

        QStringList scatterFiles;

        // Enumerate all product linker config files
        // (which are set trough 'linkerscript' tag).
        for (const auto &qbsGroup : qbsProduct.groups()) {
            if (!qbsGroup.isEnabled())
                continue;
            const auto qbsArtifacts = qbsGroup.sourceArtifacts();
            for (const auto &qbsArtifact : qbsArtifacts) {
                const auto qbsTags = qbsArtifact.fileTags();
                if (!qbsTags.contains(QLatin1String("linkerscript")))
                    continue;
                const QString scatterFile = QFileInfo(qbsArtifact.filePath())
                        .absoluteFilePath();
                scatterFiles.push_back(scatterFile);
            }
        }

        // Enumerate all scatter files
        // (which are set trough '--scatter' option).
        const QStringList scatters = gen::utils::allFlagValues(
                    flags, QStringLiteral("--scatter"));
        for (const auto &scatter : scatters) {
            const QString scatterFile = QFileInfo(scatter)
                    .absoluteFilePath();
            if (!scatterFiles.contains(scatterFile))
                scatterFiles.push_back(scatterFile);
        }

        // Transform all paths to relative.
        const QString baseDirectory = qbs::gen::utils::buildRootPath(qbsProject);
        std::transform(scatterFiles.begin(), scatterFiles.end(),
                       std::back_inserter(scatterFiles),
                       [baseDirectory](const QString &scatterFile) {
            return gen::utils::relativeFilePath(baseDirectory, scatterFile);
        });

        // Make a first scatter file as a main scatter file.
        // Other scatter files will be interpretes as a misc controls.
        if (scatterFiles.count() > 0)
            mainScatterFile = scatterFiles.takeFirst();

        for (const auto &scatterFile : qAsConst(scatterFiles)) {
            const auto control = QStringLiteral("--scatter %1").arg(scatterFile);
            miscControls.push_back(control);
        }

        // Interpret other compiler flags as a misc controls (exclude only
        // that flags which are was already handled).
        for (auto flagIt = flags.cbegin(); flagIt < flags.cend(); ++flagIt) {
            if (flagIt->contains(QLatin1String("--ropi"))
                    || flagIt->contains(QLatin1String("--rwpi"))
                    || flagIt->contains(QLatin1String("--noscanlib"))
                    || flagIt->contains(QLatin1String("--strict"))
                    ) {
                continue;
            }
            if (flagIt->startsWith(QLatin1String("--scatter"))
                    ) {
                ++flagIt;
                continue;
            }
            miscControls.push_back(*flagIt);
        }
    }

    int enableRopi = 0;
    int enableRwpi = 0;
    int dontSearchLibs = 0;
    int enableReportMightFail = 0;

    QString mainScatterFile;
    QStringList miscControls;
};

} // namespace

ArmTargetLinkerGroup::ArmTargetLinkerGroup(
        const qbs::Project &qbsProject,
        const qbs::ProductData &qbsProduct)
    : gen::xml::PropertyGroup("LDads")
{
    const LinkerPageOptions opts(qbsProject, qbsProduct);

    // Add 'ROPI' item.
    appendProperty(QByteArrayLiteral("Ropi"), opts.enableRopi);
    // Add 'RWPI' item.
    appendProperty(QByteArrayLiteral("Rwpi"), opts.enableRwpi);
    // Add 'Don't search standard libraries' item.
    appendProperty(QByteArrayLiteral("noStLib"), opts.dontSearchLibs);
    // Add 'Report might fail' item.
    appendProperty(QByteArrayLiteral("RepFail"), opts.enableReportMightFail);
    // Add 'Scatter file' item.
    appendProperty(QByteArrayLiteral("ScatterFile"),
                   QDir::toNativeSeparators(opts.mainScatterFile));
}

} // namespace v5
} // namespace arm
} // namespace keiluv
} // namespace qbs
