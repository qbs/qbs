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

#include "armlinkersettingsgroup_v8.h"

#include "../../iarewutils.h"

#include <QtCore/qdir.h>

namespace qbs {
namespace iarew {
namespace arm {
namespace v8 {

constexpr int kLinkerArchiveVersion = 0;
constexpr int kLinkerDataVersion = 20;

namespace {

// Config page options.

struct ConfigPageOptions final
{
    explicit ConfigPageOptions(const QString &baseDirectory,
                               const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        // Accumulate config definitions (if exists).
        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);
        configDefines = IarewUtils::flagValues(
                    flags, QStringLiteral("--config_def"));
        const QString toolkitPath = IarewUtils::toolkitRootPath(qbsProduct);

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
        // (which are set trough '--config' option).
        const QVariantList configPathValues = IarewUtils::flagValues(
                    flags, QStringLiteral("--config"));
        for (const auto &configPathValue : configPathValues) {
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
    }

    QVariantList configFilePaths;
    QVariantList configDefines;
};

// Library page options.

struct LibraryPageOptions final
{
    explicit LibraryPageOptions(const QString &baseDirectory,
                                const ProductData &qbsProduct,
                                const std::vector<ProductData> &qbsProductDeps)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();

        entryPoint = gen::utils::cppStringModuleProperty(
                    qbsProps, QStringLiteral("entryPoint"));

        // Add libraries search paths.
        const QString toolkitPath = IarewUtils::toolkitRootPath(qbsProduct);
        const QStringList libraryPaths = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("libraryPaths")});
        for (const QString &libraryPath : libraryPaths) {
            const QFileInfo libraryPathInfo(libraryPath);
            const QString fullLibrarySearchPath =
                    libraryPathInfo.absoluteFilePath();
            if (fullLibrarySearchPath.startsWith(
                        toolkitPath, Qt::CaseInsensitive)) {
                const QString path = IarewUtils::toolkitRelativeFilePath(
                            toolkitPath, fullLibrarySearchPath);
                librarySearchPaths.push_back(path);
            } else {
                const QString path = IarewUtils::projectRelativeFilePath(
                            baseDirectory,fullLibrarySearchPath);
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
                if (fullStaticLibraryPath.startsWith(
                            toolkitPath, Qt::CaseInsensitive)) {
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

        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);
        enableRuntimeLibsSearch = !flags.contains(
                    QLatin1String("--no_library_search"));
    }

    QString entryPoint;
    QVariantList staticLibraries;
    QVariantList librarySearchPaths;
    int enableRuntimeLibsSearch = 0;
};

// Output page options.

struct OutputPageOptions final
{
    explicit OutputPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);
        debugInfo = !flags.contains(QLatin1String("--strip"));
        outputFile = gen::utils::targetBinary(qbsProduct);
    }

    int debugInfo = 0;
    QString outputFile;
};

// Input page options.

struct InputPageOptions final
{
    explicit InputPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);
        keepSymbols = IarewUtils::flagValues(flags, QStringLiteral("--keep"));
    }

    QVariantList keepSymbols;
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

    ListingAction generateMap = GenerateListing;
};

// Optimizations page options.

struct OptimizationsPageOptions final
{
    explicit OptimizationsPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);
        inlineSmallRoutines = flags.contains(QLatin1String("--inline"));
        mergeDuplicateSections = flags.contains(
                    QLatin1String("--merge_duplicate_sections"));
        virtualFuncElimination = flags.contains(QLatin1String("--vfe"));
    }

    int inlineSmallRoutines = 0;
    int mergeDuplicateSections = 0;
    int virtualFuncElimination = 0;
};

// Advanced page options.

struct AdvancedPageOptions final
{
    explicit AdvancedPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);
        allowExceptions = !flags.contains(QLatin1String("--no_exceptions"));
    }

    int allowExceptions = 0;
};

// Defines page options.

struct DefinesPageOptions final
{
    explicit DefinesPageOptions(const ProductData &qbsProduct)
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
        treatWarningsAsErrors = gen::utils::cppIntegerModuleProperty(
                    qbsProps, QStringLiteral("treatWarningsAsErrors"));
    }

    int treatWarningsAsErrors = 0;
};

} // namespace

// ArmLinkerSettingsGroup

ArmLinkerSettingsGroup::ArmLinkerSettingsGroup(
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
    buildOutputPage(qbsProduct);
    buildInputPage(qbsProduct);
    buildListPage(qbsProduct);
    buildOptimizationsPage(qbsProduct);
    buildAdvancedPage(qbsProduct);
    buildDefinesPage(qbsProduct);

    // Should be called as latest stage!
    buildExtraOptionsPage(qbsProduct);
}

void ArmLinkerSettingsGroup::buildConfigPage(
        const QString &baseDirectory,
        const ProductData &qbsProduct)
{
    ConfigPageOptions opts(baseDirectory, qbsProduct);
    // Add 'IlinkConfigDefines' item
    // (Configuration file symbol definitions).
    addOptionsGroup(QByteArrayLiteral("IlinkConfigDefines"),
                    opts.configDefines);

    if (opts.configFilePaths.count() > 0) {
        // Note: IAR IDE does not allow to specify a multiple config files,
        // although the IAR linker support it. So, we use followig 'trick':
        // we take a first config file and to add it as usual to required items;
        // and then an other remainders we forward to the "Extra options page".
        const QVariant configPath = opts.configFilePaths.takeFirst();
        // Add 'IlinkIcfOverride' item (Override default).
        addOptionsGroup(QByteArrayLiteral("IlinkIcfOverride"),
                        {1});
        // Add 'IlinkIcfFile' item (Linker configuration file).
        addOptionsGroup(QByteArrayLiteral("IlinkIcfFile"),
                        {configPath});

        // Add remainder configuration files to the "Extra options page".
        if (!opts.configFilePaths.isEmpty()) {
            for (QVariant &configPath : opts.configFilePaths)
                configPath = QLatin1String("--config ")
                        + configPath.toString();

            m_extraOptions << opts.configFilePaths;
        }
    }
}

void ArmLinkerSettingsGroup::buildLibraryPage(
        const QString &baseDirectory,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps)
{
    LibraryPageOptions opts(baseDirectory, qbsProduct, qbsProductDeps);
    // Add 'IlinkOverrideProgramEntryLabel' item
    // (Override default program entry).
    addOptionsGroup(QByteArrayLiteral("IlinkOverrideProgramEntryLabel"),
                    {1});
    const int select = opts.entryPoint.isEmpty() ? 1 : 0;
    addOptionsGroup(QByteArrayLiteral("IlinkProgramEntryLabelSelect"),
                    {select});
    // Add 'IlinkProgramEntryLabel' item (Entry point name).
    addOptionsGroup(QByteArrayLiteral("IlinkProgramEntryLabel"),
                    {opts.entryPoint});

    if (!opts.staticLibraries.isEmpty()) {
        // Add 'IlinkAdditionalLibs' item (Additional libraries).
        addOptionsGroup(QByteArrayLiteral("IlinkAdditionalLibs"),
                        opts.staticLibraries);
    }

    // Add 'IlinkAutoLibEnable' item
    // (Automatic runtime library selection).
    addOptionsGroup(QByteArrayLiteral("IlinkAutoLibEnable"),
                    {opts.enableRuntimeLibsSearch});

    // Add library searh directories to the
    // "Extra options page", because IAR IDE
    // has not other options to add this paths.
    for (QVariant &libraryPath : opts.librarySearchPaths)
        libraryPath = QLatin1String("-L ") + libraryPath.toString();

    m_extraOptions << opts.librarySearchPaths;
}

void ArmLinkerSettingsGroup::buildOutputPage(
        const ProductData &qbsProduct)
{
    const OutputPageOptions opts(qbsProduct);
    // Add 'IlinkDebugInfoEnable' item
    // (Include debug information in output).
    addOptionsGroup(QByteArrayLiteral("IlinkDebugInfoEnable"),
                    {opts.debugInfo});
    // Add 'IlinkOutputFile' item (Output filename).
    addOptionsGroup(QByteArrayLiteral("IlinkOutputFile"),
                    {opts.outputFile});
}

void ArmLinkerSettingsGroup::buildInputPage(
        const ProductData &qbsProduct)
{
    const InputPageOptions opts(qbsProduct);
    // Add 'IlinkKeepSymbols' item ().
    addOptionsGroup(QByteArrayLiteral("IlinkKeepSymbols"),
                    opts.keepSymbols);
}

void ArmLinkerSettingsGroup::buildListPage(
        const ProductData &qbsProduct)
{
    const ListPageOptions opts(qbsProduct);
    // Add 'IlinkMapFile' item (Generate linker map file).
    addOptionsGroup(QByteArrayLiteral("IlinkMapFile"),
                    {opts.generateMap});
}

void ArmLinkerSettingsGroup::buildOptimizationsPage(
        const ProductData &qbsProduct)
{
    const OptimizationsPageOptions opts(qbsProduct);
    // Add 'IlinkOptInline' item (Inline small routines).
    addOptionsGroup(QByteArrayLiteral("IlinkOptInline"),
                    {opts.inlineSmallRoutines});
    // Add 'IlinkOptMergeDuplSections'item
    // (Merge duplicate sections).
    addOptionsGroup(QByteArrayLiteral("IlinkOptMergeDuplSections"),
                    {opts.mergeDuplicateSections});
    // Add 'IlinkOptUseVfe' item
    // (Perform C++ virtual functions elimination).
    addOptionsGroup(QByteArrayLiteral("IlinkOptUseVfe"),
                    {opts.virtualFuncElimination});
}

void ArmLinkerSettingsGroup::buildAdvancedPage(
        const ProductData &qbsProduct)
{
    const AdvancedPageOptions opts(qbsProduct);
    // Add 'IlinkOptExceptionsAllow' item (Allow C++ exceptions).
    addOptionsGroup(QByteArrayLiteral("IlinkOptExceptionsAllow"),
                    {opts.allowExceptions});
}

void ArmLinkerSettingsGroup::buildDefinesPage(
        const ProductData &qbsProduct)
{
    const DefinesPageOptions opts(qbsProduct);
    // Add 'IlinkDefines' item (Defined symbols).
    addOptionsGroup(QByteArrayLiteral("IlinkDefines"),
                    {opts.defineSymbols});
}

void ArmLinkerSettingsGroup::buildDiagnosticsPage(
        const ProductData &qbsProduct)
{
    const DiagnosticsPageOptions opts(qbsProduct);
    // Add 'IlinkWarningsAreErrors' item
    // (Treat all warnings as errors).
    addOptionsGroup(QByteArrayLiteral("IlinkWarningsAreErrors"),
                    {opts.treatWarningsAsErrors});
}

void ArmLinkerSettingsGroup::buildExtraOptionsPage(
        const ProductData &qbsProduct)
{
    Q_UNUSED(qbsProduct)

    // Add 'IlinkUseExtraOptions' and 'IlinkExtraOptions' items.
    addOptionsGroup(QByteArrayLiteral("IlinkUseExtraOptions"),
                    {1});
    addOptionsGroup(QByteArrayLiteral("IlinkExtraOptions"),
                    m_extraOptions);
}

} // namespace v8
} // namespace arm
} // namespace iarew
} // namespace qbs
