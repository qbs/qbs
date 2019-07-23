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

#include "mcs51targetassemblergroup_v5.h"
#include "mcs51utils.h"

#include "../../keiluvutils.h"

namespace qbs {
namespace keiluv {
namespace mcs51 {
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

        // Don't use standard macro.
        if (flags.contains(QLatin1String("NOMACRO"), Qt::CaseInsensitive))
            useStandardMacroProcessor = false;

        // Use MPL.
        if (flags.contains(QLatin1String("MPL"), Qt::CaseInsensitive))
            useMacroProcessingLanguage = true;

        // Define 8051 SFR names.
        if (flags.contains(QLatin1String("NOMOD51"), Qt::CaseInsensitive))
            suppressSfrNames = true;

        // Define symbols.
        defineSymbols = qbs::KeiluvUtils::defines(qbsProps);
        // Include paths.
        includePaths = qbs::KeiluvUtils::includes(qbsProps);

        // Interpret other assembler flags as a misc controls (exclude only
        // that flags which are was already handled).
        for (const auto &flag : flags) {
            if (flag.compare(QLatin1String("NOMACRO"),
                             Qt::CaseInsensitive) == 0
                    || flag.compare(QLatin1String("MACRO"),
                                    Qt::CaseInsensitive) == 0
                    || flag.compare(QLatin1String("NOMPL"),
                                    Qt::CaseInsensitive) == 0
                    || flag.compare(QLatin1String("MPL"),
                                    Qt::CaseInsensitive) == 0
                    || flag.compare(QLatin1String("NOMOD51"),
                                    Qt::CaseInsensitive) == 0
                    || flag.compare(QLatin1String("MOD51"),
                                    Qt::CaseInsensitive) == 0
                    ) {
                continue;
            }
            miscControls.push_back(flag);
        }
    }

    int useStandardMacroProcessor = true;
    int useMacroProcessingLanguage = false;
    int suppressSfrNames = false;
    QStringList defineSymbols;
    QStringList includePaths;
    QStringList miscControls;
};

} // namespace

Mcs51TargetAssemblerGroup::Mcs51TargetAssemblerGroup(
        const qbs::Project &qbsProject,
        const qbs::ProductData &qbsProduct)
    : gen::xml::PropertyGroup("Ax51")
{
    const AssemblerPageOptions opts(qbsProject, qbsProduct);

    // Add 'Macro processor (Standard)'
    appendProperty(QByteArrayLiteral("UseStandard"),
                   opts.useStandardMacroProcessor);
    // Add 'Macro processor (MPL)'
    appendProperty(QByteArrayLiteral("UseMpl"),
                   opts.useMacroProcessingLanguage);
    // Add 'Define 8051 SFR names'
    appendProperty(QByteArrayLiteral("UseMod51"),
                   opts.suppressSfrNames);

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
                QByteArrayLiteral("Define"),
                opts.defineSymbols);
    // Add an empty 'Undefine' item.
    variousControlsGroup->appendProperty(
                QByteArrayLiteral("Undefine"), {});
    // Add 'Include Paths' item.
    variousControlsGroup->appendMultiLineProperty(
                QByteArrayLiteral("IncludePath"),
                opts.includePaths, QLatin1Char(';'));
}

} // namespace v5
} // namespace mcs51
} // namespace keiluv
} // namespace qbs
