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

#include "armtargetcommonoptionsgroup_v5.h"

#include "../../keiluvutils.h"

#include <generators/generatorutils.h>

namespace qbs {
namespace keiluv {
namespace arm {
namespace v5 {

namespace {

const struct DeviceEntry {
    QByteArray cpu; // CPU option.
    std::set<QByteArray> fpus; // FPU's options.
    QByteArray device; // Project file entry.
} deviceDict[] = {
    {"8-M.Base", {}, "ARMv8MBL"},
    {"8-M.Main", {"FPv5-SP"}, "ARMv8MML_SP"},
    {"8-M.Main", {"FPv5_D16"}, "ARMv8MML_DP"},
    {"8-M.Main", {"SoftVFP"},"ARMv8MML"},
    {"8-M.Main.dsp", {"FPv5-SP"}, "ARMv8MML_DSP_SP"},
    {"8-M.Main.dsp", {"FPv5_D16"}, "ARMv8MML_DSP_DP"},
    {"8-M.Main.dsp", {"SoftVFP"}, "ARMv8MML_DSP"},
    {"Cortex-M0", {}, "ARMCM0"},
    {"Cortex-M0+", {}, "ARMCM0P"},
    {"Cortex-M0plus", {}, "ARMCM0P"},
    {"Cortex-M23", {}, "ARMCM23"}, // same as ARMCM23_TZ
    {"Cortex-M3", {}, "ARMCM3"},
    {"Cortex-M4", {}, "ARMCM4"},
    {"Cortex-M4.fp", {}, "ARMCM4_FP"},
    {"Cortex-M7", {"SoftVFP"}, "ARMCM7"},
    {"Cortex-M7.fp.dp", {}, "ARMCM7_DP"},
    {"Cortex-M7.fp.sp", {}, "ARMCM7_SP"},
    {"SC000", {}, "ARMSC000"},
    {"SC300", {}, "ARMSC300"},
    {"Cortex-M33.no_dsp", {"SoftVFP"}, "ARMCM33"}, // same as ARMCM33_TZ
    {"Cortex-M33", {"FPv5-SP", "softvfp+vfpv2"}, "ARMCM33_DSP_FP"}, // same as ARMCM33_DSP_FP_TZ
};

struct CommonPageOptions final
{
    explicit CommonPageOptions(const Project &qbsProject,
                               const ProductData &qbsProduct)
    {
        Q_UNUSED(qbsProject)

        const auto &qbsProps = qbsProduct.moduleProperties();
        const auto flags = KeiluvUtils::cppModuleCompilerFlags(qbsProps);

        // Browse information.
        // ???

        // Debug information.
        debugInfo = gen::utils::debugInformation(qbsProduct);

        // Output parameters.
        executableName = gen::utils::targetBinary(qbsProduct);
        // Fix output binary name if it is a library. Because
        // the IDE appends an additional suffix (.LIB) to end
        // of an output library name.
        if (executableName.endsWith(QLatin1String(".lib")))
            executableName = qbsProduct.targetName();

        const QString baseDirectory = gen::utils::buildRootPath(qbsProject);
        objectDirectory = QDir::toNativeSeparators(
                    gen::utils::objectsOutputDirectory(
                        baseDirectory, qbsProduct));
        listingDirectory = QDir::toNativeSeparators(
                    gen::utils::listingOutputDirectory(
                        baseDirectory, qbsProduct));

        // Target type.
        targetType = KeiluvUtils::outputBinaryType(qbsProduct);

        // Detect the device name from the command line options
        // (like --cpu and --fpu).
        const auto cpu = gen::utils::firstFlagValue(
                    flags, QStringLiteral("--cpu")).toLatin1();
        const auto fpus = gen::utils::allFlagValues(
                    flags, QStringLiteral("--fpu"));

        for (const auto &deviceEntry : deviceDict) {
            // Since Qt 5.12 we can use QByteArray::compare(..., Qt::CaseInsensitive)
            // instead.
            if (cpu.toLower() != deviceEntry.cpu.toLower())
                continue;

            size_t fpuMatches = 0;
            const auto dictFpuBegin = std::cbegin(deviceDict->fpus);
            const auto dictFpuEnd = std::cend(deviceDict->fpus);
            for (const auto &fpu : fpus) {
                const auto dictFpuIt = std::find_if(
                            dictFpuBegin, dictFpuEnd,
                            [fpu](const QByteArray &dictFpu) {
                    return fpu.compare(QString::fromLatin1(dictFpu),
                                       Qt::CaseInsensitive) == 0;
                });
                if (dictFpuIt != dictFpuEnd)
                    ++fpuMatches;
            }

            if (fpuMatches < deviceEntry.fpus.size())
                continue;

            deviceName = QString::fromLatin1(deviceEntry.device);
            cpuType = QString::fromLatin1(deviceEntry.cpu);
            break;
        }
    }

    int debugInfo = false;
    int browseInfo = false;
    QString deviceName;
    QString cpuType;
    QString executableName;
    QString objectDirectory;
    QString listingDirectory;
    KeiluvUtils::OutputBinaryType targetType =
            KeiluvUtils::ApplicationOutputType;
};

} // namespace

ArmTargetCommonOptionsGroup::ArmTargetCommonOptionsGroup(
        const qbs::Project &qbsProject,
        const qbs::ProductData &qbsProduct)
    : gen::xml::PropertyGroup("TargetCommonOption")
{
    const CommonPageOptions opts(qbsProject, qbsProduct);

    // Fill device items.
    appendProperty(QByteArrayLiteral("Device"),
                   opts.deviceName);
    appendProperty(QByteArrayLiteral("Vendor"),
                   QByteArrayLiteral("ARM"));
    appendProperty(QByteArrayLiteral("PackID"),
                   QByteArrayLiteral("ARM.CMSIS.5.6.0"));
    appendProperty(QByteArrayLiteral("PackURL"),
                   QByteArrayLiteral("http://www.keil.com/pack/"));

    const auto cpuType = QStringLiteral("CPUTYPE(\"%1\")")
            .arg(opts.cpuType);
    appendProperty(QByteArrayLiteral("Cpu"), cpuType);

    // Add 'Debug Information' item.
    appendProperty(QByteArrayLiteral("DebugInformation"),
                   opts.debugInfo);
    // Add 'Browse Information' item.
    appendProperty(QByteArrayLiteral("BrowseInformation"),
                   opts.browseInfo);

    // Add 'Name of Executable'.
    appendProperty(QByteArrayLiteral("OutputName"),
                   opts.executableName);
    // Add 'Output objects directory'.
    appendProperty(QByteArrayLiteral("OutputDirectory"),
                   opts.objectDirectory);
    // Add 'Output listing directory'.
    appendProperty(QByteArrayLiteral("ListingPath"),
                   opts.listingDirectory);

    // Add 'Create Executable/Library' item.
    const int isExecutable = (opts.targetType
                              == KeiluvUtils::ApplicationOutputType);
    const int isLibrary = (opts.targetType
                           == KeiluvUtils::LibraryOutputType);
    appendProperty(QByteArrayLiteral("CreateExecutable"),
                   isExecutable);
    appendProperty(QByteArrayLiteral("CreateLib"),
                   isLibrary);
}

} // namespace v5
} // namespace arm
} // namespace keiluv
} // namespace qbs
