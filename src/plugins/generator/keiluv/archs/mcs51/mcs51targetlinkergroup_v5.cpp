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

#include "mcs51targetlinkergroup_v5.h"
#include "mcs51utils.h"

#include "../../keiluvutils.h"

namespace qbs {
namespace keiluv {
namespace mcs51 {
namespace v5 {

namespace {

struct LinkerPageOptions final
{
    explicit LinkerPageOptions(const Project &qbsProject,
                               const ProductData &qbsProduct)
    {
        Q_UNUSED(qbsProject)

        const auto &qbsProps = qbsProduct.moduleProperties();
        const auto flags = qbs::KeiluvUtils::cppModuleLinkerFlags(qbsProps);

        // Handle all 'BIT' memory flags.
        parseMemory(flags, QStringLiteral("BIT"),
                    bitAddresses, bitSegments);
        // Handle all 'CODE' memory flags.
        parseMemory(flags, QStringLiteral("CODE"),
                    codeAddresses, codeSegments);
        // Handle all 'DATA' memory flags.
        parseMemory(flags, QStringLiteral("DATA"),
                    dataAddresses, dataSegments);
        // Handle all 'IDATA' memory flags.
        parseMemory(flags, QStringLiteral("IDATA"),
                    idataAddresses, idataSegments);
        // Handle all 'PDATA' memory flags.
        parseMemory(flags, QStringLiteral("PDATA"),
                    pdataAddresses, pdataSegments);
        // Handle all 'XDATA' memory flags.
        parseMemory(flags, QStringLiteral("XDATA"),
                    xdataAddresses, xdataSegments);

        // Enumerate all flags in a form like:
        // 'PRECEDE(foo, bar) PRECEDE(baz)'.
        const auto precedeValues = KeiluvUtils::flagValues(
                    flags, QStringLiteral("PRECEDE"));
        for (const auto &precedeValue : precedeValues) {
            const auto parts = KeiluvUtils::flagValueParts(precedeValue);
            precedeSegments.reserve(precedeSegments.size() + parts.count());
            std::copy(parts.cbegin(), parts.cend(),
                      std::back_inserter(precedeSegments));
        }

        // Enumerate all flags in a form like:
        // 'STACK(foo, bar) STACK(baz)'.
        const auto stackValues = KeiluvUtils::flagValues(
                    flags, QStringLiteral("STACK"));
        for (const auto &stackValue : stackValues) {
            const auto parts = KeiluvUtils::flagValueParts(stackValue);
            stackSegments.reserve(stackSegments.size() + parts.count());
            std::copy(parts.cbegin(), parts.cend(),
                      std::back_inserter(stackSegments));
        }

        // Interpret other linker flags as a misc controls (exclude only
        // that flags which are was already handled).
        for (const auto &flag : flags) {
            if (flag.startsWith(QLatin1String("BIT"),
                                Qt::CaseInsensitive)
                    || flag.startsWith(QLatin1String("CODE"),
                                       Qt::CaseInsensitive)
                    || flag.startsWith(QLatin1String("DATA"),
                                       Qt::CaseInsensitive)
                    || flag.startsWith(QLatin1String("IDATA"),
                                       Qt::CaseInsensitive)
                    || flag.startsWith(QLatin1String("PDATA"),
                                       Qt::CaseInsensitive)
                    || flag.startsWith(QLatin1String("XDATA"),
                                       Qt::CaseInsensitive)
                    || flag.startsWith(QLatin1String("PRECEDE"),
                                       Qt::CaseInsensitive)
                    || flag.startsWith(QLatin1String("STACK"),
                                       Qt::CaseInsensitive)
                    ) {
                continue;
            }
            miscControls.push_back(flag);
        }
    }

    static void parseMemory(const QStringList &flags,
                            const QString &flagKey,
                            QStringList &destAddresses,
                            QStringList &destSegments)
    {
        // Handle all flags in a form like:
        // 'FLAGKEY(0x00-0x20, 30, foo, bar(0x40)) FLAGKEY(baz)'.
        const auto values = KeiluvUtils::flagValues(flags, flagKey);
        for (const auto &value : values) {
            const auto parts = KeiluvUtils::flagValueParts(value);
            for (const auto &part : parts) {
                if (part.contains(QLatin1Char('-'))) {
                    // Seems, it is an address range.
                    destAddresses.push_back(part);
                } else {
                    // Check on address (specified in decimal
                    // or hexadecimal form).
                    bool ok = false;
                    part.toInt(&ok, 16);
                    if (!ok)
                        part.toInt(&ok, 10);
                    if (ok) {
                        // Seems, it is just a single address.
                        destAddresses.push_back(part);
                    } else {
                        // Seems it is a segment name.
                        destSegments.push_back(part);
                    }
                }
            }
        }
    }

    QStringList bitAddresses;
    QStringList bitSegments;
    QStringList codeAddresses;
    QStringList codeSegments;
    QStringList dataAddresses;
    QStringList dataSegments;
    QStringList idataAddresses;
    QStringList idataSegments;
    QStringList pdataAddresses;
    QStringList pdataSegments;
    QStringList xdataAddresses;
    QStringList xdataSegments;

    QStringList precedeSegments;
    QStringList stackSegments;

    QStringList miscControls;
};

} // namespace

Mcs51TargetLinkerGroup::Mcs51TargetLinkerGroup(
        const qbs::Project &qbsProject,
        const qbs::ProductData &qbsProduct)
    : gen::xml::PropertyGroup("Lx51")
{
    const LinkerPageOptions opts(qbsProject, qbsProduct);

    // Add 'Misc Controls' item.
    appendMultiLineProperty(QByteArrayLiteral("MiscControls"),
                            opts.miscControls, QLatin1Char(' '));

    // Add 'Use Memory Layout from Target Dialog' item.
    // Note: we always disable it, as we expect that
    // the layout will be specified from the linker's
    // command line.
    appendProperty(QByteArrayLiteral("UseMemoryFromTarget"),
                   0);

    // Add 'Bit Range' item.
    appendMultiLineProperty(QByteArrayLiteral("BitBaseAddress"),
                            opts.bitAddresses);
    // Add 'Code Range' item.
    appendMultiLineProperty(QByteArrayLiteral("CodeBaseAddress"),
                            opts.codeAddresses);
    // Add 'Data Range' item.
    appendMultiLineProperty(QByteArrayLiteral("DataBaseAddress"),
                            opts.dataAddresses);
    // Add 'IData Range' item.
    appendMultiLineProperty(QByteArrayLiteral("IDataBaseAddress"),
                            opts.idataAddresses);
    // Add 'PData Range' item.
    appendMultiLineProperty(QByteArrayLiteral("PDataBaseAddress"),
                            opts.pdataAddresses);
    // Add 'XData Range' item.
    appendMultiLineProperty(QByteArrayLiteral("XDataBaseAddress"),
                            opts.xdataAddresses);

    // Add 'Bit Segment' item.
    appendMultiLineProperty(QByteArrayLiteral("BitSegmentName"),
                            opts.bitSegments);
    // Add 'Code Segment' item.
    appendMultiLineProperty(QByteArrayLiteral("CodeSegmentName"),
                            opts.codeSegments);
    // Add 'Data Segment' item.
    appendMultiLineProperty(QByteArrayLiteral("DataSegmentName"),
                            opts.dataSegments);
    // Add 'IData Segment' item.
    appendMultiLineProperty(QByteArrayLiteral("IDataSegmentName"),
                            opts.idataSegments);

    // Note: PData has not segments!

    // Add 'XData Segment' item.
    appendMultiLineProperty(QByteArrayLiteral("XDataSegmentName"),
                            opts.xdataSegments);
    // Add 'Precede' item.
    appendMultiLineProperty(QByteArrayLiteral("Precede"),
                            opts.precedeSegments);
    // Add 'Stack' item.
    appendMultiLineProperty(QByteArrayLiteral("Stack"),
                            opts.stackSegments);
}

} // namespace v5
} // namespace mcs51
} // namespace keiluv
} // namespace qbs
