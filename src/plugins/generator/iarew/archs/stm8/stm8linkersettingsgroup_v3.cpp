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

#include "stm8linkersettingsgroup_v3.h"

#include "../../iarewutils.h"

#include <QtCore/qdir.h>

namespace qbs {
namespace iarew {
namespace stm8 {
namespace v3 {

constexpr int kLinkerArchiveVersion = 5;
constexpr int kLinkerDataVersion = 4;

namespace {

// Config page options.

struct ConfigPageOptions final
{
    explicit ConfigPageOptions(const QString &baseDirectory,
                               const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QString toolkitPath = IarewUtils::toolkitRootPath(qbsProduct);

        // Enumerate all product linker config files
        // (which are set trough 'linkerscript' tag).
        for (const auto &qbsGroup : qbsProduct.groups()) {
            const auto qbsArtifacts = qbsGroup.sourceArtifacts();
            for (const auto &qbsArtifact : qbsArtifacts) {
                const auto qbsTags = qbsArtifact.fileTags();
                if (!qbsTags.contains(QLatin1String("linkerscript")))
                    continue;
                const QString fullConfigPath = qbsArtifact.filePath();
                if (fullConfigPath.startsWith(toolkitPath, Qt::CaseInsensitive)) {
                    const QString path = IarewUtils::toolkitRelativeFilePath(
                                toolkitPath, fullConfigPath);
                    configFilePaths.push_back(path);
                } else {
                    const QString path = IarewUtils::projectRelativeFilePath(
                                baseDirectory, fullConfigPath);
                    configFilePaths.push_back(path);
                }
            }
        }

        // Enumerate all product linker config files
        // (which are set trough '-config' option).
        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);
        const QVariantList configPathValues = IarewUtils::flagValues(
                    flags, QStringLiteral("--config"));
        for (const QVariant &configPathValue : configPathValues) {
            const QString fullConfigPath = configPathValue.toString();
            if (fullConfigPath.startsWith(toolkitPath, Qt::CaseInsensitive)) {
                const QString path = IarewUtils::toolkitRelativeFilePath(
                            toolkitPath, fullConfigPath);
                if (!configFilePaths.contains(path))
                    configFilePaths.push_back(path);
            } else {
                const QString path = IarewUtils::projectRelativeFilePath(
                            baseDirectory, fullConfigPath);
                if (!configFilePaths.contains(path))
                    configFilePaths.push_back(path);
            }
        }

        // Enumerate all config definition symbols (except
        // the CSTACK_SIZE and HEAP_SIZE which are handles
        // on the general page).
        configDefinitions = IarewUtils::flagValues(
                    flags, QStringLiteral("--config_def"));
        configDefinitions.erase(std::remove_if(
                                    configDefinitions.begin(),
                                    configDefinitions.end(),
                                    [](const auto &definition){
            const auto def = definition.toString();
            return def.startsWith(QLatin1String("_CSTACK_SIZE"))
                    || def.startsWith(QLatin1String("_HEAP_SIZE"));
        }), configDefinitions.end());
    }

    QVariantList configFilePaths;
    QVariantList configDefinitions;
};

struct LibraryPageOptions final
{
    explicit LibraryPageOptions(const QString &baseDirectory,
                                const ProductData &qbsProduct,
                                const std::vector<ProductData> &qbsProductDeps)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QString toolkitPath = IarewUtils::toolkitRootPath(qbsProduct);

        entryPoint = gen::utils::cppStringModuleProperty(
                    qbsProps, QStringLiteral("entryPoint"));

        // Add static libraries paths.
        const QStringList staticLibrariesProps =
                gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("staticLibraries")});
        for (const QString &staticLibrary : staticLibrariesProps) {
            const QFileInfo staticLibraryInfo(staticLibrary);
            if (staticLibraryInfo.isAbsolute()) {
                const QString fullStaticLibraryPath =
                        staticLibraryInfo.absoluteFilePath();
                if (fullStaticLibraryPath.startsWith(toolkitPath,
                                                     Qt::CaseInsensitive)) {
                    const QString path = IarewUtils::toolkitRelativeFilePath(
                                toolkitPath, fullStaticLibraryPath);
                    staticLibraries.push_back(path);
                } else {
                    const QString path = IarewUtils::projectRelativeFilePath(
                                baseDirectory, fullStaticLibraryPath);
                    staticLibraries.push_back(path);
                }
            } else {
                staticLibraries.push_back(staticLibrary);
            }
        }

        // Add static libraries from product dependencies.
        for (const ProductData &qbsProductDep : qbsProductDeps) {
            const QString depBinaryPath = QLatin1String("$PROJ_DIR$/")
                    + gen::utils::targetBinaryPath(baseDirectory,
                                                   qbsProductDep);
            staticLibraries.push_back(depBinaryPath);
        }
    }

    QString entryPoint;
    QVariantList staticLibraries;
};

struct OptimizationsPageOptions final
{
    explicit OptimizationsPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);

        mergeDuplicateSections = flags.contains(
                    QLatin1String("--merge_duplicate_sections"));
    }

    bool mergeDuplicateSections = true;
};

// Output page options.

struct OutputPageOptions final
{
    explicit OutputPageOptions(const ProductData &qbsProduct)
    {
        outputFile = gen::utils::targetBinary(qbsProduct);
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);

        enableDebugInfo = !flags.contains(QLatin1String("--strip"));
    }

    QString outputFile;
    bool enableDebugInfo = true;
};

// List page options.

struct ListPageOptions final
{
    enum ListingAction {
        NoListing,
        GenerateListing
    };

    explicit ListPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        generateMap = gen::utils::cppBooleanModuleProperty(
                    qbsProps, QStringLiteral("generateLinkerMapFile"))
                ? ListPageOptions::GenerateListing
                : ListPageOptions::NoListing;
    }

    ListingAction generateMap = NoListing;
};

// Define page options.

struct DefinePageOptions final
{
    explicit DefinePageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);

        defineSymbols = IarewUtils::flagValues(
                    flags, QStringLiteral("--define_symbol"));
    }

    QVariantList defineSymbols;
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

// Stm8LinkerSettingsGroup

Stm8LinkerSettingsGroup::Stm8LinkerSettingsGroup(
        const Project &qbsProject,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps)
{
    setName(QByteArrayLiteral("ILINK"));
    setArchiveVersion(kLinkerArchiveVersion);
    setDataVersion(kLinkerDataVersion);
    setDataDebugInfo(gen::utils::debugInformation(qbsProduct));

    const QString buildRootDirectory = gen::utils::buildRootPath(qbsProject);

    buildConfigPage(buildRootDirectory, qbsProduct);
    buildLibraryPage(buildRootDirectory, qbsProduct, qbsProductDeps);
    buildOptimizationsPage(qbsProduct);
    buildOutputPage(qbsProduct);
    buildListPage(qbsProduct);
    buildDefinePage(qbsProduct);
    buildDiagnosticsPage(qbsProduct);

    // Should be called as latest stage!
    buildExtraOptionsPage(qbsProduct);
}

void Stm8LinkerSettingsGroup::buildConfigPage(
        const QString &baseDirectory,
        const ProductData &qbsProduct)
{
    ConfigPageOptions opts(baseDirectory, qbsProduct);

    if (opts.configFilePaths.count() > 0) {
        // Note: IAR IDE does not allow to specify a multiple config files,
        // although the IAR linker support it. So, we use followig 'trick':
        // we take a first config file and to add it as usual to required items;
        // and then an other remainders we forward to the "Extra options page".
        const QVariant configPath = opts.configFilePaths.takeFirst();
        // Add 'IlinkIcfOverride' item (Override default).
        addOptionsGroup(QByteArrayLiteral("IlinkIcfOverride"),
                        {1});
        // Add 'IlinkIcfFile' item (Linke configuration file).
        addOptionsGroup(QByteArrayLiteral("IlinkIcfFile"),
                        {configPath});

        // Add remainder configuration files to the "Extra options page".
        if (!opts.configFilePaths.isEmpty()) {
            for (QVariant &configPath : opts.configFilePaths)
                configPath = QLatin1String("--config ") + configPath.toString();

            m_extraOptions << opts.configFilePaths;
        }
    }

    // Add 'IlinkConfigDefines' item (Configuration file
    // symbol definitions).
    addOptionsGroup(QByteArrayLiteral("IlinkConfigDefines"),
                    opts.configDefinitions);
}

void Stm8LinkerSettingsGroup::buildLibraryPage(
        const QString &baseDirectory,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps)
{
    LibraryPageOptions opts(baseDirectory, qbsProduct, qbsProductDeps);

    // Add 'IlinkOverrideProgramEntryLabel' item
    // (Override default program entry).
    addOptionsGroup(QByteArrayLiteral("IlinkOverrideProgramEntryLabel"),
                    {1});

    if (opts.entryPoint.isEmpty()) {
        // Add 'IlinkProgramEntryLabelSelect' item
        // (Defined by application).
        addOptionsGroup(QByteArrayLiteral("IlinkProgramEntryLabelSelect"),
                        {1});
    } else {
        // Add 'IlinkProgramEntryLabel' item
        // (Entry symbol).
        addOptionsGroup(QByteArrayLiteral("IlinkProgramEntryLabel"),
                        {opts.entryPoint});
    }

    // Add 'IlinkAdditionalLibs' item (Additional libraries).
    addOptionsGroup(QByteArrayLiteral("IlinkAdditionalLibs"),
                    {opts.staticLibraries});
}

void Stm8LinkerSettingsGroup::buildOptimizationsPage(
        const ProductData &qbsProduct)
{
    OptimizationsPageOptions opts(qbsProduct);

    // Add 'IlinkOptMergeDuplSections' item
    // (Merge duplicate sections).
    addOptionsGroup(QByteArrayLiteral("IlinkOptMergeDuplSections"),
                    {opts.mergeDuplicateSections});
}

void Stm8LinkerSettingsGroup::buildOutputPage(
        const ProductData &qbsProduct)
{
    const OutputPageOptions opts(qbsProduct);

    // Add 'IlinkOutputFile' item (Output file name).
    addOptionsGroup(QByteArrayLiteral("IlinkOutputFile"),
                    {opts.outputFile});
    // Add 'IlinkDebugInfoEnable' item
    // (Include debug information in output).
    addOptionsGroup(QByteArrayLiteral("IlinkDebugInfoEnable"),
                    {opts.enableDebugInfo});
}

void Stm8LinkerSettingsGroup::buildListPage(
        const ProductData &qbsProduct)
{
    const ListPageOptions opts(qbsProduct);
    // Add 'IlinkMapFile' item (Generate linker map file).
    addOptionsGroup(QByteArrayLiteral("IlinkMapFile"),
                    {opts.generateMap});
}

void Stm8LinkerSettingsGroup::buildDefinePage(
        const ProductData &qbsProduct)
{
    const DefinePageOptions opts(qbsProduct);
    // Add 'IlinkDefines' item (Defined symbols).
    addOptionsGroup(QByteArrayLiteral("IlinkDefines"),
                    opts.defineSymbols);
}

void Stm8LinkerSettingsGroup::buildDiagnosticsPage(
        const ProductData &qbsProduct)
{
    const DiagnosticsPageOptions opts(qbsProduct);
    // Add 'IlinkWarningsAreErrors' item (Treat all warnings as errors).
    addOptionsGroup(QByteArrayLiteral("IlinkWarningsAreErrors"),
                    {opts.warningsAsErrors});
}

void Stm8LinkerSettingsGroup::buildExtraOptionsPage(
        const ProductData &qbsProduct)
{
    Q_UNUSED(qbsProduct)

    if (m_extraOptions.isEmpty())
        return;

    // Add 'IlinkUseExtraOptions' (Use command line options).
    addOptionsGroup(QByteArrayLiteral("IlinkUseExtraOptions"),
                    {1});
    // Add 'IlinkExtraOptions' item (Command line options).
    addOptionsGroup(QByteArrayLiteral("IlinkExtraOptions"),
                    m_extraOptions);
}

} // namespace v3
} // namespace stm8
} // namespace iarew
} // namespace qbs
