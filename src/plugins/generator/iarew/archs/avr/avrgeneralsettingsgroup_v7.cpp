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

#include "avrgeneralsettingsgroup_v7.h"

#include "../../iarewutils.h"

namespace qbs {
namespace iarew {
namespace avr {
namespace v7 {

constexpr int kGeneralArchiveVersion = 12;
constexpr int kGeneralDataVersion = 10;

namespace {

struct TargetMcuEntry final
{
    QByteArray targetName;
    QByteArray targetFlag;
};

// Dictionary of known AVR MCU's and its compiler options.
const TargetMcuEntry mcusDict[] = {
    {"AT43USB320A", "at43usb320a"},
    {"AT43USB325", "at43usb325"},
    {"AT43USB326", "at43usb326"},
    {"AT43USB351M", "at43usb351m"},
    {"AT43USB353M", "at43usb353m"},
    {"AT43USB355", "at43usb355"},
    {"AT76C712", "at76c712"},
    {"AT76C713", "at76c713"},
    {"AT86RF401", "at86rf401"},
    {"AT90CAN128", "can128"},
    {"AT90CAN32", "can32"},
    {"AT90CAN64", "can64"},
    {"AT90PWM1", "pwm1"},
    {"AT90PWM161", "pwm161"},
    {"AT90PWM2", "pwm2"},
    {"AT90PWM216", "pwm216"},
    {"AT90PWM2B", "pwm2b"},
    {"AT90PWM3", "pwm3"},
    {"AT90PWM316", "pwm316"},
    {"AT90PWM3B", "pwm3b"},
    {"AT90PWM81", "pwm81"},
    {"AT90S1200", "1200"},
    {"AT90S2313", "2313"},
    {"AT90S2323", "2323"},
    {"AT90S2333", "2333"},
    {"AT90S2343", "2343"},
    {"AT90S4414", "4414"},
    {"AT90S4433", "4433"},
    {"AT90S4434", "4434"},
    {"AT90S8515", "8515"},
    {"AT90S8534", "8534"},
    {"AT90S8535", "8535"},
    {"AT90SCR050", "scr050"},
    {"AT90SCR075", "scr075"},
    {"AT90SCR100", "scr100"},
    {"AT90SCR200", "scr200"},
    {"AT90SCR400", "scr400"},
    {"AT90USB128", "usb128"},
    {"AT90USB1286", "usb1286"},
    {"AT90USB1287", "usb1287"},
    {"AT90USB162", "usb162"},
    {"AT90USB64", "usb64"},
    {"AT90USB646", "usb646"},
    {"AT90USB647", "usb647"},
    {"AT90USB82", "usb82"},
    {"AT94Kxx", "at94k"},
    {"ATA5272", "ata5272"},
    {"ATA5505", "ata5505"},
    {"ATA5700M322", "ata5700m322"},
    {"ATA5702M322", "ata5702m322"},
    {"ATA5781", "ata5781"},
    {"ATA5782", "ata5782"},
    {"ATA5783", "ata5783"},
    {"ATA5785", "ata5785"},
    {"ATA5787", "ata5787"},
    {"ATA5790", "ata5790"},
    {"ATA5790N", "ata5790n"},
    {"ATA5795", "ata5795"},
    {"ATA5830", "ata5830"},
    {"ATA5831", "ata5831"},
    {"ATA5832", "ata5832"},
    {"ATA5833", "ata5833"},
    {"ATA5835", "ata5835"},
    {"ATA6285", "ata6285"},
    {"ATA6286", "ata6286"},
    {"ATA6289", "ata6289"},
    {"ATA8210", "ata8210"},
    {"ATA8215", "ata8215"},
    {"ATA8510", "ata8510"},
    {"ATA8515", "ata8515"},
    {"ATmX224E", "mx224e"},
    {"ATmXT112SL", "mxt112sl"},
    {"ATmXT224", "mxt224"},
    {"ATmXT224E", "mxt224e"},
    {"ATmXT336S", "mxt336s"},
    {"ATmXT540S", "mxt540s"},
    {"ATmXT540S_RevA", "mxt540s_reva"},
    {"ATmXTS200", "mxts200"},
    {"ATmXTS220", "mxts220"},
    {"ATmXTS220E", "mxts220e"},
    {"ATmega007", "m007"},
    {"ATmega103", "m103"},
    {"ATmega128", "m128"},
    {"ATmega1280", "m1280"},
    {"ATmega1281", "m1281"},
    {"ATmega1284", "m1284"},
    {"ATmega1284P", "m1284p"},
    {"ATmega1284RFR2", "m1284rfr2"},
    {"ATmega128A", "m128a"},
    {"ATmega128RFA1", "m128rfa1"},
    {"ATmega128RFA2", "m128rfa2"},
    {"ATmega128RFR2", "m128rfr2"},
    {"ATmega16", "m16"},
    {"ATmega1608", "m1608"},
    {"ATmega1609", "m1609"},
    {"ATmega161", "m161"},
    {"ATmega162", "m162"},
    {"ATmega163", "m163"},
    {"ATmega164", "m164"},
    {"ATmega164A", "m164a"},
    {"ATmega164P", "m164p"},
    {"ATmega164PA", "m164pa"},
    {"ATmega165", "m165"},
    {"ATmega165A", "m165a"},
    {"ATmega165P", "m165p"},
    {"ATmega165PA", "m165pa"},
    {"ATmega168", "m168"},
    {"ATmega168A", "m168a"},
    {"ATmega168P", "m168p"},
    {"ATmega168PA", "m168pa"},
    {"ATmega168PB", "m168pb"},
    {"ATmega169", "m169"},
    {"ATmega169A", "m169a"},
    {"ATmega169P", "m169p"},
    {"ATmega169PA", "m169pa"},
    {"ATmega16A", "m16a"},
    {"ATmega16HVA", "m16hva"},
    {"ATmega16HVA2", "m16hva2"},
    {"ATmega16HVB", "m16hvb"},
    {"ATmega16M1", "m16m1"},
    {"ATmega16U2", "m16u2"},
    {"ATmega16U4", "m16u4"},
    {"ATmega2560", "m2560"},
    {"ATmega2561", "m2561"},
    {"ATmega2564RFR2", "m2564rfr2"},
    {"ATmega256RFA2", "m256rfa2"},
    {"ATmega256RFR2", "m256rfr2"},
    {"ATmega26HVG", "m26hvg"},
    {"ATmega32", "m32"},
    {"ATmega3208", "m3208"},
    {"ATmega3209", "m3209"},
    {"ATmega323", "m323"},
    {"ATmega324", "m324"},
    {"ATmega324A", "m324a"},
    {"ATmega324P", "m324p"},
    {"ATmega324PA", "m324pa"},
    {"ATmega324PB", "m324pb"},
    {"ATmega325", "m325"},
    {"ATmega3250", "m3250"},
    {"ATmega3250A", "m3250a"},
    {"ATmega3250P", "m3250p"},
    {"ATmega3250PA", "m3250pa"},
    {"ATmega325A", "m325a"},
    {"ATmega325P", "m325p"},
    {"ATmega325PA", "m325pa"},
    {"ATmega328", "m328"},
    {"ATmega328P", "m328p"},
    {"ATmega328PB", "m328pb"},
    {"ATmega329", "m329"},
    {"ATmega3290", "m3290"},
    {"ATmega3290A", "m3290a"},
    {"ATmega3290P", "m3290p"},
    {"ATmega3290PA", "m3290pa"},
    {"ATmega329A", "m329a"},
    {"ATmega329P", "m329p"},
    {"ATmega329PA", "m329pa"},
    {"ATmega32A", "m32a"},
    {"ATmega32C1", "m32c1"},
    {"ATmega32HVB", "m32hvb"},
    {"ATmega32M1", "m32m1"},
    {"ATmega32U2", "m32u2"},
    {"ATmega32U4", "m32u4"},
    {"ATmega32U6", "m32u6"},
    {"ATmega406", "m406"},
    {"ATmega48", "m48"},
    {"ATmega4808", "m4808"},
    {"ATmega4809", "m4809"},
    {"ATmega48A", "m48a"},
    {"ATmega48HVF", "m48hvf"},
    {"ATmega48P", "m48p"},
    {"ATmega48PA", "m48pa"},
    {"ATmega48PB", "m48pb"},
    {"ATmega4HVD", "m4hvd"},
    {"ATmega603", "m603"},
    {"ATmega64", "m64"},
    {"ATmega640", "m640"},
    {"ATmega644", "m644"},
    {"ATmega644A", "m644a"},
    {"ATmega644P", "m644p"},
    {"ATmega644PA", "m644pa"},
    {"ATmega644RFR2", "m644rfr2"},
    {"ATmega645", "m645"},
    {"ATmega6450", "m6450"},
    {"ATmega6450A", "m6450a"},
    {"ATmega6450P", "m6450p"},
    {"ATmega645A", "m645a"},
    {"ATmega645P", "m645p"},
    {"ATmega649", "m649"},
    {"ATmega6490", "m6490"},
    {"ATmega6490A", "m6490a"},
    {"ATmega6490P", "m6490p"},
    {"ATmega649A", "m649a"},
    {"ATmega649P", "m649p"},
    {"ATmega64A", "m64a"},
    {"ATmega64C1", "m64c1"},
    {"ATmega64HVE", "m256rfa2"},
    {"ATmega64HVE", "m64hve"},
    {"ATmega64HVE2", "m64hve2"},
    {"ATmega64M1", "m64m1"},
    {"ATmega64RFA2", "m64rfa2"},
    {"ATmega64RFR2", "m64rfr2"},
    {"ATmega8", "m8"},
    {"ATmega808", "m808"},
    {"ATmega809", "m809"},
    {"ATmega83", "m83"},
    {"ATmega8515", "m8515"},
    {"ATmega8535", "m8535"},
    {"ATmega88", "m88"},
    {"ATmega88A", "m88a"},
    {"ATmega88P", "m88p"},
    {"ATmega88PA", "m88pa"},
    {"ATmega88PB", "m88pb"},
    {"ATmega8A", "m8a"},
    {"ATmega8HVA", "m8hva"},
    {"ATmega8HVD", "m8hvd"},
    {"ATmega8U2", "m8u2"},
    {"ATtiny10", "tiny10"},
    {"ATtiny102", "tiny102"},
    {"ATtiny104", "tiny104"},
    {"ATtiny11", "tiny11"},
    {"ATtiny12", "tiny12"},
    {"ATtiny13", "tiny13"},
    {"ATtiny13A", "tiny13a"},
    {"ATtiny15", "tiny15"},
    {"ATtiny1604", "tiny1604"},
    {"ATtiny1606", "tiny1606"},
    {"ATtiny1607", "tiny1607"},
    {"ATtiny1614", "tiny1614"},
    {"ATtiny1616", "tiny1616"},
    {"ATtiny1617", "tiny1617"},
    {"ATtiny1634", "tiny1634"},
    {"ATtiny167", "tiny167"},
    {"ATtiny20", "tiny20"},
    {"ATtiny202", "tiny202"},
    {"ATtiny204", "tiny204"},
    {"ATtiny212", "tiny212"},
    {"ATtiny214", "tiny214"},
    {"ATtiny22", "tiny22"},
    {"ATtiny2313", "tiny2313"},
    {"ATtiny2313A", "tiny2313a"},
    {"ATtiny23U", "tiny23u"},
    {"ATtiny24", "tiny24"},
    {"ATtiny24A", "tiny24a"},
    {"ATtiny25", "tiny25"},
    {"ATtiny26", "tiny26"},
    {"ATtiny261", "tiny261"},
    {"ATtiny261A", "tiny261a"},
    {"ATtiny28", "tiny28"},
    {"ATtiny3214", "tiny3214"},
    {"ATtiny3216", "tiny3216"},
    {"ATtiny3217", "tiny3217"},
    {"ATtiny4", "tiny4"},
    {"ATtiny40", "tiny40"},
    {"ATtiny402", "tiny402"},
    {"ATtiny404", "tiny404"},
    {"ATtiny406", "tiny406"},
    {"ATtiny412", "tiny412"},
    {"ATtiny414", "tiny414"},
    {"ATtiny416", "tiny416"},
    {"ATtiny417", "tiny417"},
    {"ATtiny4313", "tiny4313"},
    {"ATtiny43U", "tiny43u"},
    {"ATtiny44", "tiny44"},
    {"ATtiny441", "tiny441"},
    {"ATtiny44A", "tiny44a"},
    {"ATtiny45", "tiny45"},
    {"ATtiny461", "tiny461"},
    {"ATtiny461A", "tiny461a"},
    {"ATtiny474", "tiny474"},
    {"ATtiny48", "tiny48"},
    {"ATtiny5", "tiny5"},
    {"ATtiny80", "tiny80"},
    {"ATtiny804", "tiny804"},
    {"ATtiny806", "tiny806"},
    {"ATtiny807", "tiny807"},
    {"ATtiny80_pre_2015", "tiny80_pre_2015"},
    {"ATtiny814", "tiny814"},
    {"ATtiny816", "tiny816"},
    {"ATtiny817", "tiny817"},
    {"ATtiny828", "tiny828"},
    {"ATtiny84", "tiny84"},
    {"ATtiny840", "tiny840"},
    {"ATtiny841", "tiny841"},
    {"ATtiny84A", "tiny84a"},
    {"ATtiny85", "tiny85"},
    {"ATtiny861", "tiny861"},
    {"ATtiny861A", "tiny861a"},
    {"ATtiny87", "tiny87"},
    {"ATtiny88", "tiny88"},
    {"ATtiny9", "tiny9"},
    {"ATxmega128A1", "xm128a1"},
    {"ATxmega128A1U", "xm128a1u"},
    {"ATxmega128A3", "xm128a3"},
    {"ATxmega128A3U", "xm128a3u"},
    {"ATxmega128A4", "xm128a4"},
    {"ATxmega128A4U", "xm128a4u"},
    {"ATxmega128B1", "xm128b1"},
    {"ATxmega128B3", "xm128b3"},
    {"ATxmega128C3", "xm128c3"},
    {"ATxmega128D3", "xm128d3"},
    {"ATxmega128D4", "xm128d4"},
    {"ATxmega16A4", "xm16a4"},
    {"ATxmega16A4U", "xm16a4u"},
    {"ATxmega16C4", "xm16c4"},
    {"ATxmega16D4", "xm16d4"},
    {"ATxmega16E5", "xm16e5"},
    {"ATxmega192A1", "xm192a1"},
    {"ATxmega192A3", "xm192a3"},
    {"ATxmega192A3U", "xm192a3u"},
    {"ATxmega192C3", "xm192c3"},
    {"ATxmega192D3", "xm192d3"},
    {"ATxmega256A1", "xm256a1"},
    {"ATxmega256A3", "xm256a3"},
    {"ATxmega256A3B", "xm256a3b"},
    {"ATxmega256A3BU", "xm256a3bu"},
    {"ATxmega256A3U", "xm256a3u"},
    {"ATxmega256B1", "xm256b1"},
    {"ATxmega256C3", "xm256c3"},
    {"ATxmega256D3", "xm256d3"},
    {"ATxmega32A4", "xm32a4"},
    {"ATxmega32A4U", "xm32a4u"},
    {"ATxmega32C3", "xm32c3"},
    {"ATxmega32C4", "xm32c4"},
    {"ATxmega32D3", "xm32d3"},
    {"ATxmega32D4", "xm32d4"},
    {"ATxmega32D4P", "xm32d4p"},
    {"ATxmega32E5", "xm32e5"},
    {"ATxmega32X1", "xm32x1"},
    {"ATxmega384A1", "xm384a1"},
    {"ATxmega384C3", "xm384c3"},
    {"ATxmega384D3", "xm384d3"},
    {"ATxmega64A1", "xm64a1"},
    {"ATxmega64A1U", "xm64a1u"},
    {"ATxmega64A3", "xm64a3"},
    {"ATxmega64A3U", "xm64a3u"},
    {"ATxmega64A4", "xm64a4"},
    {"ATxmega64A4U", "xm64a4u"},
    {"ATxmega64B1", "xm64b1"},
    {"ATxmega64B3", "xm64b3"},
    {"ATxmega64C3", "xm64c3"},
    {"ATxmega64D3", "xm64d3"},
    {"ATxmega64D4", "xm64d4"},
    {"ATxmega8E5", "xm8e5"},
    {"M3000", "m3000"},
    {"MaxBSE", "maxbse"},
};

// Target page options.

struct TargetPageOptions final
{
    enum MemoryModel {
        TinyModel,
        SmallModel,
        LargeModel,
        HugeModel
    };

    explicit TargetPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("driverFlags")});
        // Detect target MCU record.
        const QString mcuValue = IarewUtils::flagValue(
                    flags, QStringLiteral("--cpu")).toLower();
        targetMcu = mcuStringFromFlagValue(mcuValue);
        // Detect target memory model.
        const QString modelValue = IarewUtils::flagValue(
                    flags, QStringLiteral("-m"));
        if (modelValue == QLatin1Char('t'))
            memoryModel = TargetPageOptions::TinyModel;
        else if (modelValue == QLatin1Char('s'))
            memoryModel = TargetPageOptions::SmallModel;
        else if (modelValue == QLatin1Char('l'))
            memoryModel = TargetPageOptions::LargeModel;
        else if (modelValue == QLatin1Char('h'))
            memoryModel = TargetPageOptions::HugeModel;
        // Detect target EEPROM util size.
        eepromUtilSize = IarewUtils::flagValue(
                    flags, QStringLiteral("--eeprom_size")).toInt();
    }

    static QString mcuStringFromFlagValue(const QString &mcuValue)
    {
        const auto targetBegin = std::cbegin(mcusDict);
        const auto targetEnd = std::cend(mcusDict);
        const auto targetIt = std::find_if(targetBegin, targetEnd,
                                           [mcuValue](
                                           const TargetMcuEntry &entry) {
            return entry.targetFlag == mcuValue.toLatin1();
        });
        if (targetIt != targetEnd) {
            return QStringLiteral("%1\t%2")
                    .arg(QString::fromLatin1(targetIt->targetFlag),
                         QString::fromLatin1(targetIt->targetName));
        }
        return {};
    }

    QString targetMcu;
    MemoryModel memoryModel = TinyModel;
    int eepromUtilSize = 0;
};

// System page options.

struct SystemPageOptions final
{
    explicit SystemPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = gen::utils::cppStringModuleProperties(
                    qbsProps, {QStringLiteral("driverLinkerFlags"),
                               QStringLiteral("defines")});
        cstackSize = IarewUtils::flagValue(
                    flags, QStringLiteral("_..X_CSTACK_SIZE")).toInt();
        rstackSize = IarewUtils::flagValue(
                    flags, QStringLiteral("_..X_RSTACK_SIZE")).toInt();
    }

    int cstackSize = 0;
    int rstackSize = 0;
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
        PrintfSmallFormatter = 6,
        PrintfSmallNoMultibytesFormatter = 7,
        PrintfTinyFormatter = 8
    };

    enum ScanfFormatter {
        ScanfAutoFormatter = 0,
        ScanfFullFormatter = 1,
        ScanfFullNoMultibytesFormatter = 2,
        ScanfLargeFormatter = 3,
        ScanfLargeNoMultibytesFormatter = 4,
        ScanfSmallFormatter = 6,
        ScanfSmallNoMultibytesFormatter = 7
    };

    explicit LibraryOptionsPageOptions(const ProductData &qbsProduct)
    {
        const auto &qbsProps = qbsProduct.moduleProperties();
        const QStringList flags = IarewUtils::cppModuleLinkerFlags(qbsProps);
        for (const QString &flag : flags) {
            if (flag.endsWith(QLatin1String("_printf"), Qt::CaseInsensitive)) {
                const QString prop = flag.split(QLatin1Char('=')).at(0).toLower();
                if (prop == QLatin1String("-e_printffull"))
                    printfFormatter = LibraryOptionsPageOptions::PrintfFullFormatter;
                else if (prop == QLatin1String("-e_printffullnomb"))
                    printfFormatter = LibraryOptionsPageOptions::PrintfFullNoMultibytesFormatter;
                else if (prop == QLatin1String("-e_printflarge"))
                    printfFormatter = LibraryOptionsPageOptions::PrintfLargeFormatter;
                else if (prop == QLatin1String("-e_printflargenomb"))
                    printfFormatter = LibraryOptionsPageOptions::PrintfLargeNoMultibytesFormatter;
                else if (prop == QLatin1String("-e_printfsmall"))
                    printfFormatter = LibraryOptionsPageOptions::PrintfSmallFormatter;
                else if (prop == QLatin1String("-e_printfsmallnomb"))
                    printfFormatter = LibraryOptionsPageOptions::PrintfSmallNoMultibytesFormatter;
                else if (prop == QLatin1String("-printftiny"))
                    printfFormatter = LibraryOptionsPageOptions::PrintfTinyFormatter;
            } else if (flag.endsWith(QLatin1String("_scanf"), Qt::CaseInsensitive)) {
                const QString prop = flag.split(QLatin1Char('=')).at(0).toLower();
                if (prop == QLatin1String("-e_scanffull"))
                    scanfFormatter = LibraryOptionsPageOptions::ScanfFullFormatter;
                else if (prop == QLatin1String("-e_scanffullnomb"))
                    scanfFormatter = LibraryOptionsPageOptions::ScanfFullNoMultibytesFormatter;
                else if (prop == QLatin1String("-e_scanflarge"))
                    scanfFormatter = LibraryOptionsPageOptions::ScanfLargeFormatter;
                else if (prop == QLatin1String("-e_scanflargenomb"))
                    scanfFormatter = LibraryOptionsPageOptions::ScanfLargeNoMultibytesFormatter;
                else if (prop == QLatin1String("-e_scanfsmall"))
                    scanfFormatter = LibraryOptionsPageOptions::ScanfSmallFormatter;
                else if (prop == QLatin1String("-e_scanfsmallnomb"))
                    scanfFormatter = LibraryOptionsPageOptions::ScanfSmallNoMultibytesFormatter;
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
        FullDlibLibrary,
        CustomDlibLibrary,
        ClibLibrary,
        CustomClibLibrary,
        ThirdPartyLibrary
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

        if (flags.contains(QLatin1String("--dlib"))) {
            const QString dlibToolkitPath =
                    IarewUtils::dlibToolkitRootPath(qbsProduct);
            const QFileInfo configInfo(IarewUtils::flagValue(
                                           flags,
                                           QStringLiteral("--dlib_config")));
            const QString configFilePath = configInfo.absoluteFilePath();
            if (configFilePath.startsWith(dlibToolkitPath,
                                          Qt::CaseInsensitive)) {
                if (configFilePath.endsWith(QLatin1String("-n.h"),
                                            Qt::CaseInsensitive)) {
                    libraryType = LibraryConfigPageOptions::NormalDlibLibrary;
                } else if (configFilePath.endsWith(QLatin1String("-f.h"),
                                                   Qt::CaseInsensitive)) {
                    libraryType = LibraryConfigPageOptions::FullDlibLibrary;
                } else {
                    libraryType = LibraryConfigPageOptions::CustomDlibLibrary;
                }

                configPath = IarewUtils::toolkitRelativeFilePath(
                            baseDirectory, configFilePath);

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
                            baseDirectory, configFilePath);
            }
        } else if (flags.contains(QLatin1String("--clib"))) {
            const QString clibToolkitPath =
                    IarewUtils::clibToolkitRootPath(qbsProduct);
            // Find clib library inside of IAR toolkit directory.
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
                // This means that clib library is 'custom'
                // (but we don't know its path).
                libraryType = LibraryConfigPageOptions::CustomClibLibrary;
            }
        } else {
            libraryType = LibraryConfigPageOptions::NoLibrary;
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

// AvrGeneralSettingsGroup

AvrGeneralSettingsGroup::AvrGeneralSettingsGroup(
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
    buildSystemPage(qbsProduct);
    buildLibraryOptionsPage(qbsProduct);
    buildLibraryConfigPage(buildRootDirectory, qbsProduct);
    buildOutputPage(buildRootDirectory, qbsProduct);
}

void AvrGeneralSettingsGroup::buildTargetPage(
        const ProductData &qbsProduct)
{
    const TargetPageOptions opts(qbsProduct);
    // Add 'GenDeviceSelectMenu' item
    // (Processor configuration chooser).
    addOptionsGroup(QByteArrayLiteral("GenDeviceSelectMenu"),
                    {opts.targetMcu});
    // Add 'Variant Memory' item
    // (Memory model: tiny/small/large/huge).
    addOptionsGroup(QByteArrayLiteral("Variant Memory"),
                    {opts.memoryModel});
    // Add 'GGEepromUtilSize' item
    // (Utilize inbuilt EEPROM size, in bytes).
    addOptionsGroup(QByteArrayLiteral("GGEepromUtilSize"),
                    {opts.eepromUtilSize});
}

void AvrGeneralSettingsGroup::buildSystemPage(
        const ProductData &qbsProduct)
{
    const SystemPageOptions opts (qbsProduct);
    // Add 'SCCStackSize' item (Data stack
    // - CSTACK size in bytes).
    addOptionsGroup(QByteArrayLiteral("SCCStackSize"),
                    {opts.cstackSize});
    // Add 'SCRStackSize' item (Return address stack
    // - RSTACK depth in bytes).
    addOptionsGroup(QByteArrayLiteral("SCRStackSize"),
                    {opts.rstackSize});
}

void AvrGeneralSettingsGroup::buildLibraryOptionsPage(
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

void AvrGeneralSettingsGroup::buildLibraryConfigPage(
        const QString &baseDirectory,
        const ProductData &qbsProduct)
{
    const LibraryConfigPageOptions opts(baseDirectory, qbsProduct);
    // Add 'GRuntimeLibSelect' and 'GRuntimeLibSelectSlave' items
    // (Link with runtime: none/dlib/clib/etc).
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

void AvrGeneralSettingsGroup::buildOutputPage(
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

} // namespace v7
} // namespace avr
} // namespace iarew
} // namespace qbs
