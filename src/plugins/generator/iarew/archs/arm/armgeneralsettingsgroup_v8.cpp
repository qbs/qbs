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

#include "armgeneralsettingsgroup_v8.h"

#include "../../iarewutils.h"

#include <QtCore/qdir.h>

namespace qbs {
namespace iarew {
namespace arm {
namespace v8 {

constexpr int kGeneralArchiveVersion = 3;
constexpr int kGeneralDataVersion = 30;

namespace {

struct CpuCoreEntry final
{
    enum CpuCoreCode {
        Arm7tdmi = 0,
        Arm7tdmis = 1,
        Arm710t = 2,
        Arm720t = 3,
        Arm740t = 4,
        Arm7ejs = 5,
        Arm9tdmi = 6,
        Arm920t = 7,
        Arm922t = 8,
        Arm940t = 9,
        Arm9e = 10,
        Arm9es = 11,
        Arm926ejs = 12,
        Arm946es = 13,
        Arm966es = 14,
        Arm968es = 15,
        Arm10e = 16,
        Arm1020e = 17,
        Arm1022e = 18,
        Arm1026ejs = 19,
        Arm1136j = 20,
        Arm1136js = 21,
        Arm1176j = 24,
        Arm1176js = 25,
        Xscale = 32,
        XscaleIr7 = 33,
        CortexM0 = 34,
        CortexM0Plus = 35,
        CortexM1 = 36,
        CortexMs1 = 37,
        CortexM3 = 38,
        CortexM4 = 39,
        CortexM7 = 41,
        CortexR4 = 42,
        CortexR5 = 44,
        CortexR7 = 46,
        CortexR8 = 48,
        CortexR52 = 49,
        CortexA5 = 50,
        CortexA7 = 52,
        CortexA8 = 53,
        CortexA9 = 54,
        CortexA15 = 55,
        CortexA17 = 56,
        CortexM23 = 58,
        CortexM33 = 59
    };

    // Required to compile in MSVC2015.
    CpuCoreEntry(CpuCoreCode cc, QByteArray tf)
        : coreCode(cc), targetFlag(std::move(tf))
    {}

    CpuCoreCode coreCode = Arm7tdmi;
    QByteArray targetFlag;
};

// Dictionary of known ARM CPU cores and its compiler options.
const CpuCoreEntry cpusDict[] = {
    {CpuCoreEntry::Arm7tdmi, "arm7tdmi"}, // same as 'sc100'
    {CpuCoreEntry::Arm7tdmis, "arm7tdmi-s"},
    {CpuCoreEntry::Arm710t, "arm710t"},
    {CpuCoreEntry::Arm720t, "arm720t"},
    {CpuCoreEntry::Arm740t, "arm740t"},
    {CpuCoreEntry::Arm7ejs, "arm7ej-s"},
    {CpuCoreEntry::Arm9tdmi, "arm9tdmi"},
    {CpuCoreEntry::Arm920t, "arm920t"},
    {CpuCoreEntry::Arm922t, "arm922t"},
    {CpuCoreEntry::Arm940t, "arm940t"},
    {CpuCoreEntry::Arm9e, "arm9e"},
    {CpuCoreEntry::Arm9es, "arm9e-s"},
    {CpuCoreEntry::Arm926ejs, "arm926ej-s"},
    {CpuCoreEntry::Arm946es, "arm946e-s"},
    {CpuCoreEntry::Arm966es, "arm966e-s"},
    {CpuCoreEntry::Arm968es, "arm968e-s"},
    {CpuCoreEntry::Arm10e, "arm10e"},
    {CpuCoreEntry::Arm1020e, "arm1020e"},
    {CpuCoreEntry::Arm1022e, "arm1022e"},
    {CpuCoreEntry::Arm1026ejs, "arm1026ej-s"},
    {CpuCoreEntry::Arm1136j, "arm1136j"},
    {CpuCoreEntry::Arm1136js, "arm1136j-s"},
    {CpuCoreEntry::Arm1176j, "arm1176j"},
    {CpuCoreEntry::Arm1176js, "arm1176j-s"},
    {CpuCoreEntry::Xscale, "xscale"},
    {CpuCoreEntry::XscaleIr7, "xscale-ir7"},
    {CpuCoreEntry::CortexM0, "cortex-m0"},
    {CpuCoreEntry::CortexM0Plus, "cortex-m0+"}, // same as 'sc000'
    {CpuCoreEntry::CortexM1, "cortex-m1"},
    {CpuCoreEntry::CortexMs1, "cortex-ms1"},
    {CpuCoreEntry::CortexM3, "cortex-m3"}, // same as 'sc300'
    {CpuCoreEntry::CortexM4, "cortex-m4"},
    {CpuCoreEntry::CortexM7, "cortex-m7"},
    {CpuCoreEntry::CortexR4, "cortex-r4"},
    {CpuCoreEntry::CortexR5, "cortex-r5"},
    {CpuCoreEntry::CortexR7, "cortex-r7"},
    {CpuCoreEntry::CortexR8, "cortex-r8"},
    {CpuCoreEntry::CortexR52, "cortex-r52"},
    {CpuCoreEntry::CortexA5, "cortex-a5"},
    {CpuCoreEntry::CortexA7, "cortex-a7"},
    {CpuCoreEntry::CortexA8, "cortex-a8"},
    {CpuCoreEntry::CortexA9, "cortex-a9"},
    {CpuCoreEntry::CortexA15, "cortex-a15"},
    {CpuCoreEntry::CortexA17, "cortex-a17"},
    {CpuCoreEntry::CortexM23, "cortex-m23"},
    {CpuCoreEntry::CortexM33, "cortex-m33"},
};

struct FpuCoreEntry final
{
    enum FpuRegistersCount {
        NoFpuRegisters,
        Fpu16Registers,
        Fpu32Registers
    };

    enum FpuCoreCode {
        NoVfp = 0,
        Vfp2 = 2,
        Vfp3d16 = 3,
        Vfp3 = 3,
        Vfp4sp = 4,
        Vfp4d16 = 5,
        Vfp4 = 5,
        Vfp5sp = 6,
        Vfp5d16 = 7,
        Vfp9s = 8
    };

    // Required to compile in MSVC2015.
    FpuCoreEntry(FpuCoreCode cc, FpuRegistersCount rc,
                 QByteArray tf)
        : coreCode(cc), regsCount(rc), targetFlag(std::move(tf))
    {}

    FpuCoreCode coreCode = NoVfp;
    FpuRegistersCount regsCount = NoFpuRegisters;
    QByteArray targetFlag;
};

// Dictionary of known ARM FPU cores and its compiler options.
const FpuCoreEntry fpusDict[] = {
    {FpuCoreEntry::Vfp2, FpuCoreEntry::NoFpuRegisters, "vfpv2"},
    {FpuCoreEntry::Vfp3d16, FpuCoreEntry::Fpu16Registers, "vfpv3_d16"},
    {FpuCoreEntry::Vfp3, FpuCoreEntry::Fpu32Registers, "vfpv3"},
    {FpuCoreEntry::Vfp4sp, FpuCoreEntry::Fpu16Registers, "vfpv4_sp"},
    {FpuCoreEntry::Vfp4d16, FpuCoreEntry::Fpu16Registers, "vfpv4_d16"},
    {FpuCoreEntry::Vfp4, FpuCoreEntry::Fpu32Registers, "vfpv4"},
    {FpuCoreEntry::Vfp5sp, FpuCoreEntry::Fpu16Registers, "vfpv5_sp"},
    {FpuCoreEntry::Vfp5d16, FpuCoreEntry::Fpu16Registers, "vfpv5_d16"},
    {FpuCoreEntry::Vfp9s, FpuCoreEntry::NoFpuRegisters, "vfp9-s"},
};

// Target page options.

struct TargetPageOptions final
{
    enum Endianness {
        LittleEndian,
        BigEndian
    };

    explicit TargetPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("driverFlags")});
        // Detect target CPU code.
        const QString cpuValue = IarewUtils::flagValue(
                    flags, QStringLiteral("--cpu"))
                .toLower();
        const auto cpuEnd = std::cend(cpusDict);
        const auto cpuIt = std::find_if(std::cbegin(cpusDict), cpuEnd,
                                        [cpuValue](
                                        const CpuCoreEntry &entry) {
            return entry.targetFlag == cpuValue.toLatin1();
        });
        if (cpuIt != cpuEnd)
            targetCpu = cpuIt->coreCode;
        // Detect target FPU code.
        const QString fpuValue = IarewUtils::flagValue(
                    flags, QStringLiteral("--fpu"))
                .toLower();
        const auto fpuEnd = std::cend(fpusDict);
        const auto fpuIt = std::find_if(std::cbegin(fpusDict), fpuEnd,
                                        [fpuValue](
                                        const FpuCoreEntry &entry) {
            return entry.targetFlag == fpuValue.toLatin1();
        });
        if (fpuIt != fpuEnd) {
            targetFpu = fpuIt->coreCode;
            targetFpuRegs = fpuIt->regsCount;
        }
        // Detect endian.
        const QString prop = gen::utils::cppStringModuleProperty(
                    qbsProps, QStringLiteral("endianness"));
        if (prop == QLatin1String("big"))
            endianness = TargetPageOptions::BigEndian;
        else if (prop == QLatin1String("little"))
            endianness = TargetPageOptions::LittleEndian;
    }

    CpuCoreEntry::CpuCoreCode targetCpu = CpuCoreEntry::Arm7tdmi;
    FpuCoreEntry::FpuCoreCode targetFpu = FpuCoreEntry::NoVfp;
    FpuCoreEntry::FpuRegistersCount targetFpuRegs = FpuCoreEntry::NoFpuRegisters;
    Endianness endianness = LittleEndian;
};

// Library 1 page options.

struct LibraryOnePageOptions final
{
    enum PrintfFormatter {
        PrintfAutoFormatter,
        PrintfFullFormatter,
        PrintfLargeFormatter,
        PrintfSmallFormatter,
        PrintfTinyFormatter
    };

    enum ScanfFormatter {
        ScanfAutoFormatter,
        ScanfFullFormatter,
        ScanfLargeFormatter,
        ScanfSmallFormatter
    };

    explicit LibraryOnePageOptions(const ProductData &qbsProduct)
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
                if (prop == QLatin1String("_printffullnomb"))
                    printfFormatter = LibraryOnePageOptions::PrintfFullFormatter;
                else if (prop == QLatin1String("_printflargenomb"))
                    printfFormatter = LibraryOnePageOptions::PrintfLargeFormatter;
                else if (prop == QLatin1String("_printfsmallnomb"))
                    printfFormatter = LibraryOnePageOptions::PrintfSmallFormatter;
                else if (prop == QLatin1String("_printftiny"))
                    printfFormatter = LibraryOnePageOptions::PrintfTinyFormatter;
            } else if (flagIt->startsWith(QLatin1String("_scanf="),
                                          Qt::CaseInsensitive)) {
                const QString prop = flagIt->split(
                            QLatin1Char('=')).at(1).toLower();;
                if (prop == QLatin1String("_scanffullnomb"))
                    scanfFormatter = LibraryOnePageOptions::ScanfFullFormatter;
                else if (prop == QLatin1String("_scanflargenomb"))
                    scanfFormatter = LibraryOnePageOptions::ScanfLargeFormatter;
                else if (prop == QLatin1String("_scanfsmallnomb"))
                    scanfFormatter = LibraryOnePageOptions::ScanfSmallFormatter;
            }
        }
    }

    PrintfFormatter printfFormatter = PrintfAutoFormatter;
    ScanfFormatter scanfFormatter = ScanfAutoFormatter;
};

// Library 2 page options.

struct LibraryTwoPageOptions final
{
    enum HeapType {
        AutomaticHeap,
        AdvancedHeap,
        BasicHeap,
        NoFreeHeap
    };

    explicit LibraryTwoPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);
        if (flags.contains(QLatin1String("--advanced_heap")))
            heapType = LibraryTwoPageOptions::AdvancedHeap;
        else if (flags.contains(QLatin1String("--basic_heap")))
            heapType = LibraryTwoPageOptions::BasicHeap;
        else if (flags.contains(QLatin1String("--no_free_heap")))
            heapType = LibraryTwoPageOptions::NoFreeHeap;
        else
            heapType = LibraryTwoPageOptions::AutomaticHeap;
    }

    HeapType heapType = AutomaticHeap;
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

    enum ThreadSupport {
        NoThread,
        EnableThread
    };

    enum LowLevelInterface {
        NoInterface,
        SemihostedInterface
    };

    explicit LibraryConfigPageOptions(const QString &baseDirectory,
                                      const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleCompilerFlags(qbsProps);
        const QFileInfo dlibFileInfo(IarewUtils::flagValue(
                                         flags, QStringLiteral("--dlib_config")));
        if (!dlibFileInfo.exists()) {
            dlibType = LibraryConfigPageOptions::NoLibrary;
        } else {
            const QString toolkitPath = IarewUtils::toolkitRootPath(qbsProduct);
            const QString dlibFilePath = dlibFileInfo.absoluteFilePath();
            if (dlibFilePath.startsWith(toolkitPath, Qt::CaseInsensitive)) {
                if (dlibFilePath.endsWith(QLatin1String("dlib_config_normal.h"),
                                          Qt::CaseInsensitive)) {
                    dlibType = LibraryConfigPageOptions::NormalLibrary;
                } else if (dlibFilePath.endsWith(
                               QLatin1String("dlib_config_full.h"),
                               Qt::CaseInsensitive)) {
                    dlibType = LibraryConfigPageOptions::FullLibrary;
                } else  {
                    dlibType = LibraryConfigPageOptions::CustomLibrary;
                }

                dlibConfigPath = IarewUtils::toolkitRelativeFilePath(
                            toolkitPath, dlibFilePath);
            } else {
                dlibType = LibraryConfigPageOptions::CustomLibrary;
                dlibConfigPath = IarewUtils::projectRelativeFilePath(
                            baseDirectory, dlibFilePath);
            }
        }

        threadSupport = flags.contains(QLatin1String("--threaded_lib"))
                ? LibraryConfigPageOptions::EnableThread
                : LibraryConfigPageOptions::NoThread;
        lowLevelInterface = flags.contains(QLatin1String("--semihosting"))
                ? LibraryConfigPageOptions::SemihostedInterface
                : LibraryConfigPageOptions::NoInterface;
    }

    RuntimeLibrary dlibType = NoLibrary;
    QString dlibConfigPath;
    ThreadSupport threadSupport = NoThread;
    LowLevelInterface lowLevelInterface = NoInterface;
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

// ArmGeneralSettingsGroup

ArmGeneralSettingsGroup::ArmGeneralSettingsGroup(
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
    buildLibraryOptionsOnePage(qbsProduct);
    buildLibraryOptionsTwoPage(qbsProduct);
    buildLibraryConfigPage(buildRootDirectory, qbsProduct);
    buildOutputPage(buildRootDirectory, qbsProduct);
}

void ArmGeneralSettingsGroup::buildTargetPage(
        const ProductData &qbsProduct)
{
    const TargetPageOptions opts(qbsProduct);
    // Add 'GBECoreSlave', 'CoreVariant', 'GFPUCoreSlave2' items
    // (Processor variant chooser).
    addOptionsGroup(QByteArrayLiteral("GBECoreSlave"),
                    {opts.targetCpu}, 26);
    addOptionsGroup(QByteArrayLiteral("CoreVariant"),
                    {opts.targetCpu}, 26);
    addOptionsGroup(QByteArrayLiteral("GFPUCoreSlave2"),
                    {opts.targetCpu}, 26);
    // Add 'FPU2', 'NrRegs' item (Floating point settings chooser).
    addOptionsGroup(QByteArrayLiteral("FPU2"),
                    {opts.targetFpu}, 0);
    addOptionsGroup(QByteArrayLiteral("NrRegs"),
                    {opts.targetFpuRegs}, 0);
    // Add 'GEndianMode' item (Endian mode chooser).
    addOptionsGroup(QByteArrayLiteral("GEndianMode"),
                    {opts.endianness});
}

void ArmGeneralSettingsGroup::buildLibraryOptionsOnePage(
        const ProductData &qbsProduct)
{
    const LibraryOnePageOptions opts(qbsProduct);
    // Add 'OGPrintfVariant' item (Printf formatter).
    addOptionsGroup(QByteArrayLiteral("OGPrintfVariant"),
                    {opts.printfFormatter});
    // Add 'OGScanfVariant' item (Printf formatter).
    addOptionsGroup(QByteArrayLiteral("OGScanfVariant"),
                    {opts.scanfFormatter});
}

void ArmGeneralSettingsGroup::buildLibraryOptionsTwoPage(
        const ProductData &qbsProduct)
{
    const LibraryTwoPageOptions opts(qbsProduct);
    // Add 'OgLibHeap' item (Heap selection:
    // auto/advanced/basic/nofree).
    addOptionsGroup(QByteArrayLiteral("OgLibHeap"),
                    {opts.heapType});
}

void ArmGeneralSettingsGroup::buildLibraryConfigPage(
        const QString &baseDirectory,
        const ProductData &qbsProduct)
{
    const LibraryConfigPageOptions opts(baseDirectory, qbsProduct);
    // Add 'GRuntimeLibSelect', 'GRuntimeLibSelectSlave'
    // and 'RTConfigPath2' items
    // (Link with runtime: none/normal/full/custom).
    addOptionsGroup(QByteArrayLiteral("GRuntimeLibSelect"),
                    {opts.dlibType});
    addOptionsGroup(QByteArrayLiteral("GRuntimeLibSelectSlave"),
                    {opts.dlibType});
    addOptionsGroup(QByteArrayLiteral("RTConfigPath2"),
                    {opts.dlibConfigPath});
    // Add 'GRuntimeLibThreads'item
    // (Enable thread support in library).
    addOptionsGroup(QByteArrayLiteral("GRuntimeLibThreads"),
                    {opts.threadSupport});
    // Add 'GenLowLevelInterface' item (Library low-level
    // interface: none/semihosted/breakpoint).
    addOptionsGroup(QByteArrayLiteral("GenLowLevelInterface"),
                    {opts.lowLevelInterface});
}

void ArmGeneralSettingsGroup::buildOutputPage(
        const QString &baseDirectory,
        const ProductData &qbsProduct)
{
    const OutputPageOptions opts(baseDirectory, qbsProduct);
    // Add 'GOutputBinary' item
    // (Output file: executable/library).
    addOptionsGroup(QByteArrayLiteral("GOutputBinary"),
                    {opts.binaryType});
    // Add 'ExePath' item
    // (Executable/binaries output directory).
    addOptionsGroup(QByteArrayLiteral("ExePath"),
                    {opts.binaryDirectory});
    // Add 'ObjPath' item
    // (Object files output directory).
    addOptionsGroup(QByteArrayLiteral("ObjPath"),
                    {opts.objectDirectory});
    // Add 'ListPath' item
    // (List files output directory).
    addOptionsGroup(QByteArrayLiteral("ListPath"),
                    {opts.listingDirectory});
}

} // namespace v8
} // namespace arm
} // namespace iarew
} // namespace qbs
