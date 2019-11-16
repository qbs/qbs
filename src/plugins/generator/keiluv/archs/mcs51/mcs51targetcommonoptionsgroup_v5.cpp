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

#include "mcs51targetcommonoptionsgroup_v5.h"

#include "../../keiluvutils.h"

#include <generators/generatorutils.h>

namespace qbs {
namespace keiluv {
namespace mcs51 {
namespace v5 {

namespace {

struct CommonPageOptions final
{
    explicit CommonPageOptions(const Project &qbsProject,
                               const ProductData &qbsProduct)
    {
        Q_UNUSED(qbsProject)

        const auto &qbsProps = qbsProduct.moduleProperties();
        const auto flags = KeiluvUtils::cppModuleCompilerFlags(qbsProps);

        // Browse information.
        if (flags.contains(QLatin1String("BROWSE"), Qt::CaseInsensitive))
            browseInfo = true;

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
    }

    int browseInfo = false;
    int debugInfo = false;
    QString executableName;
    QString objectDirectory;
    QString listingDirectory;
    KeiluvUtils::OutputBinaryType targetType =
            KeiluvUtils::ApplicationOutputType;
};

} // namespace

Mcs51TargetCommonOptionsGroup::Mcs51TargetCommonOptionsGroup(
        const qbs::Project &qbsProject,
        const qbs::ProductData &qbsProduct)
    : gen::xml::PropertyGroup("TargetCommonOption")
{
    const CommonPageOptions opts(qbsProject, qbsProduct);

    // Add 'Generic 8051 device' items,
    // because we can't detect a target device
    // form the present command lines.
    appendProperty(QByteArrayLiteral("Device"),
                   QByteArrayLiteral("8051 (all Variants)"));
    appendProperty(QByteArrayLiteral("Vendor"),
                   QByteArrayLiteral("Generic"));
    appendProperty(QByteArrayLiteral("DeviceId"),
                   QByteArrayLiteral("2994"));

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
} // namespace mcs51
} // namespace keiluv
} // namespace qbs
