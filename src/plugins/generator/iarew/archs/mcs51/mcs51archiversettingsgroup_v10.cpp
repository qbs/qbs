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

#include "mcs51archiversettingsgroup_v10.h"

#include "../../iarewutils.h"

namespace qbs {
namespace iarew {
namespace mcs51 {
namespace v10 {

constexpr int kArchiverArchiveVersion = 2;
constexpr int kArchiverDataVersion = 1;

namespace {

// Output page options.

struct OutputPageOptions final
{
    explicit OutputPageOptions(const QString &baseDirectory,
                               const ProductData &qbsProduct)
    {
        outputFile = QLatin1String("$PROJ_DIR$/")
                + gen::utils::targetBinaryPath(baseDirectory, qbsProduct);
    }

    QString outputFile;
};

} // namespace

// Mcs51ArchiverSettingsGroup

Mcs51ArchiverSettingsGroup::Mcs51ArchiverSettingsGroup(
        const Project &qbsProject,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps)
{
    Q_UNUSED(qbsProductDeps)

    setName(QByteArrayLiteral("XAR"));
    setArchiveVersion(kArchiverArchiveVersion);
    setDataVersion(kArchiverDataVersion);
    setDataDebugInfo(gen::utils::debugInformation(qbsProduct));

    const QString buildRootDirectory = gen::utils::buildRootPath(qbsProject);
    buildOutputPage(buildRootDirectory, qbsProduct);
}

void Mcs51ArchiverSettingsGroup::buildOutputPage(
        const QString &baseDirectory,
        const ProductData &qbsProduct)
{
    const OutputPageOptions opts(baseDirectory, qbsProduct);
    // Add 'XAROverride' item (Override default).
    addOptionsGroup(QByteArrayLiteral("XAROverride"),
                    {1});
    // Add 'XAROutput2' item (Output filename).
    addOptionsGroup(QByteArrayLiteral("XAROutput2"),
                    {opts.outputFile});
}

} // namespace v10
} // namespace mcs51
} // namespace iarew
} // namespace qbs
