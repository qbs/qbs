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

#include "msp430linkersettingsgroup_v7.h"

#include "../../iarewutils.h"

#include <QtCore/qdir.h>

namespace qbs {
namespace iarew {
namespace msp430 {
namespace v7 {

constexpr int kLinkerArchiveVersion = 4;
constexpr int kLinkerDataVersion = 30;

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

        // Library search paths.
        librarySearchPaths = gen::utils::cppVariantModuleProperties(
                    qbsProps, {QStringLiteral("libraryPaths")});

        // Entry point.
        entryPoint = gen::utils::cppStringModuleProperty(
                    qbsProps, QStringLiteral("entryPoint"));
    }

    QVariantList configFilePaths;
    QVariantList librarySearchPaths;
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
            const QString symbol = flag.mid(2);
            // Ignore system-defined macroses, because its already
            // handled in "General Options" page.
            if (symbol.startsWith(QLatin1Char('?'))
                    || symbol.startsWith(QLatin1Char('_'))
                    ) {
                continue;
            }
            defineSymbols.push_back(symbol);
        }
    }

    QVariantList defineSymbols;
};

} // namespace

//Msp430LinkerSettingsGroup

Msp430LinkerSettingsGroup::Msp430LinkerSettingsGroup(
        const Project &qbsProject,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps)
{
    Q_UNUSED(qbsProductDeps)
    setName(QByteArrayLiteral("XLINK"));
    setArchiveVersion(kLinkerArchiveVersion);
    setDataVersion(kLinkerDataVersion);
    setDataDebugInfo(gen::utils::debugInformation(qbsProduct));

    const QString buildRootDirectory = gen::utils::buildRootPath(qbsProject);

    buildConfigPage(buildRootDirectory, qbsProduct);
    buildOutputPage(qbsProduct);
    buildListPage(qbsProduct);
    buildDefinePage(qbsProduct);

    // Should be called as latest stage!
    buildExtraOptionsPage(qbsProduct);
}

void Msp430LinkerSettingsGroup::buildConfigPage(
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

    // Add 'xcOverrideProgramEntryLabel' item
    // (Override default program entry).
    addOptionsGroup(QByteArrayLiteral("xcOverrideProgramEntryLabel"),
                    {1});
    // Add 'xcProgramEntryLabel' item (Entry point name).
    addOptionsGroup(QByteArrayLiteral("xcProgramEntryLabel"),
                    {opts.entryPoint});

    // Add 'XIncludes' item (Entry point name).
    addOptionsGroup(QByteArrayLiteral("XIncludes"),
                    {opts.librarySearchPaths});
}

void Msp430LinkerSettingsGroup::buildOutputPage(
        const ProductData &qbsProduct)
{
    const OutputPageOptions opts(qbsProduct);

    // Add 'XOutOverride' item (Output file name).
    addOptionsGroup(QByteArrayLiteral("XOutOverride"),
                    {1});
    // Add 'OutputFile' item (Output file name).
    addOptionsGroup(QByteArrayLiteral("OutputFile"),
                    {opts.outputFile});
}

void Msp430LinkerSettingsGroup::buildListPage(
        const ProductData &qbsProduct)
{
    const ListPageOptions opts(qbsProduct);
    // Add 'XList' item (Generate linker map file).
    addOptionsGroup(QByteArrayLiteral("XList"),
                    {opts.generateMap});
}

void Msp430LinkerSettingsGroup::buildDefinePage(
        const ProductData &qbsProduct)
{
    const DefinePageOptions opts(qbsProduct);
    // Add 'XDefines' item (Defined symbols).
    addOptionsGroup(QByteArrayLiteral("XDefines"),
                    opts.defineSymbols);
}

void Msp430LinkerSettingsGroup::buildExtraOptionsPage(
        const ProductData &qbsProduct)
{
    Q_UNUSED(qbsProduct)

    if (m_extraOptions.isEmpty())
        return;

    // Add 'XExtraOptionsCheck' (Use command line options).
    addOptionsGroup(QByteArrayLiteral("XExtraOptionsCheck"),
                    {1});
    // Add 'XExtraOptions' item (Command line options).
    addOptionsGroup(QByteArrayLiteral("XExtraOptions"),
                    m_extraOptions);
}

} // namespace v7
} // namespace msp430
} // namespace iarew
} // namespace qbs
