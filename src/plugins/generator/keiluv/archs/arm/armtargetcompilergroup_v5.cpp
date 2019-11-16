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

#include "armtargetcompilergroup_v5.h"

#include "../../keiluvutils.h"

namespace qbs {
namespace keiluv {
namespace arm {
namespace v5 {

namespace {

struct CompilerPageOptions final
{
    enum WarningLevel {
        WarningLevelUnspecified = 0,
        WarningLevelNone,
        WarningLevelAll
    };

    enum OptimizationLevel {
        OptimizationLevelUnspecified = 0,
        OptimizationLevelNone,
        OptimizationLevelOne,
        OptimizationLevelTwo,
        OptimizationLevelThree,
    };

    explicit CompilerPageOptions(const Project &qbsProject,
                                 const ProductData &qbsProduct)
    {
        Q_UNUSED(qbsProject)

        const auto &qbsProps = qbsProduct.moduleProperties();
        const auto flags = qbs::KeiluvUtils::cppModuleCompilerFlags(qbsProps);

        // Warning levels.
        const QString wLevel = gen::utils::cppStringModuleProperty(
                    qbsProps, QStringLiteral("warningLevel"));
        if (wLevel == QLatin1String("none"))
            warningLevel = WarningLevelNone;
        else if (wLevel == QLatin1String("all"))
            warningLevel = WarningLevelAll;
        else
            warningLevel = WarningLevelUnspecified;

        // Generation code.
        generateExecuteOnlyCode = flags.contains(QLatin1String("--execute_only"));

        // Optimization levels.
        const QString oLevel = gen::utils::cppStringModuleProperty(
                    qbsProps, QStringLiteral("optimization"));
        if (oLevel == QLatin1String("fast"))
            enableTimeOptimization = 1;
        else if (oLevel == QLatin1String("small"))
            optimizationLevel = OptimizationLevelThree;
        else if (oLevel == QLatin1String("none"))
            optimizationLevel = OptimizationLevelNone;

        // Split load and store multiple.
        splitLdm = flags.contains(QLatin1String("--split_ldm"));
        // One ELF section per function.
        splitSections = flags.contains(QLatin1String("--split_sections"));
        // String ANSI C.
        useStrictAnsiC = flags.contains(QLatin1String("--strict"));
        // Enum container always int.
        forceEnumAsInt = flags.contains(QLatin1String("--enum_is_int"));
        // Plain char is signed.
        useSignedChar = flags.contains(QLatin1String("--signed_chars"));
        // Read-only position independent.
        enableRopi = flags.contains(QLatin1String("/ropi"));
        // Read-write position independent.
        enableRwpi = flags.contains(QLatin1String("/rwpi"));

        // C-language version.
        const QString clVersion = gen::utils::cppStringModuleProperty(
                    qbsProps, QStringLiteral("cLanguageVersion"));
        // C99 mode.
        useC99Language = clVersion.contains(QLatin1String("c99"));

        // Define symbols.
        defineSymbols = qbs::KeiluvUtils::defines(qbsProps);
        // Include paths.
        includePaths = qbs::KeiluvUtils::includes(qbsProps);

        // Interpret other compiler flags as a misc controls (exclude only
        // that flags which are was already handled).
        for (auto flagIt = flags.cbegin(); flagIt < flags.cend(); ++flagIt) {
            if (flagIt->contains(QLatin1String("--execute_only"))
                    || flagIt->contains(QLatin1String("--split_ldm"))
                    || flagIt->contains(QLatin1String("--split_sections"))
                    || flagIt->contains(QLatin1String("--strict"))
                    || flagIt->contains(QLatin1String("--enum_is_int"))
                    || flagIt->contains(QLatin1String("--signed_chars"))
                    || flagIt->contains(QLatin1String("/ropi"))
                    || flagIt->contains(QLatin1String("/rwpi"))
                    || flagIt->contains(QLatin1String("--c99"))
                    ) {
                continue;
            }
            if (flagIt->startsWith(QLatin1String("-O"))
                    || flagIt->startsWith(QLatin1String("-W"))
                    || flagIt->startsWith(QLatin1String("-D"))
                    || flagIt->startsWith(QLatin1String("-I"))
                    || flagIt->startsWith(QLatin1String("--cpu"))
                    || flagIt->startsWith(QLatin1String("--fpu"))
                    ) {
                ++flagIt;
                continue;
            }
            miscControls.push_back(*flagIt);
        }
    }

    WarningLevel warningLevel = WarningLevelAll;
    OptimizationLevel optimizationLevel = OptimizationLevelUnspecified;
    int enableTimeOptimization = 0;
    int generateExecuteOnlyCode = 0;
    int splitLdm = 0;
    int splitSections = 0;
    int useStrictAnsiC = 0;
    int forceEnumAsInt = 0;
    int useSignedChar = 0;
    int enableRopi = 0;
    int enableRwpi = 0;
    int useC99Language = 0;

    QStringList defineSymbols;
    QStringList includePaths;
    QStringList miscControls;
};

} // namespace

ArmTargetCompilerGroup::ArmTargetCompilerGroup(
        const qbs::Project &qbsProject,
        const qbs::ProductData &qbsProduct)
    : gen::xml::PropertyGroup("Cads")
{
    const CompilerPageOptions opts(qbsProject, qbsProduct);

    // Add 'Code Optimization' items.
    appendProperty(QByteArrayLiteral("Optim"), opts.optimizationLevel);
    appendProperty(QByteArrayLiteral("oTime"), opts.enableTimeOptimization);
    // Add 'Slpit LDM' item.
    appendProperty(QByteArrayLiteral("SplitLS"), opts.splitLdm);
    // Add 'Slpit sections' item.
    appendProperty(QByteArrayLiteral("OneElfS"), opts.splitSections);
    // Add 'Strict ANSI C' item.
    appendProperty(QByteArrayLiteral("Strict"), opts.useStrictAnsiC);
    // Add 'Enums as int' item.
    appendProperty(QByteArrayLiteral("EnumInt"), opts.forceEnumAsInt);
    // Add 'Plain char as signed' item.
    appendProperty(QByteArrayLiteral("PlainCh"), opts.useSignedChar);
    // Add 'ROPI' item.
    appendProperty(QByteArrayLiteral("Ropi"), opts.enableRopi);
    // Add 'RWPI' item.
    appendProperty(QByteArrayLiteral("Rwpi"), opts.enableRwpi);
    // Add 'Warnings' item.
    appendProperty(QByteArrayLiteral("wLevel"), opts.warningLevel);
    // Add 'Use C99' item.
    appendProperty(QByteArrayLiteral("uC99"), opts.useC99Language);
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
