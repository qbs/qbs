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

#include "msp430generalsettingsgroup_v7.h"

#include "../../iarewutils.h"

namespace qbs {
namespace iarew {
namespace msp430 {
namespace v7 {

constexpr int kGeneralArchiveVersion = 21;
constexpr int kGeneralDataVersion = 34;

namespace {

// Target page options.

struct TargetPageOptions final
{
    enum CodeModel {
        SmallCodeModel,
        LargeCodeModel
    };

    enum DataModel {
        SmallDataModel,
        MediumDataModel,
        LargeDataModel
    };

    enum FloatingPointDoubleSize {
        DoubleSize32Bits,
        DoubleSize64Bits
    };

    enum HardwareMultiplierType {
        AllowDirectAccessMultiplier,
        UseOnlyLibraryCallsMultiplier
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
        // Detect floating point double size.
        const int doubleSize = IarewUtils::flagValue(
                    flags, QStringLiteral("--double")).toInt();
        if (doubleSize == 32)
            floatingPointDoubleSize = DoubleSize32Bits;
        else if (doubleSize == 64)
            floatingPointDoubleSize = DoubleSize64Bits;
        // Detect hardware multiplier.
        const QString multiplier = IarewUtils::flagValue(
                    flags, QStringLiteral("--multiplier"));
        enableHardwareMultiplier = (multiplier.compare(QLatin1String("16")) == 0
                                    || multiplier.compare(QLatin1String("16s")) == 0
                                    || multiplier.compare(QLatin1String("32")) == 0);
        // Detect code and read-only data position-independence.
        enableRopi = flags.contains(QLatin1String("--ropi"));
        // No dynamic read-write initialization.
        disableDynamicReadWriteInitialization = flags.contains(
                    QLatin1String("--no_rw_dynamic_init"));

        // Detect target device name.
        detectDeviceMenu(qbsProduct);
    }

    void detectDeviceMenu(const ProductData &qbsProduct)
    {
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
                if (!fullConfigPath.startsWith(toolkitPath, Qt::CaseInsensitive))
                    continue;

                const QFileInfo configInfo(fullConfigPath);
                const QString configBase = configInfo.baseName();
                if (!configBase.startsWith(QLatin1String("lnk")))
                    continue;

                // Remove 'lnk' prefix.
                const QString deviceName = QStringLiteral("MSP%1")
                        .arg(configBase.mid(3).toUpper());

                deviceMenu = QStringLiteral("%1\t%1").arg(deviceName);
                return;
            }
        }

        // Falling back to generic menu.
        if (deviceMenu.isEmpty()) {
            const auto &qbsProps = qbsProduct.moduleProperties();
            const QStringList flags = IarewUtils::cppModuleCompilerFlags(qbsProps);
            const QString cpuCore = IarewUtils::flagValue(flags, QStringLiteral("--core"));
            if (cpuCore.isEmpty())
                return;
            deviceMenu = QStringLiteral("MSP%1\tGeneric MSP%1 device").arg(cpuCore);
            return;
        }
    }

    CodeModel codeModel = LargeCodeModel;
    DataModel dataModel = SmallDataModel;
    FloatingPointDoubleSize floatingPointDoubleSize = DoubleSize32Bits;
    HardwareMultiplierType hardwareMultiplierType = AllowDirectAccessMultiplier;
    int enableHardwareMultiplier = 0;
    int enableRopi = 0;
    int disableDynamicReadWriteInitialization = 0;
    QString deviceMenu;
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
        NormalDLibrary,
        FullDLibrary,
        CustomDLibrary,
        CLibrary, // deprecated
        CustomCLibrary, // deprecated
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
                    libraryType = LibraryConfigPageOptions::NormalDLibrary;
                } else if (configFilePath.endsWith(QLatin1String("f.h"),
                                                   Qt::CaseInsensitive)) {
                    libraryType = LibraryConfigPageOptions::FullDLibrary;
                } else {
                    libraryType = LibraryConfigPageOptions::CustomDLibrary;
                }

                configPath = IarewUtils::toolkitRelativeFilePath(
                            baseDirectory, configFilePath);
            } else {
                libraryType = LibraryConfigPageOptions::CustomDLibrary;

                configPath = configFilePath;
            }
        }
    }

    RuntimeLibrary libraryType = NormalDLibrary;
    QString libraryPath;
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
            if (flagIt->endsWith(QLatin1String("=_printf"),
                                 Qt::CaseInsensitive)) {
                const QString prop = flagIt->split(
                            QLatin1Char('=')).at(0).toLower();
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
            } else if (flagIt->endsWith(QLatin1String("=_scanf"),
                                        Qt::CaseInsensitive)) {
                const QString prop = flagIt->split(
                            QLatin1Char('=')).at(0).toLower();
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

        // Detect stack size.
        stackSize = IarewUtils::flagValue(
                    flags, QStringLiteral("-D_STACK_SIZE"));
        if (stackSize.isEmpty())
            stackSize = QLatin1String("A0");
        // Detect data heap16 size.
        data16HeapSize = IarewUtils::flagValue(
                    flags, QStringLiteral("-D_DATA16_HEAP_SIZE"));
        if (data16HeapSize.isEmpty())
            stackSize = QLatin1String("A0");
        // Detect data heap20 size.
        data20HeapSize = IarewUtils::flagValue(
                    flags, QStringLiteral("-D_DATA20_HEAP_SIZE"));
        if (data20HeapSize.isEmpty())
            stackSize = QLatin1String("50");
    }

    QString stackSize;
    QString data16HeapSize;
    QString data20HeapSize;
};

} // namespace

//Msp430GeneralSettingsGroup

Msp430GeneralSettingsGroup::Msp430GeneralSettingsGroup(
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

void Msp430GeneralSettingsGroup::buildTargetPage(
        const ProductData &qbsProduct)
{
    const TargetPageOptions opts(qbsProduct);
    // Add 'OGChipSelectMenu' item
    // (Device: xxx).
    addOptionsGroup(QByteArrayLiteral("OGChipSelectMenu"),
                    {opts.deviceMenu});

    // Add 'RadioCodeModelType' item
    // (Code model: small/large).
    addOptionsGroup(QByteArrayLiteral("RadioCodeModelType"),
                    {opts.codeModel});
    // Add 'RadioDataModelType' item
    // (Data model: small/medium/large).
    addOptionsGroup(QByteArrayLiteral("RadioDataModelType"),
                    {opts.dataModel});
    // Add 'OGDouble' item
    // (Floating point double size: 32/64 bits).
    addOptionsGroup(QByteArrayLiteral("OGDouble"),
                    {opts.floatingPointDoubleSize});
    // Add 'Hardware Multiplier' item
    // (Hardware multiplier).
    addOptionsGroup(QByteArrayLiteral("Hardware Multiplier"),
                    {opts.enableHardwareMultiplier});
    if (opts.enableHardwareMultiplier) {
        // Add 'RadioHardwareMultiplierType' item.
        addOptionsGroup(QByteArrayLiteral("Hardware RadioHardwareMultiplierType"),
                        {opts.hardwareMultiplierType});
    }
    // Add 'Ropi' item.
    // (Position independence: Code and read-only data).
    addOptionsGroup(QByteArrayLiteral("Ropi"),
                    {opts.enableRopi});
    // Add 'NoRwDynamicInit' item.
    // (Position independence: No dynamic read/write initialization).
    addOptionsGroup(QByteArrayLiteral("NoRwDynamicInit"),
                    {opts.disableDynamicReadWriteInitialization});
}

void Msp430GeneralSettingsGroup::buildOutputPage(
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

void Msp430GeneralSettingsGroup::buildLibraryConfigPage(
        const QString &baseDirectory,
        const ProductData &qbsProduct)
{
    const LibraryConfigPageOptions opts(baseDirectory, qbsProduct);
    // Add 'GRuntimeLibSelect' and 'GRuntimeLibSelectSlave' items
    // (Link with runtime: none/normal/full/custom).
    addOptionsGroup(QByteArrayLiteral("GRuntimeLibSelect"),
                    {opts.libraryType});
    addOptionsGroup(QByteArrayLiteral("GRuntimeLibSelectSlave"),
                    {opts.libraryType});
    // Add 'RTConfigPath' item (Runtime configuration file).
    addOptionsGroup(QByteArrayLiteral("RTConfigPath"),
                    {opts.configPath});
    // Add 'RTLibraryPath' item (Runtime library file).
    addOptionsGroup(QByteArrayLiteral("RTLibraryPath"),
                    {opts.libraryPath});
}

void Msp430GeneralSettingsGroup::buildLibraryOptionsPage(
        const ProductData &qbsProduct)
{
    const LibraryOptionsPageOptions opts(qbsProduct);
    // Add 'Output variant' item (Printf formatter).
    addOptionsGroup(QByteArrayLiteral("Output variant"),
                    {opts.printfFormatter});
    // Add 'Input variant' item (Scanf formatter).
    addOptionsGroup(QByteArrayLiteral("Input variant"),
                    {opts.scanfFormatter});
}

void Msp430GeneralSettingsGroup::buildStackHeapPage(
        const ProductData &qbsProduct)
{
    const StackHeapPageOptions opts(qbsProduct);
    // Add 'GStackHeapOverride' item (Override default).
    addOptionsGroup(QByteArrayLiteral("GStackHeapOverride"),
                    {1});
    // Add 'GStackSize2' item (Stack size).
    addOptionsGroup(QByteArrayLiteral("GStackSize2"),
                    {opts.stackSize});
    // Add 'GHeapSize2' item (Heap16 size).
    addOptionsGroup(QByteArrayLiteral("GHeapSize2"),
                    {opts.data16HeapSize});
    // Add 'GHeap20Size' item (Heap16 size).
    addOptionsGroup(QByteArrayLiteral("GHeap20Size"),
                    {opts.data20HeapSize});
}

} // namespace v7
} // namespace msp430
} // namespace iarew
} // namespace qbs
