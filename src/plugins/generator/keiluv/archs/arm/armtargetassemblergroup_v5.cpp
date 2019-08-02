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

#include "armtargetassemblergroup_v5.h"

#include "../../keiluvutils.h"

namespace qbs {
namespace keiluv {
namespace arm {
namespace v5 {

namespace {

struct AssemblerPageOptions final
{
    explicit AssemblerPageOptions(const Project &qbsProject,
                                  const ProductData &qbsProduct)
    {
        Q_UNUSED(qbsProject)

        const auto &qbsProps = qbsProduct.moduleProperties();
        const auto flags = qbs::KeiluvUtils::cppModuleAssemblerFlags(qbsProps);

        // Read-only position independent.
        enableRopi = flags.contains(QLatin1String("/ropi"));
        // Read-write position independent.
        enableRwpi = flags.contains(QLatin1String("/rwpi"));
        // Enable thumb mode.
        enableThumbMode = flags.contains(QLatin1String("--16"));
        // Split load and store multiple.
        splitLdm = flags.contains(QLatin1String("--split_ldm"));
        // Generation code.
        generateExecuteOnlyCode = flags.contains(QLatin1String("--execute_only"));

        // Warning levels.
        const QString wLevel = gen::utils::cppStringModuleProperty(
                    qbsProps, QStringLiteral("warningLevel"));
        disableWarnings = wLevel == QLatin1String("none");

        // Define symbols.
        defineSymbols = qbs::KeiluvUtils::defines(qbsProps);
        // Include paths.
        includePaths = qbs::KeiluvUtils::includes(qbsProps);

        // Interpret other compiler flags as a misc controls (exclude only
        // that flags which are was already handled).
        for (auto flagIt = flags.cbegin(); flagIt < flags.cend(); ++flagIt) {
            if (flagIt->contains(QLatin1String("/ropi"))
                    || flagIt->contains(QLatin1String("/rwpi"))
                    || flagIt->contains(QLatin1String("--16"))
                    || flagIt->contains(QLatin1String("--split_ldm"))
                    || flagIt->contains(QLatin1String("--execute_only"))
                    || flagIt->contains(QLatin1String("--nowarn"))
                    ) {
                continue;
            }
            if (flagIt->startsWith(QLatin1String("-I"))
                    || flagIt->startsWith(QLatin1String("--cpu"))
                    || flagIt->startsWith(QLatin1String("--fpu"))
                    || flagIt->startsWith(QLatin1String("-pd"))
                    ) {
                ++flagIt;
                continue;
            }
            miscControls.push_back(*flagIt);
        }
    }

    int enableRopi = 0;
    int enableRwpi = 0;
    int enableThumbMode = 0;
    int disableWarnings = 0;
    int splitLdm = 0;
    int generateExecuteOnlyCode = 0;

    QStringList defineSymbols;
    QStringList includePaths;
    QStringList miscControls;
};

} // namespace

ArmTargetAssemblerGroup::ArmTargetAssemblerGroup(
        const qbs::Project &qbsProject,
        const qbs::ProductData &qbsProduct)
    : gen::xml::PropertyGroup("Aads")
{
    const AssemblerPageOptions opts(qbsProject, qbsProduct);

    // Add 'ROPI' item.
    appendProperty(QByteArrayLiteral("Ropi"), opts.enableRopi);
    // Add 'RWPI' item.
    appendProperty(QByteArrayLiteral("Rwpi"), opts.enableRwpi);
    // Add 'Use thumb mode' item.
    appendProperty(QByteArrayLiteral("thumb"), opts.enableThumbMode);
    // Add 'Slpit LDM' item.
    appendProperty(QByteArrayLiteral("SplitLS"), opts.splitLdm);
    // Add 'Disable warnings' item.
    appendProperty(QByteArrayLiteral("NoWarn"), opts.disableWarnings);
    // Add 'Generate code exedutable only' item.
    appendProperty(QByteArrayLiteral("useXo"), opts.generateExecuteOnlyCode);

    // Add other various controls.
    // Note: A sub-items order makes sense!
    const auto variousControlsGroup = appendChild<gen::xml::PropertyGroup>(
                QByteArrayLiteral("VariousControls"));
    // Add 'Misc Controls' item.
    variousControlsGroup->appendMultiLineProperty(
                QByteArrayLiteral("MiscControls"),
                opts.miscControls, QLatin1Char(' '));
    // Add 'Define' item.
    variousControlsGroup->appendMultiLineProperty(
                QByteArrayLiteral("Define"), opts.defineSymbols);
    // Add an empty 'Undefine' item.
    variousControlsGroup->appendProperty(
                QByteArrayLiteral("Undefine"), {});
    // Add 'Include Paths' item.
    variousControlsGroup->appendMultiLineProperty(
                QByteArrayLiteral("IncludePath"),
                opts.includePaths, QLatin1Char(';'));
}

} // namespace v5
} // namespace arm
} // namespace keiluv
} // namespace qbs
