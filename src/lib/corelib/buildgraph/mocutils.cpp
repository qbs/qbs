/****************************************************************************
**
** Copyright (C) 2026 Ivan Komissarov (abbapoh@gmail.com).
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "mocutils.h"

#include "artifact.h"
#include "projectbuilddata.h"
#include "rawscanresults.h"

#include <language/language.h>

namespace qbs {
namespace Internal {

static const ResolvedScanner *mocScanner(const ResolvedProduct *product)
{
    for (const ResolvedScannerPtr &scanner : product->scanners) {
        if (scanner->scannerId == QLatin1String("Qt.core.moc")) {
            return scanner.get();
        }
    }
    return nullptr;
}

QStringList gatherIncludedMocCppBaseNames(const ResolvedProduct *product)
{
    if (!product->buildData)
        return {};

    const ResolvedScanner * const scanner = mocScanner(product);
    if (!scanner)
        return {};

    const ProjectBuildData * const projectBuildData = product->topLevelProject()->buildData.get();
    if (!projectBuildData)
        return {};

    const RawScanResults &rawScanResults = projectBuildData->rawScanResults;
    const QString &scannerId = scanner->scannerId;
    static const FileTags mocCppTags = {"cpp", "cpp.combine", "cppm", "objcpp", "objcpp.combine"};
    QSet<QString> baseNames;

    for (Artifact * const artifact : product->lookupArtifactsByFileTags(mocCppTags)) {
        const RawScanResults::ScanData * const scanData = rawScanResults.existingScanData(
            artifact,
            scannerId,
            artifact->properties,
            [scanner](const PropertyMapConstPtr &lhs, const PropertyMapConstPtr &rhs) {
                return areResolvedScannerModulePropertiesCompatible(*scanner, lhs, rhs);
            });
        if (!scanData)
            continue;

        for (const QString &baseName : scanData->rawScanResult.scannerProperties
                                           .value(QStringLiteral("includedMocCppBaseNames"))
                                           .toStringList()) {
            baseNames.insert(baseName);
        }
    }

    return rangeTo<QStringList>(baseNames);
}

} // namespace Internal
} // namespace qbs
