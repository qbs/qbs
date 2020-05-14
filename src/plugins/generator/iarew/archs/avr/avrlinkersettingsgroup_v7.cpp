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

#include "avrlinkersettingsgroup_v7.h"

#include "../../iarewutils.h"

#include <QtCore/qdir.h>

namespace qbs {
namespace iarew {
namespace avr {
namespace v7 {

constexpr int kLinkerArchiveVersion = 3;
constexpr int kLinkerDataVersion = 16;

namespace {

// Config page options.

struct ConfigPageOptions final
{
    explicit ConfigPageOptions(const QString &baseDirectory,
                               const ProductData &qbsProduct,
                               const std::vector<ProductData> &qbsProductDeps)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QString toolkitPath = IarewUtils::toolkitRootPath(qbsProduct);

        entryPoint = gen::utils::cppStringModuleProperty(
                    qbsProps, QStringLiteral("entryPoint"));

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
        // (which are set trough '-f' option).
        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);
        const QVariantList configPathValues = IarewUtils::flagValues(
                    flags, QStringLiteral("-f"));
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

        // Add libraries search paths.
        const QStringList libraryPaths = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("libraryPaths")});
        for (const QString &libraryPath : libraryPaths) {
            const QFileInfo libraryPathInfo(libraryPath);
            const QString fullLibrarySearchPath =
                    libraryPathInfo.absoluteFilePath();
            if (fullLibrarySearchPath.startsWith(toolkitPath,
                                                 Qt::CaseInsensitive)) {
                const QString path = IarewUtils::toolkitRelativeFilePath(
                            toolkitPath, fullLibrarySearchPath);
                librarySearchPaths.push_back(path);
            } else {
                const QString path = IarewUtils::projectRelativeFilePath(
                            baseDirectory, fullLibrarySearchPath);
                librarySearchPaths.push_back(path);
            }
        }

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

    QVariantList configFilePaths;
    QVariantList librarySearchPaths;
    QVariantList staticLibraries;
    QString entryPoint;
};

// Output page options.

struct OutputPageOptions final
{
    explicit OutputPageOptions(const ProductData &qbsProduct)
    {
        outputFile = gen::utils::targetBinary(qbsProduct);
    }

    QString outputFile;
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
        // Enumerate all linker defines.
        for (const QString &flag : flags) {
            if (!flag.startsWith(QLatin1String("-D")))
                continue;
            const auto symbol = flag.mid(2);
            // Ignore system-defined macroses.
            if (symbol.startsWith(QLatin1String("_..X_HEAP_SIZE"))
                    || symbol.startsWith(QLatin1String("_..X_TINY_HEAP_SIZE"))
                    || symbol.startsWith(QLatin1String("_..X_NEAR_HEAP_SIZE"))
                    || symbol.startsWith(QLatin1String("_..X_FAR_HEAP_SIZE"))
                    || symbol.startsWith(QLatin1String("_..X_HUGE_HEAP_SIZE"))
                    || symbol.startsWith(QLatin1String("_..X_CSTACK_SIZE"))
                    || symbol.startsWith(QLatin1String("_..X_RSTACK_SIZE"))
                    || symbol.startsWith(QLatin1String("_..X_FLASH_CODE_END"))
                    || symbol.startsWith(QLatin1String("_..X_FLASH_BASE"))
                    || symbol.startsWith(QLatin1String("_..X_CSTACK_BASE"))
                    || symbol.startsWith(QLatin1String("_..X_CSTACK_END"))
                    || symbol.startsWith(QLatin1String("_..X_RSTACK_BASE"))
                    || symbol.startsWith(QLatin1String("_..X_RSTACK_END"))
                    || symbol.startsWith(QLatin1String("_..X_EXT_SRAM_BASE"))
                    || symbol.startsWith(QLatin1String("_..X_EXT_SRAM_SIZE"))
                    || symbol.startsWith(QLatin1String("_..X_EXT_ROM_BASE"))
                    || symbol.startsWith(QLatin1String("_..X_EXT_ROM_SIZE"))
                    || symbol.startsWith(QLatin1String("_..X_EXT_NV_BASE"))
                    || symbol.startsWith(QLatin1String("_..X_EXT_NV_SIZE"))
                    || symbol.startsWith(QLatin1String("_..X_SRAM_BASE"))
                    || symbol.startsWith(QLatin1String("_..X_SRAM_SIZE"))
                    || symbol.startsWith(QLatin1String("_..X_RSTACK_BASE"))
                    || symbol.startsWith(QLatin1String("_..X_RSTACK_SIZE"))
                    ) {
                continue;
            }
            defineSymbols.push_back(symbol);
        }
    }

    QVariantList defineSymbols;
};

// Diagnostics page options.

struct DiagnosticsPageOptions final
{
    explicit DiagnosticsPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QString warningLevel = gen::utils::cppStringModuleProperty(
                    qbsProps, QStringLiteral("warningLevel"));
        suppressAllWarnings = (warningLevel == QLatin1String("none"));
    }

    int suppressAllWarnings = 0;
};

} // namespace

// AvrLinkerSettingsGroup

AvrLinkerSettingsGroup::AvrLinkerSettingsGroup(
        const Project &qbsProject,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps)
{
    setName(QByteArrayLiteral("XLINK"));
    setArchiveVersion(kLinkerArchiveVersion);
    setDataVersion(kLinkerDataVersion);
    setDataDebugInfo(gen::utils::debugInformation(qbsProduct));

    const QString buildRootDirectory = gen::utils::buildRootPath(qbsProject);

    buildConfigPage(buildRootDirectory, qbsProduct, qbsProductDeps);
    buildOutputPage(qbsProduct);
    buildListPage(qbsProduct);
    buildDefinePage(qbsProduct);
    buildDiagnosticsPage(qbsProduct);

    // Should be called as latest stage!
    buildExtraOptionsPage(qbsProduct);
}

void AvrLinkerSettingsGroup::buildConfigPage(
        const QString &baseDirectory,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps)
{
    ConfigPageOptions opts(baseDirectory, qbsProduct, qbsProductDeps);

    if (opts.configFilePaths.count() > 0) {
        // Note: IAR IDE does not allow to specify a multiple config files,
        // although the IAR linker support it. So, we use followig 'trick':
        // we take a first config file and to add it as usual to required items;
        // and then an other remainders we forward to the "Extra options page".
        const QVariant configPath = opts.configFilePaths.takeFirst();
        // Add 'XclOverride' item (Override default).
        addOptionsGroup(QByteArrayLiteral("XclOverride"),
                        {1});
        // Add 'XclFile' item (Linke configuration file).
        addOptionsGroup(QByteArrayLiteral("XclFile"),
                        {configPath});

        // Add remainder configuration files to the "Extra options page".
        if (!opts.configFilePaths.isEmpty()) {
            for (QVariant &configPath : opts.configFilePaths)
                configPath = QLatin1String("-f ") + configPath.toString();

            m_extraOptions << opts.configFilePaths;
        }
    }

    if (opts.staticLibraries.count() > 0)
        m_extraOptions << opts.staticLibraries;

    if (!opts.entryPoint.isEmpty()) {
        // Add 'xcProgramEntryLabel' item (Entry symbol).
        addOptionsGroup(QByteArrayLiteral("xcProgramEntryLabel"),
                        {opts.entryPoint});
        // Add 'xcOverrideProgramEntryLabel' item
        // (Override default program entry).
        addOptionsGroup(QByteArrayLiteral("xcOverrideProgramEntryLabel"),
                        {1});
        // Add 'xcProgramEntryLabelSelect' item.
        addOptionsGroup(QByteArrayLiteral("xcProgramEntryLabelSelect"),
                        {0});
    }

    // Add 'XIncludes' item (Libraries search paths).
    addOptionsGroup(QByteArrayLiteral("XIncludes"),
                    opts.librarySearchPaths);
}

void AvrLinkerSettingsGroup::buildOutputPage(
        const ProductData &qbsProduct)
{
    const OutputPageOptions opts(qbsProduct);
    // Add 'XOutOverride' item (Override default output file).
    addOptionsGroup(QByteArrayLiteral("XOutOverride"),
                    {1});
    // Add 'OutputFile' item (Output file name).
    addOptionsGroup(QByteArrayLiteral("OutputFile"),
                    {opts.outputFile});
}

void AvrLinkerSettingsGroup::buildListPage(
        const ProductData &qbsProduct)
{
    const ListPageOptions opts(qbsProduct);
    // Add 'XList' item (Generate linker listing).
    addOptionsGroup(QByteArrayLiteral("XList"),
                    {opts.generateMap});
}

void AvrLinkerSettingsGroup::buildDefinePage(
        const ProductData &qbsProduct)
{
    const DefinePageOptions opts(qbsProduct);
    // Add 'XDefines' item (Defined symbols).
    addOptionsGroup(QByteArrayLiteral("XDefines"),
                    opts.defineSymbols);
}

void AvrLinkerSettingsGroup::buildDiagnosticsPage(
        const ProductData &qbsProduct)
{
    const DiagnosticsPageOptions opts(qbsProduct);
    // Add 'SuppressAllWarn' item (Suppress all warnings).
    addOptionsGroup(QByteArrayLiteral("SuppressAllWarn"),
                    {opts.suppressAllWarnings});
}

void AvrLinkerSettingsGroup::buildExtraOptionsPage(const ProductData &qbsProduct)
{
    Q_UNUSED(qbsProduct)

    if (!m_extraOptions.isEmpty()) {
        // Add 'XExtraOptionsCheck' (Use command line options).
        addOptionsGroup(QByteArrayLiteral("XExtraOptionsCheck"),
                        {1});
        // Add 'XExtraOptions' item (Command line options).
        addOptionsGroup(QByteArrayLiteral("XExtraOptions"),
                        m_extraOptions);
    }
}

} // namespace v7
} // namespace avr
} // namespace iarew
} // namespace qbs
