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

#include "msp430compilersettingsgroup_v7.h"

#include "../../iarewutils.h"

namespace qbs {
namespace iarew {
namespace msp430 {
namespace v7 {

constexpr int kCompilerArchiveVersion = 4;
constexpr int kCompilerDataVersion = 38;

namespace {

// Output page options.

struct OutputPageOptions final
{
    explicit OutputPageOptions(const ProductData &qbsProduct)
    {
        debugInfo = gen::utils::debugInformation(qbsProduct);
    }

    int debugInfo = 0;
};

// Language one page options.

struct LanguageOnePageOptions final
{
    enum LanguageExtension {
        CLanguageExtension,
        CxxLanguageExtension,
        AutoLanguageExtension
    };

    enum CLanguageDialect {
        C89LanguageDialect,
        C99LanguageDialect
    };

    enum CxxLanguageDialect {
        EmbeddedCPlusPlus,
        ExtendedEmbeddedCPlusPlus
    };

    enum LanguageConformance {
        AllowIarExtension,
        RelaxedStandard,
        StrictStandard
    };

    explicit LanguageOnePageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleCompilerFlags(qbsProps);
        // File extension based by default.
        languageExtension = LanguageOnePageOptions::AutoLanguageExtension;
        // C language dialect.
        const QStringList cLanguageVersion = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("cLanguageVersion")});
        if (cLanguageVersion.contains(QLatin1String("c89")))
            cLanguageDialect = LanguageOnePageOptions::C89LanguageDialect;
        else if (cLanguageVersion.contains(QLatin1String("c99")))
            cLanguageDialect = LanguageOnePageOptions::C99LanguageDialect;
        // C++ language dialect.
        if (flags.contains(QLatin1String("--ec++")))
            cxxLanguageDialect = LanguageOnePageOptions::EmbeddedCPlusPlus;
        else if (flags.contains(QLatin1String("--eec++")))
            cxxLanguageDialect = LanguageOnePageOptions::ExtendedEmbeddedCPlusPlus;
        // Language conformance.
        if (flags.contains(QLatin1String("-e")))
            languageConformance = LanguageOnePageOptions::AllowIarExtension;
        else if (flags.contains(QLatin1String("--strict")))
            languageConformance = LanguageOnePageOptions::StrictStandard;
        else
            languageConformance = LanguageOnePageOptions::RelaxedStandard;

        allowVla = flags.contains(QLatin1String("--vla"));
        useCppInlineSemantics = flags.contains(
                    QLatin1String("--use_c++_inline"));
        requirePrototypes = flags.contains(
                    QLatin1String("--require_prototypes"));
        destroyStaticObjects = !flags.contains(
                    QLatin1String("--no_static_destruction"));
    }

    LanguageExtension languageExtension = AutoLanguageExtension;
    CLanguageDialect cLanguageDialect = C99LanguageDialect;
    CxxLanguageDialect cxxLanguageDialect = EmbeddedCPlusPlus;
    LanguageConformance languageConformance = AllowIarExtension;
    int allowVla = 0;
    int useCppInlineSemantics = 0;
    int requirePrototypes = 0;
    int destroyStaticObjects = 0;
};

// Language two page options.

struct LanguageTwoPageOptions final
{
    enum PlainCharacter {
        SignedCharacter,
        UnsignedCharacter
    };

    enum FloatingPointSemantic {
        StrictSemantic,
        RelaxedSemantic
    };

    explicit LanguageTwoPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleCompilerFlags(qbsProps);
        plainCharacter = flags.contains(QLatin1String("--char_is_signed"))
                ? LanguageTwoPageOptions::SignedCharacter
                : LanguageTwoPageOptions::UnsignedCharacter;
        floatingPointSemantic = flags.contains(QLatin1String("--relaxed_fp"))
                ? LanguageTwoPageOptions::RelaxedSemantic
                : LanguageTwoPageOptions::StrictSemantic;
        enableMultibyteSupport = flags.contains(
                    QLatin1String("--enable_multibytes"));
        guardCalls = flags.contains(
                    QLatin1String("--guard_calls"));
    }

    PlainCharacter plainCharacter = UnsignedCharacter;
    FloatingPointSemantic floatingPointSemantic = StrictSemantic;
    int enableMultibyteSupport = 0;
    int guardCalls = 0;
};

// Code page options.

struct CodePageOptions final
{
    enum Utilization {
        UtilizationNormalUse,
        UtilizationRegVarVariables,
        UtilizationNotUsed
    };

    explicit CodePageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleCompilerFlags(qbsProps);
        // Detect R4 utilization.
        if (flags.contains(QLatin1String("--lock_r4")))
            r4utilization = UtilizationNotUsed;
        else if (flags.contains(QLatin1String("--regvar_r4")))
            r4utilization = UtilizationRegVarVariables;
        // Detect R5 utilization.
        if (flags.contains(QLatin1String("--lock_r5")))
            r5utilization = UtilizationNotUsed;
        else if (flags.contains(QLatin1String("--regvar_r54")))
            r5utilization = UtilizationRegVarVariables;
        // Detect reduce stack usage.
        reduceStackUsage = flags.contains(
                    QLatin1String("--reduce_stack_usage"));
        // Detect 20-bit context save on interrupt.
        save20BitContextOnInterrupt = flags.contains(
                    QLatin1String("--save_reg20"));
    }

    Utilization r4utilization = UtilizationNormalUse;
    Utilization r5utilization = UtilizationNormalUse;
    int reduceStackUsage = 0;
    int save20BitContextOnInterrupt = 0;
};

// Optimizations page options.

struct OptimizationsPageOptions final
{
    // Optimizations level radio-buttons with
    // combo-box on "level" widget.
    enum Strategy {
        StrategyBalanced,
        StrategySize,
        StrategySpeed
    };

    enum Level {
        LevelNone,
        LevelLow,
        LevelMedium,
        LevelHigh
    };

    enum LevelSlave {
        LevelSlave0,
        LevelSlave1,
        LevelSlave2,
        LevelSlave3
    };

    explicit OptimizationsPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QString optimization = gen::utils::cppStringModuleProperty(
                    qbsProps, QStringLiteral("optimization"));
        if (optimization == QLatin1String("none")) {
            optimizationStrategy = OptimizationsPageOptions::StrategyBalanced;
            optimizationLevel = OptimizationsPageOptions::LevelNone;
            optimizationLevelSlave = OptimizationsPageOptions::LevelSlave0;
        } else if (optimization == QLatin1String("fast")) {
            optimizationStrategy = OptimizationsPageOptions::StrategySpeed;
            optimizationLevel = OptimizationsPageOptions::LevelHigh;
            optimizationLevelSlave = OptimizationsPageOptions::LevelSlave3;
        } else if (optimization == QLatin1String("small")) {
            optimizationStrategy = OptimizationsPageOptions::StrategySize;
            optimizationLevel = OptimizationsPageOptions::LevelHigh;
            optimizationLevelSlave = OptimizationsPageOptions::LevelSlave3;
        }

        const QStringList flags = IarewUtils::cppModuleCompilerFlags(qbsProps);

        disableSizeConstraints = flags.contains(
                    QLatin1String("--no_size_constraints"));

        enableCommonSubexpressionElimination = !flags.contains(
                    QLatin1String("--no_cse"));
        enableLoopUnroll = !flags.contains(QLatin1String("--no_unroll"));
        enableFunctionInlining = !flags.contains(QLatin1String("--no_inline"));
        enableCodeMotion = !flags.contains(QLatin1String("--no_code_motion"));
        enableTypeBasedAliasAnalysis = !flags.contains(
                    QLatin1String("--no_tbaa"));
     }

    Strategy optimizationStrategy = StrategyBalanced;
    Level optimizationLevel = LevelNone;
    LevelSlave optimizationLevelSlave = LevelSlave0;
    // Separate "no size constraints" checkbox.
    int disableSizeConstraints = 0;

    // Five bit-field flags on "enabled transformations" widget.
    int enableCommonSubexpressionElimination = 0; // Common sub-expression elimination.
    int enableLoopUnroll = 0; // Loop unrolling.
    int enableFunctionInlining = 0; // Function inlining.
    int enableCodeMotion = 0; // Code motion.
    int enableTypeBasedAliasAnalysis = 0; // Type based alias analysis.
};

// Preprocessor page options.

struct PreprocessorPageOptions final
{
    explicit PreprocessorPageOptions(const QString &baseDirectory,
                                     const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        defineSymbols = gen::utils::cppVariantModuleProperties(
                    qbsProps, {QStringLiteral("defines")});

        const QString toolkitPath = IarewUtils::toolkitRootPath(qbsProduct);
        const QStringList fullIncludePaths = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("includePaths"),
                               QStringLiteral("systemIncludePaths")});
        for (const QString &fullIncludePath : fullIncludePaths) {
            const QFileInfo includeFileInfo(fullIncludePath);
            const QString includeFilePath = includeFileInfo.absoluteFilePath();
            if (includeFilePath.startsWith(toolkitPath, Qt::CaseInsensitive)) {
                const QString path = IarewUtils::toolkitRelativeFilePath(
                            toolkitPath, includeFilePath);
                includePaths.push_back(path);
            } else {
                const QString path = IarewUtils::projectRelativeFilePath(
                            baseDirectory, includeFilePath);
                includePaths.push_back(path);
            }
        }
    }

    QVariantList defineSymbols;
    QVariantList includePaths;
};

// Diagnostics page options.

struct DiagnosticsPageOptions final
{
    explicit DiagnosticsPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        warningsAsErrors = gen::utils::cppIntegerModuleProperty(
                    qbsProps, QStringLiteral("treatWarningsAsErrors"));
    }

    int warningsAsErrors = 0;
};

} // namespace

//Msp430CompilerSettingsGroup

Msp430CompilerSettingsGroup::Msp430CompilerSettingsGroup(
        const Project &qbsProject,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps)
{
    Q_UNUSED(qbsProductDeps)

    setName(QByteArrayLiteral("ICC430"));
    setArchiveVersion(kCompilerArchiveVersion);
    setDataVersion(kCompilerDataVersion);
    setDataDebugInfo(gen::utils::debugInformation(qbsProduct));

    const QString buildRootDirectory = gen::utils::buildRootPath(qbsProject);

    buildOutputPage(qbsProduct);
    buildLanguageOnePage(qbsProduct);
    buildLanguageTwoPage(qbsProduct);
    buildOptimizationsPage(qbsProduct);
    buildPreprocessorPage(buildRootDirectory, qbsProduct);
    buildDiagnosticsPage(qbsProduct);
}

void Msp430CompilerSettingsGroup::buildOutputPage(
        const ProductData &qbsProduct)
{
    const OutputPageOptions opts(qbsProduct);
    // Add 'CCDebugInfo' item (Generate debug info).
    addOptionsGroup(QByteArrayLiteral("CCDebugInfo"),
                    {opts.debugInfo});
}

void Msp430CompilerSettingsGroup::buildLanguageOnePage(
        const ProductData &qbsProduct)
{
    const LanguageOnePageOptions opts(qbsProduct);
    // Add 'IccLang' item with 'auto-extension based'
    // value (Language: C/C++/Auto).
    addOptionsGroup(QByteArrayLiteral("IccLang"),
                    {opts.languageExtension});
    // Add 'IccCDialect' item (C dialect: c89/99/11).
    addOptionsGroup(QByteArrayLiteral("IccCDialect"),
                    {opts.cLanguageDialect});
    // Add 'IccCppDialect' item (C++ dialect: embedded/extended).
    addOptionsGroup(QByteArrayLiteral("IccCppDialect"),
                    {opts.cxxLanguageDialect});
    // Add 'CCExt' item
    // (Language conformance: IAR/relaxed/strict).
    addOptionsGroup(QByteArrayLiteral("CCExt"),
                    {opts.languageConformance});
    // Add 'IccAllowVLA' item (Allow VLA).
    addOptionsGroup(QByteArrayLiteral("IccAllowVLA"),
                    {opts.allowVla});
    // Add 'IccCppInlineSemantics' item (C++ inline semantics).
    addOptionsGroup(QByteArrayLiteral("IccCppInlineSemantics"),
                    {opts.useCppInlineSemantics});
    // Add 'IccRequirePrototypes' item (Require prototypes).
    addOptionsGroup(QByteArrayLiteral("CCRequirePrototypes"),
                    {opts.requirePrototypes});
    // Add 'IccStaticDestr' item (Destroy static objects).
    addOptionsGroup(QByteArrayLiteral("IccStaticDestr"),
                    {opts.destroyStaticObjects});
}

void Msp430CompilerSettingsGroup::buildLanguageTwoPage(
        const ProductData &qbsProduct)
{
    const LanguageTwoPageOptions opts(qbsProduct);
    // Add 'IccCharIs' item (Plain char is: signed/unsigned).
    addOptionsGroup(QByteArrayLiteral("CCCharIs"),
                    {opts.plainCharacter});
    // Add 'IccFloatSemantics' item (Floatic-point
    // semantics: strict/relaxed conformance).
    addOptionsGroup(QByteArrayLiteral("IccFloatSemantics"),
                    {opts.floatingPointSemantic});
    // Add 'IccMultibyteSupport' item (Enable multibyte support).
    addOptionsGroup(QByteArrayLiteral("CCMultibyteSupport"),
                    {opts.enableMultibyteSupport});
    // Add 'CCGuardCalls' and 'OCGuardCallsSlave' item (Guard calls).
    addOptionsGroup(QByteArrayLiteral("CCGuardCalls"),
                    {opts.guardCalls});
}

void Msp430CompilerSettingsGroup::buildCodePage(
        const ProductData &qbsProduct)
{
    const CodePageOptions opts(qbsProduct);
    // Add 'OCCR4Utilize' item
    // (R4 utilization: normal/regvar/disabled).
    addOptionsGroup(QByteArrayLiteral("OCCR4Utilize"),
                    {opts.r4utilization});
    // Add 'OCCR5Utilize' item
    // (R5 utilization: normal/regvar/disabled).
    addOptionsGroup(QByteArrayLiteral("OCCR5Utilize"),
                    {opts.r5utilization});
    // Add 'ReduceStack' item
    // (Reduce stack usage).
    addOptionsGroup(QByteArrayLiteral("ReduceStack"),
                    {opts.reduceStackUsage});
    // Add 'Save20bit' item
    // (20-bit context save on interrupt).
    addOptionsGroup(QByteArrayLiteral("Save20bit"),
                    {opts.save20BitContextOnInterrupt});
}

void Msp430CompilerSettingsGroup::buildOptimizationsPage(
        const ProductData &qbsProduct)
{
    const OptimizationsPageOptions opts(qbsProduct);
    // Add 'CCOptStrategy', 'CCOptLevel' and
    // 'CCOptLevelSlave' items (Level).
    addOptionsGroup(QByteArrayLiteral("CCOptStrategy"),
                    {opts.optimizationStrategy});
    addOptionsGroup(QByteArrayLiteral("CCOptLevel"),
                    {opts.optimizationLevel});
    addOptionsGroup(QByteArrayLiteral("CCOptLevelSlave"),
                    {opts.optimizationLevelSlave});

    // Add 'CCOptimizationNoSizeConstraints' item (no size constraints).
    addOptionsGroup(QByteArrayLiteral("CCOptimizationNoSizeConstraints"),
                    {opts.disableSizeConstraints});

    // Add 'CCAllowList' item
    // (Enabled transformations: 5 check boxes).
    const QString bitflags = QStringLiteral("%1%2%3%4%5%6")
            .arg(opts.enableCommonSubexpressionElimination)
            .arg(opts.enableLoopUnroll)
            .arg(opts.enableFunctionInlining)
            .arg(opts.enableCodeMotion)
            .arg(opts.enableTypeBasedAliasAnalysis);
    addOptionsGroup(QByteArrayLiteral("CCAllowList"),
                    {bitflags});
}

void Msp430CompilerSettingsGroup::buildPreprocessorPage(
        const QString &baseDirectory,
        const ProductData &qbsProduct)
{
    const PreprocessorPageOptions opts(baseDirectory, qbsProduct);
    // Add 'CCDefines' item (Defines symbols).
    addOptionsGroup(QByteArrayLiteral("CCDefines"),
                    opts.defineSymbols);
    // Add 'newCCIncludePaths' item
    // (Additional include directories).
    addOptionsGroup(QByteArrayLiteral("newCCIncludePaths"),
                    opts.includePaths);
}

void Msp430CompilerSettingsGroup::buildDiagnosticsPage(
        const ProductData &qbsProduct)
{
    const DiagnosticsPageOptions opts(qbsProduct);
    // Add 'CCDiagWarnAreErr' item (Treat all warnings as errors).
    addOptionsGroup(QByteArrayLiteral("CCDiagWarnAreErr"),
                    {opts.warningsAsErrors});
}

} // namespace v7
} // namespace msp430
} // namespace iarew
} // namespace qbs
