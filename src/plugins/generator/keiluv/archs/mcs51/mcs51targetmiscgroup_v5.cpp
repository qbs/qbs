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

#include "mcs51targetmiscgroup_v5.h"
#include "mcs51utils.h"

#include "../../keiluvutils.h"

namespace qbs {
namespace keiluv {
namespace mcs51 {
namespace v5 {

namespace {

struct MiscPageOptions final
{
    enum MemoryModel {
        SmallMemoryModel = 0,
        CompactMemoryModel,
        LargeMemoryModel
    };

    enum CodeRomSize {
        SmallCodeRomSize = 0,
        CompactCodeRomSize,
        LargeCodeRomSize
    };

    explicit MiscPageOptions(const Project &qbsProject,
                             const ProductData &qbsProduct)
    {
        Q_UNUSED(qbsProject)

        const auto &qbsProps = qbsProduct.moduleProperties();
        const auto flags = qbs::KeiluvUtils::cppModuleCompilerFlags(qbsProps);

        // Memory model.
        if (flags.contains(QLatin1String("COMPACT"), Qt::CaseInsensitive))
            memoryModel = CompactMemoryModel;
        else if (flags.contains(QLatin1String("LARGE"), Qt::CaseInsensitive))
            memoryModel = LargeMemoryModel;

        // Code ROM size.
        const auto sizeValue = KeiluvUtils::flagValue(
                    flags, QStringLiteral("ROM"));
        if (sizeValue == QLatin1String("SMALL"))
            coderomSize = SmallCodeRomSize;
        else if (sizeValue == QLatin1String("COMPACT"))
            coderomSize = CompactCodeRomSize;
    }

    MemoryModel memoryModel = SmallMemoryModel;
    CodeRomSize coderomSize = LargeCodeRomSize;
};

} // namespace

Mcs51TargetMiscGroup::Mcs51TargetMiscGroup(
        const qbs::Project &qbsProject,
        const qbs::ProductData &qbsProduct)
    : gen::xml::PropertyGroup("Target51Misc")
{
    const MiscPageOptions opts(qbsProject, qbsProduct);

    // Add 'Memory Model' options item.
    appendProperty(QByteArrayLiteral("MemoryModel"),
                   opts.memoryModel);
    // Add 'ROM Size' options item.
    appendProperty(QByteArrayLiteral("RomSize"),
                   opts.coderomSize);
}

} // namespace v5
} // namespace mcs51
} // namespace keiluv
} // namespace qbs
