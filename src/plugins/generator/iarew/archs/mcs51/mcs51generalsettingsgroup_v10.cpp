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

#include "mcs51generalsettingsgroup_v10.h"

#include "../../iarewutils.h"

namespace qbs {
namespace iarew {
namespace mcs51 {
namespace v10 {

constexpr int kGeneralArchiveVersion = 4;
constexpr int kGeneralDataVersion = 9;

namespace {

// Target page options.

struct TargetPageOptions final
{
    enum CpuCore {
        CorePlain = 1,
        CoreExtended1,
        CoreExtended2
    };

    enum CodeModel {
        CodeModelNear = 1,
        CodeModelBanked,
        CodeModelFar,
        CodeModelBankedExtended2
    };

    enum DataModel {
        DataModelTiny = 0,
        DataModelSmall,
        DataModelLarge,
        DataModelGeneric,
        DataModelFarGeneric,
        DataModelFar
    };

    enum ConstantsMemoryPlacement {
        RamMemoryPlace = 0,
        RomMemoryPlace,
        CodeMemoryPlace
    };

    enum CallingConvention {
        DataOverlayConvention = 0,
        IDataOverlayConvention,
        IDataReentrantConvention,
        PDataReentrantConvention,
        XDataReentrantConvention,
        ExtendedStackReentrantConvention
    };

    explicit TargetPageOptions(const ProductData &qbsProduct)
    {
        chipInfoPath = detectChipInfoPath(qbsProduct);

        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleCompilerFlags(qbsProps);

        // Should be parsed before the 'code_model' and
        // 'data_model' options, as that options are depends on it.
        const QString core = IarewUtils::flagValue(
                    flags, QStringLiteral("--core"))
                .toLower();
        if (core == QLatin1String("plain")) {
            cpuCore = TargetPageOptions::CorePlain;
        } else if (core == QLatin1String("extended1")) {
            cpuCore = TargetPageOptions::CoreExtended1;
        } else if (core == QLatin1String("extended2")) {
            cpuCore = TargetPageOptions::CoreExtended2;
        } else {
            // If the core variant is not set, then choose the
            // default values (see the compiler datasheet for
            // '--core' option).
            cpuCore = TargetPageOptions::CorePlain;
        }

        const QString cm = IarewUtils::flagValue(
                    flags, QStringLiteral("--code_model"))
                .toLower();
        if (cm == QLatin1String("near")) {
            codeModel = TargetPageOptions::CodeModelNear;
        } else if (cm == QLatin1String("banked")) {
            codeModel = TargetPageOptions::CodeModelBanked;
        } else if (cm == QLatin1String("far")) {
            codeModel = TargetPageOptions::CodeModelFar;
        } else if (cm == QLatin1String("banked_ext2")) {
            codeModel = TargetPageOptions::CodeModelBankedExtended2;
        } else {
            // If the code model is not set, then choose the
            // default values (see the compiler datasheet for
            // '--code_model' option).
            if (cpuCore == TargetPageOptions::CorePlain)
                codeModel = TargetPageOptions::CodeModelNear;
            else if (cpuCore == TargetPageOptions::CoreExtended1)
                codeModel = TargetPageOptions::CodeModelFar;
            else if (cpuCore == TargetPageOptions::CoreExtended2)
                codeModel = TargetPageOptions::CodeModelBankedExtended2;
        }

        const QString dm = IarewUtils::flagValue(
                    flags, QStringLiteral("--data_model")).toLower();
        if (dm == QLatin1String("tiny")) {
            dataModel = TargetPageOptions::DataModelTiny;
        } else if (dm == QLatin1String("small")) {
            dataModel = TargetPageOptions::DataModelSmall;
        } else if (dm == QLatin1String("large")) {
            dataModel = TargetPageOptions::DataModelLarge;
        } else if (dm == QLatin1String("generic")) {
            dataModel = TargetPageOptions::DataModelGeneric;
        } else if (dm == QLatin1String("far_generic")) {
            dataModel = TargetPageOptions::DataModelFarGeneric;
        } else if (dm == QLatin1String("far")) {
            dataModel = TargetPageOptions::DataModelFar;
        } else {
            // If the data model is not set, then choose the
            // default values (see the compiler datasheet for
            // '--data_model' option).
            if (cpuCore == TargetPageOptions::CorePlain)
                dataModel = TargetPageOptions::DataModelSmall;
            else if (cpuCore == TargetPageOptions::CoreExtended1)
                dataModel = TargetPageOptions::DataModelFar;
            else if (cpuCore == TargetPageOptions::CoreExtended2)
                dataModel = TargetPageOptions::DataModelLarge;
        }

        useExtendedStack = flags.contains(QLatin1String("--extended_stack"));

        const int regsCount = IarewUtils::flagValue(
                    flags, QStringLiteral("--nr_virtual_regs"))
                .toInt();
        enum { MinVRegsCount = 8, MaxVRegsCount = 32, VRegsOffset = 8 };
        // The registers index starts with 0: 0 - means 8 registers,
        // 1 - means 9 registers and etc. Any invalid values we interpret
        // as a default value in 8 registers.
        virtualRegisters = (regsCount < MinVRegsCount || regsCount > MaxVRegsCount)
                ? 0 : (regsCount - VRegsOffset);

        const QString constPlace = IarewUtils::flagValue(
                    flags, QStringLiteral("--place_constants"))
                .toLower();
        if (constPlace == QLatin1String("data")) {
            constPlacement = TargetPageOptions::RamMemoryPlace;
        } else if (constPlace == QLatin1String("data_rom")) {
            constPlacement = TargetPageOptions::RomMemoryPlace;
        } else if (constPlace == QLatin1String("code")) {
            constPlacement = TargetPageOptions::CodeMemoryPlace;
        } else {
            // If this option is not set, then choose the
            // default value (see the compiler datasheet for
            // '--place_constants' option).
            constPlacement = TargetPageOptions::RamMemoryPlace;
        }

        const QString cc = IarewUtils::flagValue(
                    flags, QStringLiteral("--calling_convention")).toLower();
        if (cc == QLatin1String("data_overlay")) {
            callingConvention = TargetPageOptions::DataOverlayConvention;
        } else if (cc == QLatin1String("idata_overlay")) {
            callingConvention = TargetPageOptions::IDataOverlayConvention;
        } else if (cc == QLatin1String("idata_reentrant")) {
            callingConvention = TargetPageOptions::IDataReentrantConvention;
        } else if (cc == QLatin1String("pdata_reentrant")) {
            callingConvention = TargetPageOptions::PDataReentrantConvention;
        } else if (cc == QLatin1String("xdata_reentrant")) {
            callingConvention = TargetPageOptions::XDataReentrantConvention;
        } else if (cc == QLatin1String("ext_stack_reentrant")) {
            callingConvention = TargetPageOptions::ExtendedStackReentrantConvention;
        } else {
            // If this option is not set, then choose the
            // default value (see the compiler datasheet for
            // '--calling_convention' option).
            callingConvention = TargetPageOptions::IDataReentrantConvention;
        }
    }

    // Trying to indirectly detect and build the chip config path,
    // which uses to show a device name in "Device information" group box.
    static QString detectChipInfoPath(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();

        QVariantList configPaths;

        // Enumerate all product linker config files
        // (which are set trough '-f' option).
        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);
        configPaths << IarewUtils::flagValues(flags, QStringLiteral("-f"));

        // Enumerate all product linker config files
        // (which are set trough 'linkerscript' tag).
        for (const auto &qbsGroup : qbsProduct.groups()) {
            const auto qbsArtifacts = qbsGroup.sourceArtifacts();
            for (const auto &qbsArtifact : qbsArtifacts) {
                const auto qbsTags = qbsArtifact.fileTags();
                if (!qbsTags.contains(QLatin1String("linkerscript")))
                    continue;
                const auto configPath = qbsArtifact.filePath();
                // Skip duplicates.
                if (configPaths.contains(configPath))
                    continue;
                configPaths << qbsArtifact.filePath();
            }
        }

        const QString toolkitPath = IarewUtils::toolkitRootPath(qbsProduct);
        for (const QVariant &configPath : qAsConst(configPaths)) {
            const QString fullConfigPath = configPath.toString();
            // We interested only in a config paths shipped inside of a toolkit.
            if (!fullConfigPath.startsWith(toolkitPath, Qt::CaseInsensitive))
                continue;
            // Extract the chip name from the linker script name.
            const int underscoreIndex = fullConfigPath.lastIndexOf(QLatin1Char('_'));
            if (underscoreIndex == -1)
                continue;
            const int dotIndex = fullConfigPath.lastIndexOf(QLatin1Char('.'));
            if (dotIndex == -1)
                continue;
            if (dotIndex <= underscoreIndex)
                continue;
            const QString chipName = fullConfigPath.mid(
                        underscoreIndex + 1,
                        dotIndex - underscoreIndex - 1);
            // Construct full chip info path.
            const QFileInfo fullChipInfoPath(QFileInfo(fullConfigPath).absolutePath()
                                             + QLatin1Char('/') + chipName
                                             + QLatin1String(".i51"));
            if (fullChipInfoPath.exists())
                return fullChipInfoPath.absoluteFilePath();
        }

        return {};
    }

    QString chipInfoPath;
    CpuCore cpuCore = CorePlain;
    CodeModel codeModel = CodeModelNear;
    DataModel dataModel = DataModelTiny;
    int useExtendedStack = 0;
    int virtualRegisters = 0;
    ConstantsMemoryPlacement constPlacement = RamMemoryPlace;
    CallingConvention callingConvention = DataOverlayConvention;
};

// System page options.

struct StackHeapPageOptions final
{
    explicit StackHeapPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList defineSymbols = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("defines")});
        const QStringList linkerFlags = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("driverLinkerFlags")});

        idataStack = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("_IDATA_STACK_SIZE"));
        if (idataStack.isEmpty())
            idataStack = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D_IDATA_STACK_SIZE"));
        if (idataStack.isEmpty())
            idataStack = QLatin1String("0x40"); // Default IDATA stack size.
        pdataStack = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("_PDATA_STACK_SIZE"));
        if (pdataStack.isEmpty())
            pdataStack = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D_PDATA_STACK_SIZE"));
        if (pdataStack.isEmpty())
            pdataStack = QLatin1String("0x80"); // Default PDATA stack size.
        xdataStack = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("_XDATA_STACK_SIZE"));
        if (xdataStack.isEmpty())
            xdataStack = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D_XDATA_STACK_SIZE"));
        if (xdataStack.isEmpty())
            xdataStack = QLatin1String("0xEFF"); // Default XDATA stack size.
        extendedStack = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("_EXTENDED_STACK_SIZE"));
        if (extendedStack.isEmpty())
            extendedStack = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D_EXTENDED_STACK_SIZE"));
        if (extendedStack.isEmpty())
            extendedStack = QLatin1String("0x3FF"); // Default EXTENDED stack size.

        xdataHeap = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("_XDATA_HEAP_SIZE"));
        if (xdataHeap.isEmpty())
            xdataHeap = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D_XDATA_HEAP_SIZE"));
        if (xdataHeap.isEmpty())
            xdataHeap = QLatin1String("0xFF"); // Default XDATA heap size.
        farHeap = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("_FAR_HEAP_SIZE"));
        if (farHeap.isEmpty())
            farHeap = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D_FAR_HEAP_SIZE"));
        if (farHeap.isEmpty())
            farHeap = QLatin1String("0xFFF"); // Default FAR heap size.
        far22Heap = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("_FAR22_HEAP_SIZE"));
        if (far22Heap.isEmpty())
            far22Heap = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D_FAR22_HEAP_SIZE"));
        if (far22Heap.isEmpty())
            far22Heap = QLatin1String("0xFFF"); // Default FAR22 heap size.
        hugeHeap = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("_HUGE_HEAP_SIZE"));
        if (hugeHeap.isEmpty())
            hugeHeap = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D_HUGE_HEAP_SIZE"));
        if (hugeHeap.isEmpty())
            hugeHeap = QLatin1String("0xFFF"); // Default HUGE heap size.

        extStackAddress = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("?ESP"));
        if (extStackAddress.isEmpty())
            extStackAddress = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D?ESP"));
        if (extStackAddress.isEmpty())
            extStackAddress = QLatin1String("0x9B"); // Default extended stack pointer address.
        extStackMask = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("?ESP_MASK"));
        if (extStackMask.isEmpty())
            extStackMask = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D?ESP_MASK"));
        if (extStackMask.isEmpty())
            extStackMask = QLatin1String("0x03"); // Default extended stack pointer mask.
    }

    // Stack sizes.
    QString idataStack;
    QString pdataStack;
    QString xdataStack;
    QString extendedStack;
    // Heap sizes.
    QString xdataHeap;
    QString farHeap;
    QString far22Heap;
    QString hugeHeap;
    // Extended stack.
    QString extStackAddress;
    QString extStackMask;
    int extStackOffset = 0;
    int extStackStartAddress = 0;
};

// Data pointer page options.

struct DptrPageOptions final
{
    enum DptrSize {
        Dptr16,
        Dptr24
    };

    enum DptrVisibility {
        DptrShadowed,
        DptrSeparate
    };

    enum SwitchMethod {
        DptrIncludeMethod,
        DptrMaskMethod
    };

    explicit DptrPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleCompilerFlags(qbsProps);

        const QString core = IarewUtils::flagValue(
                    flags, QStringLiteral("--core"));

        const QString dptr = IarewUtils::flagValue(
                    flags, QStringLiteral("--dptr"));
        enum ValueIndex { SizeIndex, NumbersIndex,
                          VisibilityIndex, SwitchMethodIndex };
        const QStringList dptrparts = dptr.split(QLatin1Char(','));
        for (auto index = 0; index < dptrparts.count(); ++index) {
            const QString part = dptrparts.at(index).toLower();
            switch (index) {
            case SizeIndex:
                if (part == QLatin1String("16")) {
                    dptrSize = DptrPageOptions::Dptr16;
                } else if (part == QLatin1String("24")) {
                    dptrSize = DptrPageOptions::Dptr24;
                } else {
                    // If this option is not set, then choose the
                    // default value (see the compiler datasheet for
                    // '--dptr' option).
                    if (core == QLatin1String("extended1"))
                        dptrSize = DptrPageOptions::Dptr24;
                    else
                        dptrSize = DptrPageOptions::Dptr16;
                }
                break;
            case NumbersIndex: {
                const int count = part.toInt();
                if (count < 1 || count > 8) {
                    // If this option is not set, then choose the
                    // default value (see the compiler datasheet for
                    // '--dptr' option).
                    if (core == QLatin1String("extended1"))
                        dptrsCountIndex = 1; // 2 DPTR's
                    else
                        dptrsCountIndex = 0; // 1 DPTR's
                } else {
                    dptrsCountIndex = (count - 1); // DPTR's count - 1
                }
            }
                break;
            case VisibilityIndex:
                if (part == QLatin1String("shadowed"))
                    dptrVisibility = DptrPageOptions::DptrShadowed;
                else if (part == QLatin1String("separate"))
                    dptrVisibility = DptrPageOptions::DptrSeparate;
                else
                    // If this option is not set, then choose the
                    // default value (see the compiler datasheet for
                    // '--dptr' option).
                    dptrVisibility = DptrPageOptions::DptrSeparate;
                break;
            case SwitchMethodIndex:
                if (part == QLatin1String("inc")) {
                    dptrSwitchMethod = DptrPageOptions::DptrIncludeMethod;
                } else if (part.startsWith(QLatin1String("xor"))) {
                    dptrSwitchMethod = DptrPageOptions::DptrMaskMethod;
                    const int firstIndex = part.indexOf(QLatin1Char('('));
                    const int lastIndex = part.indexOf(QLatin1Char(')'));
                    dptrMask = part.mid(firstIndex + 1, part.size() - lastIndex);
                } else {
                    // If this option is not set, then choose the
                    // default value (see the compiler datasheet for
                    // '--dptr' option).
                    if (core == QLatin1String("extended1")) {
                        dptrSwitchMethod = DptrPageOptions::DptrIncludeMethod;
                    } else if (core == QLatin1String("plain")) {
                        dptrSwitchMethod = DptrPageOptions::DptrMaskMethod;
                        dptrMask = QLatin1String("0x01");
                    }
                }
                break;
            default:
                break;
            }
        }

        const QStringList defineSymbols = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("defines")});
        const QStringList linkerFlags = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("driverLinkerFlags")});

        dptrPbank = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("?PBANK"));
        if (dptrPbank.isEmpty())
            dptrPbank = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D?PBANK"));
        if (dptrPbank.isEmpty())
            dptrPbank = QLatin1String("0x93"); // Default 8-15 regs address.
        dptrPbankExt = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("?PBANK_EXT"));
        if (dptrPbankExt.isEmpty())
            dptrPbankExt = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D?PBANK_EXT"));

        dpsAddress = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("?DPS"));
        if (dpsAddress.isEmpty())
            dpsAddress = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D?DPS"));
        dpcAddress = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("?DPC"));
        if (dpcAddress.isEmpty())
            dpcAddress = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D?DPC"));

        for (auto index = 0; index < 8; ++index) {
            if (index == 0) {
                QString dpxAddress = IarewUtils::flagValue(
                            defineSymbols, QStringLiteral("?DPX"));
                if (dpxAddress.isEmpty())
                    dpxAddress = IarewUtils::flagValue(
                                linkerFlags, QStringLiteral("-D?DPX"));
                if (!dpxAddress.isEmpty())
                    dpxAddress.prepend(QLatin1String("-D?DPX="));
                if (!dptrAddresses.contains(dpxAddress))
                    dptrAddresses.push_back(dpxAddress);
            } else {
                QString dplAddress = IarewUtils::flagValue(
                            defineSymbols, QStringLiteral("?DPL%1").arg(index));
                if (dplAddress.isEmpty())
                    dplAddress = IarewUtils::flagValue(
                                linkerFlags, QStringLiteral("-D?DPL%1").arg(index));
                if (!dplAddress.isEmpty())
                    dplAddress.prepend(QStringLiteral("-D?DPL%1=").arg(index));
                if (!dptrAddresses.contains(dplAddress))
                    dptrAddresses.push_back(dplAddress);

                QString dphAddress = IarewUtils::flagValue(
                            defineSymbols, QStringLiteral("?DPH%1").arg(index));
                if (dphAddress.isEmpty())
                    dphAddress = IarewUtils::flagValue(
                                linkerFlags, QStringLiteral("-D?DPH%1").arg(index));
                if (!dphAddress.isEmpty())
                    dphAddress.prepend(QStringLiteral("-D?DPH%1=").arg(index));
                if (!dptrAddresses.contains(dphAddress))
                    dptrAddresses.push_back(dphAddress);

                QString dpxAddress = IarewUtils::flagValue(
                            defineSymbols, QStringLiteral("?DPX%1").arg(index));
                if (dpxAddress.isEmpty())
                    dpxAddress = IarewUtils::flagValue(
                                linkerFlags, QStringLiteral("-D?DPX%1").arg(index));
                if (!dpxAddress.isEmpty())
                    dpxAddress.prepend(QStringLiteral("-D?DPX%1=").arg(index));
                if (!dptrAddresses.contains(dpxAddress))
                    dptrAddresses.push_back(dpxAddress);
            }
        }
    }

    int dptrsCountIndex = 0;
    DptrSize dptrSize = Dptr16;
    DptrVisibility dptrVisibility = DptrShadowed;
    SwitchMethod dptrSwitchMethod = DptrIncludeMethod;
    QString dptrMask;
    QString dptrPbank;
    QString dptrPbankExt;
    QString dpsAddress;
    QString dpcAddress;
    QStringList dptrAddresses;
};

// Code bank page options.

struct CodeBankPageOptions final
{
    explicit CodeBankPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList defineSymbols = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("defines")});
        const QStringList linkerFlags = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("driverLinkerFlags")});

        banksCount = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("_NR_OF_BANKS"));
        if (banksCount.isEmpty())
            banksCount = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D_NR_OF_BANKS"));
        if (banksCount.isEmpty())
            banksCount = QLatin1String("0x03");
        registerAddress = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("?CBANK"));
        if (registerAddress.isEmpty())
            registerAddress = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D?CBANK"));
        if (registerAddress.isEmpty())
            registerAddress = QLatin1String("0xF0");
        registerMask = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("?CBANK_MASK"));
        if (registerMask.isEmpty())
            registerMask = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D?CBANK_MASK"));
        if (registerMask.isEmpty())
            registerMask = QLatin1String("0xFF");
        bankStart = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("_CODEBANK_START"));
        if (bankStart.isEmpty())
            bankStart = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D_CODEBANK_START"));
        if (bankStart.isEmpty())
            bankStart = QLatin1String("0x8000");
        bankEnd = IarewUtils::flagValue(
                    defineSymbols, QStringLiteral("_CODEBANK_END"));
        if (bankEnd.isEmpty())
            bankEnd = IarewUtils::flagValue(
                        linkerFlags, QStringLiteral("-D_CODEBANK_END"));
        if (bankEnd.isEmpty())
            bankEnd = QLatin1String("0xFFFF");
    }

    QString banksCount;
    QString registerAddress;
    QString registerMask;
    QString bankStart;
    QString bankEnd;
};

// Library options page options.

struct LibraryOptionsPageOptions final
{
    enum PrintfFormatter {
        PrintfAutoFormatter = 0,
        PrintfLargeFormatter = 3,
        PrintfMediumFormatter = 5,
        PrintfSmallFormatter = 6
    };

    enum ScanfFormatter {
        ScanfAutoFormatter = 0,
        ScanfLargeFormatter = 3,
        ScanfMediumFormatter = 5
    };

    explicit LibraryOptionsPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);
        for (const QString &flag : flags) {
            if (flag.endsWith(QLatin1String("_formatted_write"),
                              Qt::CaseInsensitive)) {
                const QString prop = flag.split(
                            QLatin1Char('=')).at(0).toLower();
                if (prop == QLatin1String("-e_large_write"))
                    printfFormatter = LibraryOptionsPageOptions::PrintfLargeFormatter;
                else if (prop == QLatin1String("-e_medium_write"))
                    printfFormatter = LibraryOptionsPageOptions::PrintfMediumFormatter;
                else if (prop == QLatin1String("-e_small_write"))
                    printfFormatter = LibraryOptionsPageOptions::PrintfSmallFormatter;
                else
                    // If this option is not set, then choose the
                    // default value (see the compiler datasheet for
                    // '_formatted_write' option).
                    printfFormatter = LibraryOptionsPageOptions::PrintfMediumFormatter;
            } else if (flag.endsWith(QLatin1String("_formatted_read"),
                                     Qt::CaseInsensitive)) {
                const QString prop = flag.split(QLatin1Char('='))
                        .at(0).toLower();
                if (prop == QLatin1String("-e_large_read"))
                    scanfFormatter = LibraryOptionsPageOptions::ScanfLargeFormatter;
                else if (prop == QLatin1String("-e_medium_read"))
                    scanfFormatter = LibraryOptionsPageOptions::ScanfMediumFormatter;
                else
                    // If this option is not set, then choose the
                    // default value (see the compiler datasheet for
                    // '_formatted_read' option).
                    scanfFormatter = LibraryOptionsPageOptions::ScanfMediumFormatter;
            }
        }
    }

    PrintfFormatter printfFormatter = PrintfAutoFormatter;
    ScanfFormatter scanfFormatter = ScanfAutoFormatter;
};

// Library configuration page options.

struct LibraryConfigPageOptions final
{
    enum RuntimeLibrary {
        NoLibrary,
        NormalDlibLibrary,
        CustomDlibLibrary,
        ClibLibrary,
        CustomClibLibrary
    };

    explicit LibraryConfigPageOptions(const QString &baseDirectory,
                                      const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleCompilerFlags(qbsProps);

        const QStringList libraryPaths = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("staticLibraries")});
        const auto libraryBegin = libraryPaths.cbegin();
        const auto libraryEnd = libraryPaths.cend();

        const QFileInfo dlibConfigInfo(IarewUtils::flagValue(
                                           flags, QStringLiteral("--dlib_config")));
        const QString dlibConfigFilePath = dlibConfigInfo.absoluteFilePath();
        if (!dlibConfigFilePath.isEmpty()) {
            const QString dlibToolkitPath = IarewUtils::dlibToolkitRootPath(
                        qbsProduct);
            if (dlibConfigFilePath.startsWith(dlibToolkitPath,
                                              Qt::CaseInsensitive)) {
                libraryType = LibraryConfigPageOptions::NormalDlibLibrary;
                configPath = IarewUtils::toolkitRelativeFilePath(
                            baseDirectory, dlibConfigFilePath);

                // Find dlib library inside of IAR toolkit directory.
                const auto libraryIt = std::find_if(libraryBegin, libraryEnd,
                                                    [dlibToolkitPath](
                                                    const QString &libraryPath) {
                    return libraryPath.startsWith(dlibToolkitPath);
                });
                if (libraryIt != libraryEnd) {
                    // This means that dlib library is 'standard' (placed inside
                    // of IAR toolkit directory).
                    libraryPath = IarewUtils::toolkitRelativeFilePath(
                                baseDirectory, *libraryIt);
                }
            } else {
                // This means that dlib library is 'custom'
                // (but we don't know its path).
                libraryType = LibraryConfigPageOptions::CustomDlibLibrary;
                configPath = IarewUtils::projectRelativeFilePath(
                            baseDirectory, dlibConfigFilePath);
            }
        } else {
            // Find clib library inside of IAR toolkit directory.
            const QString clibToolkitPath = IarewUtils::clibToolkitRootPath(
                        qbsProduct);
            const auto libraryIt = std::find_if(libraryBegin, libraryEnd,
                                                [clibToolkitPath](
                                                const QString &libraryPath) {
                return libraryPath.startsWith(clibToolkitPath);
            });
            if (libraryIt != libraryEnd) {
                // This means that clib library is 'standard' (placed inside
                // of IAR toolkit directory).
                libraryType = LibraryConfigPageOptions::ClibLibrary;
                libraryPath = IarewUtils::toolkitRelativeFilePath(
                            baseDirectory, *libraryIt);
            } else {
                // This means that no any libraries are used .
                libraryType = LibraryConfigPageOptions::NoLibrary;
            }
        }
    }

    RuntimeLibrary libraryType = NoLibrary;
    QString configPath;
    QString libraryPath;
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

} // namespace

// Mcs51GeneralSettingsGroup

Mcs51GeneralSettingsGroup::Mcs51GeneralSettingsGroup(
        const Project &qbsProject,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps)
{
    Q_UNUSED(qbsProject)
    Q_UNUSED(qbsProductDeps)

    setName(QByteArrayLiteral("General"));
    setArchiveVersion(kGeneralArchiveVersion);
    setDataVersion(kGeneralDataVersion);
    setDataDebugInfo(gen::utils::debugInformation(qbsProduct));

    const QString buildRootDirectory = gen::utils::buildRootPath(qbsProject);

    buildTargetPage(qbsProduct);
    buildStackHeapPage(qbsProduct);
    buildDataPointerPage(qbsProduct);
    buildCodeBankPage(qbsProduct);
    buildLibraryOptionsPage(qbsProduct);
    buildLibraryConfigPage(buildRootDirectory, qbsProduct);
    buildOutputPage(buildRootDirectory, qbsProduct);
}

void Mcs51GeneralSettingsGroup::buildTargetPage(
        const ProductData &qbsProduct)
{
    const TargetPageOptions opts(qbsProduct);
    // Add 'OGChipConfigPath' item (Device: <chip name>).
    addOptionsGroup(QByteArrayLiteral("OGChipConfigPath"),
                    {opts.chipInfoPath});

    // Add 'CPU Core' and 'CPU Core Slave' items
    // (CPU core: plain/extended{1|2}).
    addOptionsGroup(QByteArrayLiteral("CPU Core"),
                    {opts.cpuCore});
    addOptionsGroup(QByteArrayLiteral("CPU Core Slave"),
                    {opts.cpuCore});
    // Add 'Code Memory Model' and 'Code Memory Model slave' items
    // (Code model: near/banked/far/banked extended).
    addOptionsGroup(QByteArrayLiteral("Code Memory Model"),
                    {opts.codeModel});
    addOptionsGroup(QByteArrayLiteral("Code Memory Model slave"),
                    {opts.codeModel});
    // Add 'Data Memory Model' and 'Data Memory Model slave' items
    // (Data model: tiny/small/large/generic/far).
    addOptionsGroup(QByteArrayLiteral("Data Memory Model"),
                    {opts.dataModel});
    addOptionsGroup(QByteArrayLiteral("Data Memory Model slave"),
                    {opts.dataModel});
    // Add 'Use extended stack' and 'Use extended stack slave' items
    // (Use extended stack).
    addOptionsGroup(QByteArrayLiteral("Use extended stack"),
                    {opts.useExtendedStack});
    addOptionsGroup(QByteArrayLiteral("Use extended stack slave"),
                    {opts.useExtendedStack});
    // Add 'Workseg Size' item (Number of virtual registers: 8...32).
    addOptionsGroup(QByteArrayLiteral("Workseg Size"),
                    {opts.virtualRegisters});
    // Add 'Constant Placement' item
    // (Location of constants and strings: ram/rom/code memories).
    addOptionsGroup(QByteArrayLiteral("Constant Placement"),
                    {opts.constPlacement});
    // Add 'Calling convention' item (Calling convention).
    addOptionsGroup(QByteArrayLiteral("Calling convention"),
                    {opts.callingConvention});
}

void Mcs51GeneralSettingsGroup::buildStackHeapPage(
        const ProductData &qbsProduct)
{
    const StackHeapPageOptions opts(qbsProduct);
    // Add 'General Idata Stack Size' item (Stack size: IDATA).
    addOptionsGroup(QByteArrayLiteral("General Idata Stack Size"),
                    {opts.idataStack});
    // Add 'General Pdata Stack Size' item (Stack size: PDATA).
    addOptionsGroup(QByteArrayLiteral("General Pdata Stack Size"),
                    {opts.pdataStack});
    // Add 'General Xdata Stack Size' item (Stack size: XDATA).
    addOptionsGroup(QByteArrayLiteral("General Xdata Stack Size"),
                    {opts.xdataStack});
    // Add 'General Ext Stack Size' item (Stack size: Extended).
    addOptionsGroup(QByteArrayLiteral("General Ext Stack Size"),
                    {opts.extendedStack});

    // Add 'General Xdata Heap Size' item (Heap size: XDATA).
    addOptionsGroup(QByteArrayLiteral("General Xdata Heap Size"),
                    {opts.xdataHeap});
    // Add 'General Far Heap Size' item (Heap size: Far).
    addOptionsGroup(QByteArrayLiteral("General Far Heap Size"),
                    {opts.farHeap});
    // Add 'General Far22 Heap Size' item (Heap size: Far22).
    addOptionsGroup(QByteArrayLiteral("General Far22 Heap Size"),
                    {opts.far22Heap});
    // Add 'General Huge Heap Size' item (Heap size: Huge).
    addOptionsGroup(QByteArrayLiteral("General Huge Heap Size"),
                    {opts.hugeHeap});

    // Add 'Extended stack address' item
    // (Extended stack pointer address).
    addOptionsGroup(QByteArrayLiteral("Extended stack address"),
                    {opts.extStackAddress});
    // Add 'Extended stack mask' item (Extended stack pointer mask).
    addOptionsGroup(QByteArrayLiteral("Extended stack mask"),
                    {opts.extStackMask});
    // Add 'Extended stack is offset' item
    // (Extended stack pointer is an offset).
    addOptionsGroup(QByteArrayLiteral("Extended stack is offset"),
                    {opts.extStackOffset});
}

void Mcs51GeneralSettingsGroup::buildDataPointerPage(
        const ProductData &qbsProduct)
{
    const DptrPageOptions opts(qbsProduct);
    // Add 'Nr of Datapointers' item (Number of DPTRs: 1...8).
    addOptionsGroup(QByteArrayLiteral("Nr of Datapointers"),
                    {opts.dptrsCountIndex});
    // Add 'Datapointer Size' item (DPTR size: 16/24).
    addOptionsGroup(QByteArrayLiteral("Datapointer Size"),
                    {opts.dptrSize});
    // Add 'Sfr Visibility' item (DPTR address: shadowed/separate).
    addOptionsGroup(QByteArrayLiteral("Sfr Visibility"),
                    {opts.dptrVisibility});
    // Add 'Switch Method' item (Switch method: inc/mask).
    addOptionsGroup(QByteArrayLiteral("Switch Method"),
                    {opts.dptrSwitchMethod});
    // Add 'Mask Value' item (Switch method mask).
    addOptionsGroup(QByteArrayLiteral("Mask Value"),
                    {opts.dptrMask});
    // Add 'PDATA 8-15 register address' item (Page register
    // address (for bits 8-15).
    addOptionsGroup(QByteArrayLiteral("PDATA 8-15 register address"),
                    {opts.dptrPbank});
    // Add 'PDATA 16-31 register address' item (Page register
    // address (for bits 16-31).
    addOptionsGroup(QByteArrayLiteral("PDATA 16-31 register address"),
                    {opts.dptrPbankExt});
    // Add 'DPS Address' item (Selected DPTR register).
    addOptionsGroup(QByteArrayLiteral("DPS Address"),
                    {opts.dpsAddress});
    // Add 'DPC Address' item (Separate DPTR control register).
    addOptionsGroup(QByteArrayLiteral("DPC Address"),
                    {opts.dpcAddress});
    // Add 'DPTR Addresses' item (DPTR addresses: Low/High/Ext).
    const QString dptrAddresses = opts.dptrAddresses.join(QLatin1Char(' '));
    addOptionsGroup(QByteArrayLiteral("DPTR Addresses"),
                    {dptrAddresses});
}

void Mcs51GeneralSettingsGroup::buildCodeBankPage(
        const ProductData &qbsProduct)
{
    const CodeBankPageOptions opts(qbsProduct);
    // Add 'CodeBankReg' item (Register address).
    addOptionsGroup(QByteArrayLiteral("CodeBankReg"),
                    {opts.registerAddress});
    // Add 'CodeBankRegMask' item (Register mask).
    addOptionsGroup(QByteArrayLiteral("CodeBankRegMask"),
                    {opts.registerMask});
    // Add 'CodeBankNrOfs' item (Number of banks).
    addOptionsGroup(QByteArrayLiteral("CodeBankNrOfs"),
                    {opts.banksCount});
    // Add 'CodeBankStart' item (Bank start).
    addOptionsGroup(QByteArrayLiteral("CodeBankStart"),
                    {opts.bankStart});
    // Add 'CodeBankSize' item (Bank end).
    addOptionsGroup(QByteArrayLiteral("CodeBankSize"),
                    {opts.bankEnd});
}

void Mcs51GeneralSettingsGroup::buildLibraryOptionsPage(
        const ProductData &qbsProduct)
{
    const LibraryOptionsPageOptions opts(qbsProduct);
    // Add 'Output variant' item (Printf formatter).
    addOptionsGroup(QByteArrayLiteral("Output variant"),
                    {opts.printfFormatter});
    // Add 'Input variant' item (Printf formatter).
    addOptionsGroup(QByteArrayLiteral("Input variant"),
                    {opts.scanfFormatter});
}

void Mcs51GeneralSettingsGroup::buildLibraryConfigPage(
        const QString &baseDirectory,
        const ProductData &qbsProduct)
{
    const LibraryConfigPageOptions opts(baseDirectory, qbsProduct);
    // Add 'GRuntimeLibSelect2' and 'GRuntimeLibSelectSlave2' items
    // (Link with runtime: none/dlib/clib/etc).
    addOptionsGroup(QByteArrayLiteral("GRuntimeLibSelect2"),
                    {opts.libraryType});
    addOptionsGroup(QByteArrayLiteral("GRuntimeLibSelectSlave2"),
                    {opts.libraryType});
    // Add 'RTConfigPath' item (Runtime configuration file).
    addOptionsGroup(QByteArrayLiteral("RTConfigPath"),
                    {opts.configPath});
    // Add 'RTLibraryPath' item (Runtime library file).
    addOptionsGroup(QByteArrayLiteral("RTLibraryPath"),
                    {opts.libraryPath});
}

void Mcs51GeneralSettingsGroup::buildOutputPage(
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

} // namespace v10
} // namespace mcs51
} // namespace iarew
} // namespace qbs
