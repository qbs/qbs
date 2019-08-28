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

#include "stm8generalsettingsgroup_v3.h"

#include "../../iarewutils.h"

namespace qbs {
namespace iarew {
namespace stm8 {
namespace v3 {

constexpr int kGeneralArchiveVersion = 4;
constexpr int kGeneralDataVersion = 2;

namespace {

// Target page options.

struct TargetPageOptions final
{
    enum CodeModel {
        SmallCodeModel,
        MediumCodeModel,
        LargeCodeModel
    };

    enum DataModel {
        SmallDataModel,
        MediumDataModel,
        LargeDataModel
    };

    explicit TargetPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("driverFlags")});
        // Detect target code model.
        const QString codeModelValue = IarewUtils::flagValue(
                    flags, QStringLiteral("--code_model"));
        if (codeModelValue == QLatin1String("small"))
            codeModel = TargetPageOptions::SmallCodeModel;
        else if (codeModelValue == QLatin1String("medium"))
            codeModel = TargetPageOptions::MediumCodeModel;
        else if (codeModelValue == QLatin1String("large"))
            codeModel = TargetPageOptions::LargeCodeModel;
        // Detect target data model.
        const QString dataModelValue = IarewUtils::flagValue(
                    flags, QStringLiteral("--data_model"));
        if (dataModelValue == QLatin1String("small"))
            dataModel = TargetPageOptions::SmallDataModel;
        else if (dataModelValue == QLatin1String("medium"))
            dataModel = TargetPageOptions::MediumDataModel;
        else if (dataModelValue == QLatin1String("large"))
            dataModel = TargetPageOptions::LargeDataModel;
    }

    CodeModel codeModel = MediumCodeModel;
    DataModel dataModel = MediumDataModel;
};

// Output page options.

struct OutputPageOptions final
{
    explicit OutputPageOptions(const QString &baseDirectory,
                               const ProductData &qbsProduct)
    {
        binaryType = IarewUtils::outputBinaryType(qbsProduct);
        binaryDirectory = gen::utils::binaryOutputDirectory(
                    baseDirectory, qbsProduct);
        objectDirectory = gen::utils::objectsOutputDirectory(
                    baseDirectory, qbsProduct);
        listingDirectory = gen::utils::listingOutputDirectory(
                    baseDirectory, qbsProduct);
    }

    IarewUtils::OutputBinaryType binaryType = IarewUtils::ApplicationOutputType;
    QString binaryDirectory;
    QString objectDirectory;
    QString listingDirectory;
};

// Library configuration page options.

struct LibraryConfigPageOptions final
{
    enum RuntimeLibrary {
        NoLibrary,
        NormalLibrary,
        FullLibrary,
        CustomLibrary
    };

    explicit LibraryConfigPageOptions(const QString &baseDirectory,
                                      const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleCompilerFlags(qbsProps);

        const QFileInfo configInfo(IarewUtils::flagValue(
                                       flags,
                                       QStringLiteral("--dlib_config")));
        const QString configFilePath = configInfo.absoluteFilePath();

        if (!configFilePath.isEmpty()) {
            const QString libToolkitPath =
                    IarewUtils::libToolkitRootPath(qbsProduct);

            if (configFilePath.startsWith(libToolkitPath,
                                          Qt::CaseInsensitive)) {
                if (configFilePath.endsWith(QLatin1String("n.h"),
                                            Qt::CaseInsensitive)) {
                    libraryType = LibraryConfigPageOptions::NormalLibrary;
                } else if (configFilePath.endsWith(QLatin1String("f.h"),
                                                   Qt::CaseInsensitive)) {
                    libraryType = LibraryConfigPageOptions::FullLibrary;
                } else {
                    libraryType = LibraryConfigPageOptions::CustomLibrary;
                }

                configPath = IarewUtils::toolkitRelativeFilePath(
                            baseDirectory, configFilePath);
            } else {
                libraryType = LibraryConfigPageOptions::CustomLibrary;

                configPath = configFilePath;
            }
        } else {
            libraryType = LibraryConfigPageOptions::NoLibrary;
        }
    }

    RuntimeLibrary libraryType = NoLibrary;
    QString configPath;
};

// Library options page options.

struct LibraryOptionsPageOptions final
{
    enum PrintfFormatter {
        PrintfAutoFormatter = 0,
        PrintfFullFormatter = 1,
        PrintfFullNoMultibytesFormatter = 2,
        PrintfLargeFormatter = 3,
        PrintfLargeNoMultibytesFormatter = 4,
        PrintfSmallFormatter = 5,
        PrintfSmallNoMultibytesFormatter = 6,
        PrintfTinyFormatter = 7
    };

    enum ScanfFormatter {
        ScanfAutoFormatter = 0,
        ScanfFullFormatter = 1,
        ScanfFullNoMultibytesFormatter = 2,
        ScanfLargeFormatter = 3,
        ScanfLargeNoMultibytesFormatter = 4,
        ScanfSmallFormatter = 5,
        ScanfSmallNoMultibytesFormatter = 6
    };

    explicit LibraryOptionsPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);
        for (auto flagIt = flags.cbegin(); flagIt < flags.cend(); ++flagIt) {
            if (*flagIt != QLatin1String("--redirect"))
                continue;
            ++flagIt;
            if (flagIt->startsWith(QLatin1String("_printf="),
                                   Qt::CaseInsensitive)) {
                const QString prop = flagIt->split(
                            QLatin1Char('=')).at(1).toLower();
                if (prop == QLatin1String("_printffull"))
                    printfFormatter = PrintfFullFormatter;
                else if (prop == QLatin1String("_printffullnomb"))
                    printfFormatter = PrintfFullNoMultibytesFormatter;
                else if (prop == QLatin1String("_printflarge"))
                    printfFormatter = PrintfLargeFormatter;
                else if (prop == QLatin1String("_printflargenomb"))
                    printfFormatter = PrintfLargeFormatter;
                else if (prop == QLatin1String("_printfsmall"))
                    printfFormatter = PrintfSmallFormatter;
                else if (prop == QLatin1String("_printfsmallnomb"))
                    printfFormatter = PrintfSmallNoMultibytesFormatter;
                else if (prop == QLatin1String("_printftiny"))
                    printfFormatter = PrintfTinyFormatter;
            } else if (flagIt->startsWith(QLatin1String("_scanf="),
                                          Qt::CaseInsensitive)) {
                const QString prop = flagIt->split(
                            QLatin1Char('=')).at(1).toLower();
                if (prop == QLatin1String("_scanffull"))
                    scanfFormatter = ScanfFullFormatter;
                else if (prop == QLatin1String("_scanffullnomb"))
                    scanfFormatter = ScanfFullNoMultibytesFormatter;
                else if (prop == QLatin1String("_scanflarge"))
                    scanfFormatter = ScanfLargeFormatter;
                else if (prop == QLatin1String("_scanflargenomb"))
                    scanfFormatter = ScanfLargeFormatter;
                else if (prop == QLatin1String("_scanfsmall"))
                    scanfFormatter = ScanfSmallFormatter;
                else if (prop == QLatin1String("_scanfsmallnomb"))
                    scanfFormatter = ScanfSmallNoMultibytesFormatter;
            }
        }
    }

    PrintfFormatter printfFormatter = PrintfAutoFormatter;
    ScanfFormatter scanfFormatter = ScanfAutoFormatter;
};

// Stack/heap page options.

struct StackHeapPageOptions final
{
    explicit StackHeapPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);
        const auto configDefs = IarewUtils::flagValues(
                    flags, QStringLiteral("--config_def"));
        for (const auto &configDef : configDefs) {
            const auto def = configDef.toString();
            if (def.startsWith(QLatin1String("_CSTACK_SIZE="))) {
                stackSize = def.split(QLatin1Char('=')).at(1);
            } else if (def.startsWith(QLatin1String("_HEAP_SIZE="))) {
                heapSize = def.split(QLatin1Char('=')).at(1);
            }
        }
    }

    QString stackSize;
    QString heapSize;
};

} // namespace

// Stm8GeneralSettingsGroup

Stm8GeneralSettingsGroup::Stm8GeneralSettingsGroup(
        const Project &qbsProject,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps)
{
    Q_UNUSED(qbsProductDeps)

    setName(QByteArrayLiteral("General"));
    setArchiveVersion(kGeneralArchiveVersion);
    setDataVersion(kGeneralDataVersion);
    setDataDebugInfo(gen::utils::debugInformation(qbsProduct));

    const QString buildRootDirectory = gen::utils::buildRootPath(qbsProject);

    buildTargetPage(qbsProduct);
    buildOutputPage(buildRootDirectory, qbsProduct);
    buildLibraryConfigPage(buildRootDirectory, qbsProduct);
    buildLibraryOptionsPage(qbsProduct);
    buildStackHeapPage(qbsProduct);
}

void Stm8GeneralSettingsGroup::buildTargetPage(
        const ProductData &qbsProduct)
{
    const TargetPageOptions opts(qbsProduct);
    // Add 'GenCodeModel' item
    // (Code model: small/medium/large).
    addOptionsGroup(QByteArrayLiteral("GenCodeModel"),
                    {opts.codeModel});
    // Add 'GenDataModel' item
    // (Data model: small/medium/large).
    addOptionsGroup(QByteArrayLiteral("GenDataModel"),
                    {opts.dataModel});
}

void Stm8GeneralSettingsGroup::buildOutputPage(
        const QString &baseDirectory,
        const ProductData &qbsProduct)
{
    const OutputPageOptions opts(baseDirectory, qbsProduct);
    // Add 'GOutputBinary' item (Output file: executable/library).
    addOptionsGroup(QByteArrayLiteral("GOutputBinary"),
                    {opts.binaryType});
    // Add 'ExePath' item (Executable/binaries output directory).
    addOptionsGroup(QByteArrayLiteral("ExePath"),
                    {opts.binaryDirectory});
    // Add 'ObjPath' item (Object files output directory).
    addOptionsGroup(QByteArrayLiteral("ObjPath"),
                    {opts.objectDirectory});
    // Add 'ListPath' item (List files output directory).
    addOptionsGroup(QByteArrayLiteral("ListPath"),
                    {opts.listingDirectory});
}

void Stm8GeneralSettingsGroup::buildLibraryConfigPage(
        const QString &baseDirectory,
        const ProductData &qbsProduct)
{
    const LibraryConfigPageOptions opts(baseDirectory, qbsProduct);
    // Add 'GenRuntimeLibSelect' and 'GenRuntimeLibSelectSlave' items
    // (Link with runtime: none/normal/full/custom).
    addOptionsGroup(QByteArrayLiteral("GenRuntimeLibSelect"),
                    {opts.libraryType});
    addOptionsGroup(QByteArrayLiteral("GenRuntimeLibSelectSlave"),
                    {opts.libraryType});
    // Add 'GenRTConfigPath' item (Runtime configuration file).
    addOptionsGroup(QByteArrayLiteral("GenRTConfigPath"),
                    {opts.configPath});
}

void Stm8GeneralSettingsGroup::buildLibraryOptionsPage(
        const ProductData &qbsProduct)
{
    const LibraryOptionsPageOptions opts(qbsProduct);
    // Add 'GenLibOutFormatter' item (Printf formatter).
    addOptionsGroup(QByteArrayLiteral("GenLibOutFormatter"),
                    {opts.printfFormatter});
    // Add 'GenLibInFormatter' item (Scanf formatter).
    addOptionsGroup(QByteArrayLiteral("GenLibInFormatter"),
                    {opts.scanfFormatter});
}

void Stm8GeneralSettingsGroup::buildStackHeapPage(
        const ProductData &qbsProduct)
{
    const StackHeapPageOptions opts(qbsProduct);
    // Add 'GenStackSize' item (Stack size).
    addOptionsGroup(QByteArrayLiteral("GenStackSize"),
                    {opts.stackSize});
    // Add 'GenHeapSize' item (Heap size).
    addOptionsGroup(QByteArrayLiteral("GenHeapSize"),
                    {opts.heapSize});
}

} // namespace v3
} // namespace stm8
} // namespace iarew
} // namespace qbs
