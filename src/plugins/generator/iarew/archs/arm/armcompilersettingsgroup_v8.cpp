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

#include "armcompilersettingsgroup_v8.h"

#include "../../iarewutils.h"

namespace qbs {
namespace iarew {
namespace arm {
namespace v8 {

constexpr int kCompilerArchiveVersion = 2;
constexpr int kCompilerDataVersion = 34;

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

// Language 1 page options.

struct LanguageOnePageOptions final
{
    enum LanguageExtension {
        CLanguageExtension,
        CxxLanguageExtension,
        AutoLanguageExtension
    };

    enum CLanguageDialect {
        C89LanguageDialect,
        C11LanguageDialect
    };

    enum LanguageConformance {
        AllowIarExtension,
        RelaxedStandard,
        StrictStandard
    };

    explicit LanguageOnePageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        // File extension based by default.
        languageExtension = LanguageOnePageOptions::AutoLanguageExtension;
        // Language dialect.
        const QStringList cLanguageVersion = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("cLanguageVersion")});
        cLanguageDialect = cLanguageVersion.contains(QLatin1String("c89"))
                ? LanguageOnePageOptions::C89LanguageDialect
                : LanguageOnePageOptions::C11LanguageDialect;

        const QStringList flags = IarewUtils::cppModuleCompilerFlags(qbsProps);
        // Language conformance.
        if (flags.contains(QLatin1String("-e")))
            languageConformance = LanguageOnePageOptions::AllowIarExtension;
        else if (flags.contains(QLatin1String("--strict")))
            languageConformance = LanguageOnePageOptions::StrictStandard;
        else
            languageConformance = LanguageOnePageOptions::RelaxedStandard;
        // Exceptions, rtti, static desrtuction.
        enableExceptions = !flags.contains(QLatin1String("--no_exceptions"));
        enableRtti = !flags.contains(QLatin1String("--no_rtti"));
        destroyStaticObjects = !flags.contains(
                    QLatin1String("--no_static_destruction"));
        allowVla = flags.contains(QLatin1String("--vla"));
        enableInlineSemantics = flags.contains(QLatin1String("--use_c++_inline"));
        requirePrototypes = flags.contains(QLatin1String("--require_prototypes"));
    }

    LanguageExtension languageExtension = AutoLanguageExtension;
    CLanguageDialect cLanguageDialect = C89LanguageDialect;
    LanguageConformance languageConformance = AllowIarExtension;
    // C++ options.
    int enableExceptions = 0;
    int enableRtti = 0;
    int destroyStaticObjects = 0;
    int allowVla = 0;
    int enableInlineSemantics = 0;
    int requirePrototypes = 0;
};

// Language 2 page options.

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
    }

    PlainCharacter plainCharacter = SignedCharacter;
    FloatingPointSemantic floatingPointSemantic = StrictSemantic;
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
        enableCommonSubexpressionElimination = !flags.contains(
                    QLatin1String("--no_cse"));
        enableLoopUnroll = !flags.contains(
                    QLatin1String("--no_unroll"));
        enableFunctionInlining = !flags.contains(
                    QLatin1String("--no_inline"));
        enableCodeMotion = !flags.contains(
                    QLatin1String("--no_code_motion"));
        enableTypeBasedAliasAnalysis = !flags.contains(
                    QLatin1String("--no_tbaa"));
        enableStaticClustering = !flags.contains(
                    QLatin1String("--no_clustering"));
        enableInstructionScheduling = !flags.contains(
                    QLatin1String("--no_scheduling"));
        enableVectorization = flags.contains(
                    QLatin1String("--vectorize"));
        disableSizeConstraints = flags.contains(
                    QLatin1String("--no_size_constraints"));
    }

    Strategy optimizationStrategy = StrategyBalanced;
    Level optimizationLevel = LevelNone;
    LevelSlave optimizationLevelSlave = LevelSlave0;
    // Eight bit-field flags on "enabled optimizations" widget.
    int enableCommonSubexpressionElimination = 0; // Common sub-expression elimination.
    int enableLoopUnroll = 0; // Loop unrolling.
    int enableFunctionInlining = 0; // Function inlining.
    int enableCodeMotion = 0; // Code motion.
    int enableTypeBasedAliasAnalysis = 0; // Type-based alias analysis.
    int enableStaticClustering = 0; // Static clustering.
    int enableInstructionScheduling = 0; // Instruction scheduling.
    int enableVectorization = 0; // Vectorization.

    // Separate "no size constraints" checkbox.
    int disableSizeConstraints = 0;
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
        treatWarningsAsErrors = gen::utils::cppIntegerModuleProperty(
                    qbsProps, QStringLiteral("treatWarningsAsErrors"));
    }

    int treatWarningsAsErrors = 0;
};

// Code page options.

struct CodePageOptions final
{
    enum ProcessorMode {
        CpuArmMode,
        CpuThumbMode
    };

    explicit CodePageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleCompilerFlags(qbsProps);
        const QString cpuModeValue = IarewUtils::flagValue(
                    flags, QStringLiteral("--cpu_mode"));
        if (cpuModeValue == QLatin1String("thumb"))
            cpuMode = CodePageOptions::CpuThumbMode;
        else if (cpuModeValue == QLatin1String("arm"))
            cpuMode = CodePageOptions::CpuArmMode;

        generateReadOnlyPosIndependentCode = flags.contains(
                    QLatin1String("--ropi"));
        generateReadWritePosIndependentCode = flags.contains(
                    QLatin1String("--rwpi"));
        disableDynamicReadWriteInitialization = flags.contains(
                    QLatin1String("--no_rw_dynamic_init"));
        disableCodeMemoryDataReads = flags.contains(
                    QLatin1String("--no_literal_pool"));
    }

    ProcessorMode cpuMode = CpuThumbMode;
    int generateReadOnlyPosIndependentCode = 0;
    int generateReadWritePosIndependentCode = 0;
    int disableDynamicReadWriteInitialization = 0;
    int disableCodeMemoryDataReads = 0;
};

} // namespace

// ArmCompilerSettingsGroup

ArmCompilerSettingsGroup::ArmCompilerSettingsGroup(
        const Project &qbsProject,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps)
{
    Q_UNUSED(qbsProject)
    Q_UNUSED(qbsProductDeps)

    setName(QByteArrayLiteral("ICCARM"));
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
    buildCodePage(qbsProduct);
}

void ArmCompilerSettingsGroup::buildOutputPage(
            const ProductData &qbsProduct)
{
    const OutputPageOptions opts(qbsProduct);
    // Add 'CCDebugInfo' item (Generate debug info).
    addOptionsGroup(QByteArrayLiteral("CCDebugInfo"),
                    {opts.debugInfo});
}

void ArmCompilerSettingsGroup::buildLanguageOnePage(
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
    // Add 'CCExt' item (Language conformance: IAR/relaxed/strict).
    addOptionsGroup(QByteArrayLiteral("CCLangConformance"),
                    {opts.languageConformance});
    // Add 'IccExceptions2' item (Enable exceptions).
    addOptionsGroup(QByteArrayLiteral("IccExceptions2"),
                    {opts.enableExceptions});
    // Add 'IccRTTI2' item (Enable RTTI).
    addOptionsGroup(QByteArrayLiteral("IccRTTI2"),
                    {opts.enableRtti});
    // Add 'IccStaticDestr' item (Destroy static objects).
    addOptionsGroup(QByteArrayLiteral("IccStaticDestr"),
                    {opts.destroyStaticObjects});
    // Add 'IccAllowVLA' item (Allow VLA).
    addOptionsGroup(QByteArrayLiteral("IccAllowVLA"),
                    {opts.allowVla});
    // Add 'IccCppInlineSemantics' item (C++ inline semantics).
    addOptionsGroup(QByteArrayLiteral("IccCppInlineSemantics"),
                    {opts.enableInlineSemantics});
    // Add 'CCRequirePrototypes' item (Require prototypes).
    addOptionsGroup(QByteArrayLiteral("CCRequirePrototypes"),
                    {opts.requirePrototypes});
}

void ArmCompilerSettingsGroup::buildLanguageTwoPage(
        const ProductData &qbsProduct)
{
    const LanguageTwoPageOptions opts(qbsProduct);
    // Add 'CCSignedPlainChar' item (Plain char is: signed/unsigned).
    addOptionsGroup(QByteArrayLiteral("CCSignedPlainChar"),
                    {opts.plainCharacter});
    // Add 'IccFloatSemantics' item
    // (Floating-point semantic: strict/relaxed).
    addOptionsGroup(QByteArrayLiteral("IccFloatSemantics"),
                    {opts.floatingPointSemantic});
}

void ArmCompilerSettingsGroup::buildOptimizationsPage(
        const ProductData &qbsProduct)
{
    const OptimizationsPageOptions opts(qbsProduct);
    // Add 'CCOptStrategy', 'CCOptLevel'
    // and 'CCOptLevelSlave' items (Level).
    addOptionsGroup(QByteArrayLiteral("CCOptStrategy"),
                    {opts.optimizationStrategy});
    addOptionsGroup(QByteArrayLiteral("CCOptLevel"),
                    {opts.optimizationLevel});
    addOptionsGroup(QByteArrayLiteral("CCOptLevelSlave"),
                    {opts.optimizationLevelSlave});
    // Add 'CCAllowList' item (Enabled optimizations: 6 check boxes).
    const QString transformations = QStringLiteral("%1%2%3%4%5%6%7%8")
            .arg(opts.enableCommonSubexpressionElimination)
            .arg(opts.enableLoopUnroll)
            .arg(opts.enableFunctionInlining)
            .arg(opts.enableCodeMotion)
            .arg(opts.enableTypeBasedAliasAnalysis)
            .arg(opts.enableStaticClustering)
            .arg(opts.enableInstructionScheduling)
            .arg(opts.enableVectorization);
    addOptionsGroup(QByteArrayLiteral("CCAllowList"),
                    {transformations});
    // Add 'CCOptimizationNoSizeConstraints' item (No size constraints).
    addOptionsGroup(QByteArrayLiteral("CCOptimizationNoSizeConstraints"),
                    {opts.disableSizeConstraints});
}

void ArmCompilerSettingsGroup::buildPreprocessorPage(
        const QString &baseDirectory,
        const ProductData &qbsProduct)
{
    const PreprocessorPageOptions opts(baseDirectory, qbsProduct);
    // Add 'CCDefines' item (Defined symbols).
    addOptionsGroup(QByteArrayLiteral("CCDefines"),
                    opts.defineSymbols);
    // Add 'CCIncludePath2' item (Additional include directories).
    addOptionsGroup(QByteArrayLiteral("CCIncludePath2"),
                    opts.includePaths);
}

void ArmCompilerSettingsGroup::buildDiagnosticsPage(
        const ProductData &qbsProduct)
{
    const DiagnosticsPageOptions opts(qbsProduct);
    // Add 'CCDiagWarnAreErr' item (Treat all warnings as errors).
    addOptionsGroup(QByteArrayLiteral("CCDiagWarnAreErr"),
                    {opts.treatWarningsAsErrors});
}

void ArmCompilerSettingsGroup::buildCodePage(
        const ProductData &qbsProduct)
{
    const CodePageOptions opts(qbsProduct);
    // Add 'IProcessorMode2' item (Processor mode: arm/thumb).
    addOptionsGroup(QByteArrayLiteral("IProcessorMode2"),
                    {opts.cpuMode});
    // Add 'CCPosIndRopi' item (Code and read-only data "ropi").
    addOptionsGroup(QByteArrayLiteral("CCPosIndRopi"),
                    {opts.generateReadOnlyPosIndependentCode});
    // Add 'CCPosIndRwpi' item (Read/write data "rwpi").
    addOptionsGroup(QByteArrayLiteral("CCPosIndRwpi"),
                    {opts.generateReadWritePosIndependentCode});
    // Add 'CCPosIndNoDynInit' item (No dynamic read/write initialization).
    addOptionsGroup(QByteArrayLiteral("CCPosIndNoDynInit"),
                    {opts.disableDynamicReadWriteInitialization});
    // Add 'CCNoLiteralPool' item (No data reads in code memory).
    addOptionsGroup(QByteArrayLiteral("CCNoLiteralPool"),
                    {opts.disableCodeMemoryDataReads});
}

} // namespace v8
} // namespace arm
} // namespace iarew
} // namespace qbs
