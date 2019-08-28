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

#include "stm8assemblersettingsgroup_v3.h"

#include "../../iarewutils.h"

namespace qbs {
namespace iarew {
namespace stm8 {
namespace v3 {

constexpr int kAssemblerArchiveVersion = 3;
constexpr int kAssemblerDataVersion = 2;

namespace {

// Language page options.

struct LanguagePageOptions final
{
    enum MacroQuoteCharacter {
        AngleBracketsQuote,
        RoundBracketsQuote,
        SquareBracketsQuote,
        FigureBracketsQuote
    };

    explicit LanguagePageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("assemblerFlags")});
        enableSymbolsCaseSensitive = !flags.contains(
                    QLatin1String("--case_insensitive"));
        enableMultibyteSupport = flags.contains(
                    QLatin1String("--enable_multibytes"));
        allowFirstColumnMnemonics = flags.contains(
                    QLatin1String("--mnem_first"));
        allowFirstColumnDirectives = flags.contains(
                    QLatin1String("--dir_first"));

        if (flags.contains(QLatin1String("-M<>")))
            macroQuoteCharacter = LanguagePageOptions::AngleBracketsQuote;
        else if (flags.contains(QLatin1String("-M()")))
            macroQuoteCharacter = LanguagePageOptions::RoundBracketsQuote;
        else if (flags.contains(QLatin1String("-M[]")))
            macroQuoteCharacter = LanguagePageOptions::SquareBracketsQuote;
        else if (flags.contains(QLatin1String("-M{}")))
            macroQuoteCharacter = LanguagePageOptions::FigureBracketsQuote;
    }

    int enableSymbolsCaseSensitive = 1;
    int enableMultibyteSupport = 0;
    int allowFirstColumnMnemonics = 0;
    int allowFirstColumnDirectives = 0;

    MacroQuoteCharacter macroQuoteCharacter = AngleBracketsQuote;
};

// Output page options.

struct OutputPageOptions final
{
    explicit OutputPageOptions(const ProductData &qbsProduct)
    {
        debugInfo = gen::utils::debugInformation(qbsProduct);
    }

    int debugInfo = 0;
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
        for (const auto &fullIncludePath : fullIncludePaths) {
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

// Stm8AssemblerSettingsGroup

Stm8AssemblerSettingsGroup::Stm8AssemblerSettingsGroup(
        const Project &qbsProject,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps)
{
    Q_UNUSED(qbsProductDeps)

    setName(QByteArrayLiteral("ASTM8"));
    setArchiveVersion(kAssemblerArchiveVersion);
    setDataVersion(kAssemblerDataVersion);
    setDataDebugInfo(gen::utils::debugInformation(qbsProduct));

    const QString buildRootDirectory = gen::utils::buildRootPath(qbsProject);

    buildLanguagePage(qbsProduct);
    buildOutputPage(qbsProduct);
    buildPreprocessorPage(buildRootDirectory, qbsProduct);
    buildDiagnosticsPage(qbsProduct);
}

void Stm8AssemblerSettingsGroup::buildLanguagePage(
        const ProductData &qbsProduct)
{
    const LanguagePageOptions opts(qbsProduct);
    // Add 'AsmCaseSensitivity' item (User symbols are case sensitive).
    addOptionsGroup(QByteArrayLiteral("AsmCaseSensitivity"),
                    {opts.enableSymbolsCaseSensitive});
    // Add 'AsmMultibyteSupport' item (Enable multibyte support).
    addOptionsGroup(QByteArrayLiteral("AsmMultibyteSupport"),
                    {opts.enableMultibyteSupport});
    // Add 'AsmAllowMnemonics' item (Allow mnemonics in first column).
    addOptionsGroup(QByteArrayLiteral("AsmAllowMnemonics"),
                    {opts.allowFirstColumnMnemonics});
    // Add 'AsmAllowDirectives' item (Allow directives in first column).
    addOptionsGroup(QByteArrayLiteral("AsmAllowDirectives"),
                    {opts.allowFirstColumnDirectives});

    // Add 'AsmMacroChars' item (Macro quote characters: ()/[]/{}/<>).
    addOptionsGroup(QByteArrayLiteral("AsmMacroChars"),
                    {opts.macroQuoteCharacter});
}

void Stm8AssemblerSettingsGroup::buildOutputPage(
        const ProductData &qbsProduct)
{
    const OutputPageOptions opts(qbsProduct);
    // Add 'AsmDebugInfo' item (Generate debug information).
    addOptionsGroup(QByteArrayLiteral("AsmDebugInfo"),
                    {opts.debugInfo});
}

void Stm8AssemblerSettingsGroup::buildPreprocessorPage(
        const QString &baseDirectory,
        const ProductData &qbsProduct)
{
    const PreprocessorPageOptions opts(baseDirectory, qbsProduct);
    // Add 'AsmDefines' item (Defined symbols).
    addOptionsGroup(QByteArrayLiteral("AsmDefines"),
                    opts.defineSymbols);
    // Add 'AsmIncludePath' item (Additional include directories).
    addOptionsGroup(QByteArrayLiteral("AsmIncludePath"),
                    opts.includePaths);
}

void Stm8AssemblerSettingsGroup::buildDiagnosticsPage(
        const ProductData &qbsProduct)
{
    const DiagnosticsPageOptions opts(qbsProduct);
    // Add 'AsmDiagnosticsWarningsAreErrors' item.
    // (Treat all warnings as errors).
    addOptionsGroup(QByteArrayLiteral("AsmDiagnosticsWarningsAreErrors"),
                    {opts.warningsAsErrors});
}

} // namespace v3
} // namespace stm8
} // namespace iarew
} // namespace qbs
