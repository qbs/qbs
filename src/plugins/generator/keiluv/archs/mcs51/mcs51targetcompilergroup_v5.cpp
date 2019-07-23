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

#include "mcs51targetcompilergroup_v5.h"
#include "mcs51utils.h"

#include "../../keiluvutils.h"

namespace qbs {
namespace keiluv {
namespace mcs51 {
namespace v5 {

namespace {

struct CompilerPageOptions final
{
    enum WarningLevel {
        WarningLevelNone = 0,
        WarningLevelOne,
        WarningLevelTwo
    };

    enum OptimizationLevel {
        ConstantFoldingOptimizationLevel = 0,
        DeadCodeEliminationOptimizationLevel,
        DataOverlayingOptimizationLevel,
        PeepholeOptimizationLevel,
        RegisterVariablesOptimizationLevel,
        CommonSubexpressionEliminationOptimizationLevel,
        LoopRotationOptimizationLevel,
        ExtendedIndexAccessOptimizationLevel,
        ReuseCommonEntryCodeOptimizationLevel,
        CommonBlockSubroutinesOptimizationLevel,
        RearrangeCodeOptimizationLevel,
        ReuseCommonExitCodeOptimizationLevel
    };

    enum OptimizationEmphasis {
        FavorSizeOptimizationEmphasis = 0,
        FavorSpeedOptimizationEmphasis
    };

    enum FloatFuzzyBits {
        NoFloatFuzzyBits = 0,
        OneFloatFuzzyBit,
        TwoFloatFuzzyBits,
        ThreeFloatFuzzyBits,
        FourFloatFuzzyBits,
        FiveFloatFuzzyBits,
        SixFloatFuzzyBits,
        SevenFloatFuzzyBits
    };

    explicit CompilerPageOptions(const Project &qbsProject,
                                 const ProductData &qbsProduct)
    {
        Q_UNUSED(qbsProject)

        const auto &qbsProps = qbsProduct.moduleProperties();
        const auto flags = qbs::KeiluvUtils::cppModuleCompilerFlags(qbsProps);

        // Warnings.
        const QString level = gen::utils::cppStringModuleProperty(
                    qbsProps, QStringLiteral("warningLevel"));
        if (level == QLatin1String("none")) {
            warningLevel = WarningLevelNone;
        } else if (level == QLatin1String("all")) {
            warningLevel = WarningLevelTwo;
        } else {
            // In this case take it directly from the compiler command line,
            // e.g. parse the line in a form: 'WARNINGLEVEL (2)'
            const auto warnValue = KeiluvUtils::flagValue(
                        flags, QStringLiteral("WARNINGLEVEL"));
            bool ok = false;
            const auto level = warnValue.toInt(&ok);
            if (ok && gen::utils::inBounds(
                        level, int(WarningLevelNone),int(WarningLevelTwo))) {
                warningLevel = static_cast<WarningLevel>(level);
            }
        }

        // Optimizations.
        const QString optimization = gen::utils::cppStringModuleProperty(
                    qbsProps, QStringLiteral("optimization"));
        if (optimization == QLatin1String("fast")) {
            optimizationEmphasis = FavorSpeedOptimizationEmphasis;
        } else if (level == QLatin1String("small")) {
            optimizationEmphasis = FavorSizeOptimizationEmphasis;
        } else if (level == QLatin1String("small")) {
            // Don't supported by C51 compiler.
        } else {
            // In this case take it directly from the compiler command line,
            // e.g. parse the line in a form: 'OPTIMIZE (8, SPEED)'
            const auto optValue = KeiluvUtils::flagValue(
                        flags, QStringLiteral("OPTIMIZE"));
            const auto parts = KeiluvUtils::flagValueParts(optValue);
            for (const auto &part : parts) {
                bool ok = false;
                const auto level = part.toInt(&ok);
                if (ok && (level >= ConstantFoldingOptimizationLevel)
                        && (level <= ReuseCommonExitCodeOptimizationLevel)) {
                    optimizationLevel = static_cast<OptimizationLevel>(level);
                } else if (part.compare(QLatin1String("SIZE"),
                                        Qt::CaseInsensitive) == 0) {
                    optimizationEmphasis = FavorSizeOptimizationEmphasis;
                } else if (part.compare(QLatin1String("SPEED"),
                                        Qt::CaseInsensitive) == 0) {
                    optimizationEmphasis = FavorSpeedOptimizationEmphasis;
                }
            }
        }

        // Don't use absolute register accesses.
        if (flags.contains(QLatin1String("NOAREGS"), Qt::CaseInsensitive))
            dontuseAbsoluteRegsAccess = true;

        // Enable ANSI integer promotion rules.
        if (flags.contains(QLatin1String("NOINTPROMOTE"), Qt::CaseInsensitive))
            enableIntegerPromotionRules = false;

        // Keep variables in order.
        if (flags.contains(QLatin1String("ORDER"), Qt::CaseInsensitive))
            keepVariablesInOrder = true;

        // Don't use interrupt vector.
        if (flags.contains(QLatin1String("NOINTVECTOR"), Qt::CaseInsensitive))
            useInterruptVector = false;

        // Interrupt vector address.
        interruptVectorAddress = KeiluvUtils::flagValue(
                    flags, QStringLiteral("INTVECTOR"));

        // Float fuzzy bits count.
        const auto bitsValue = KeiluvUtils::flagValue(
                    flags, QStringLiteral("FLOATFUZZY"));
        bool ok = false;
        const auto bits = bitsValue.toInt(&ok);
        if (ok && gen::utils::inBounds(
                    bits, int(NoFloatFuzzyBits), int(SevenFloatFuzzyBits))) {
            floatFuzzyBits = static_cast<FloatFuzzyBits>(bits);
        }

        // Define symbols.
        defineSymbols = qbs::KeiluvUtils::defines(qbsProps);
        // Include paths.
        includePaths = qbs::KeiluvUtils::includes(qbsProps);

        // Interpret other compiler flags as a misc controls (exclude only
        // that flags which are was already handled).
        for (const auto &flag : flags) {
            if (flag.startsWith(QLatin1String("WARNINGLEVEL"),
                                Qt::CaseInsensitive)
                    || flag.startsWith(QLatin1String("OPTIMIZE"),
                                       Qt::CaseInsensitive)
                    || flag.startsWith(QLatin1String("FLOATFUZZY"),
                                       Qt::CaseInsensitive)
                    || flag.compare(QLatin1String("NOAREGS"),
                                    Qt::CaseInsensitive) == 0
                    || flag.compare(QLatin1String("AREGS"),
                                    Qt::CaseInsensitive) == 0
                    || flag.compare(QLatin1String("NOINTPROMOTE"),
                                    Qt::CaseInsensitive) == 0
                    || flag.compare(QLatin1String("INTPROMOTE"),
                                    Qt::CaseInsensitive) == 0
                    || flag.compare(QLatin1String("NOINTVECTOR"),
                                    Qt::CaseInsensitive) == 0
                    || flag.compare(QLatin1String("INTVECTOR"),
                                    Qt::CaseInsensitive) == 0
                    || flag.compare(QLatin1String("ORDER"),
                                    Qt::CaseInsensitive) == 0
                    || flag.compare(QLatin1String("BROSWE"),
                                    Qt::CaseInsensitive) == 0
                    ) {
                continue;
            }
            miscControls.push_back(flag);
        }
    }

    WarningLevel warningLevel = WarningLevelTwo;
    OptimizationLevel optimizationLevel = ReuseCommonEntryCodeOptimizationLevel;
    OptimizationEmphasis optimizationEmphasis = FavorSpeedOptimizationEmphasis;
    FloatFuzzyBits floatFuzzyBits = ThreeFloatFuzzyBits;
    int dontuseAbsoluteRegsAccess = false;
    int enableIntegerPromotionRules = true;
    int keepVariablesInOrder = false;
    int useInterruptVector = true;
    QString interruptVectorAddress;
    QStringList defineSymbols;
    QStringList includePaths;
    QStringList miscControls;
};

} // namespace

Mcs51TargetCompilerGroup::Mcs51TargetCompilerGroup(
        const qbs::Project &qbsProject,
        const qbs::ProductData &qbsProduct)
    : gen::xml::PropertyGroup("C51")
{
    const CompilerPageOptions opts(qbsProject, qbsProduct);

    // Add 'Code Optimization' options.
    appendProperty(QByteArrayLiteral("Optimize"),
                   opts.optimizationLevel);
    appendProperty(QByteArrayLiteral("SizeSpeed"),
                   opts.optimizationEmphasis);
    // Add 'Warnings' options.
    appendProperty(QByteArrayLiteral("WarningLevel"),
                   opts.warningLevel);
    // Add 'Don't use absolute register access' item.
    appendProperty(QByteArrayLiteral("uAregs"),
                   opts.dontuseAbsoluteRegsAccess);
    // Add 'Enable integer promotion rules' item.
    appendProperty(QByteArrayLiteral("IntegerPromotion"),
                   opts.enableIntegerPromotionRules);
    // Add 'Keep variables in order' item.
    appendProperty(QByteArrayLiteral("VariablesInOrder"),
                   opts.keepVariablesInOrder);
    // Add 'Use interrupt vector' item.
    appendProperty(QByteArrayLiteral("UseInterruptVector"),
                   opts.useInterruptVector);
    appendProperty(QByteArrayLiteral("InterruptVectorAddress"),
                   opts.interruptVectorAddress);
    // Add 'Float fuzzy bits' item.
    appendProperty(QByteArrayLiteral("Fuzzy"),
                   opts.floatFuzzyBits);

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
