/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "../scanner.h"
#include "cpp_global.h"
#include <cppscanner/cppscanner.h>

#include <tools/qbspluginmanager.h>
#include <tools/scannerpluginmanager.h>

struct Opaq : public qbs::Internal::CppScannerContext
{
    int currentIncludeIndex{0};
    int currentModuleIndex{0};
};

static void *openScanner(const unsigned short *filePath, const char *fileTags, int flags)
{
    std::unique_ptr<Opaq> opaq(new Opaq);
    const bool ok = qbs::Internal::scanCppFile(
        *opaq,
        QStringView(filePath),
        std::string_view(fileTags),
        flags & ScanForFileTagsFlag,
        flags & ScanForDependenciesFlag);
    if (!ok)
        return nullptr;
    return opaq.release();
}

static void closeScanner(void *ptr)
{
    const auto opaque = static_cast<Opaq *>(ptr);
    delete opaque;
}

static const char *next(void *opaq, int *size, int *flags)
{
    const auto opaque = static_cast<Opaq *>(opaq);
    if (opaque->currentIncludeIndex < opaque->includedFiles.size()) {
        const auto &result = opaque->includedFiles.at(opaque->currentIncludeIndex);
        ++opaque->currentIncludeIndex;
        *size = static_cast<int>(result.fileName.size());
        *flags = result.flags;
        return result.fileName.data();
    }
    if (opaque->currentModuleIndex < opaque->requiresModules.size()) {
        const auto &result = opaque->requiresModules.at(opaque->currentModuleIndex);
        ++opaque->currentModuleIndex;
        *size = result.size();
        *flags = SC_MODULE_FLAG;
        return result.data();
    }
    *size = 0;
    *flags = 0;
    return nullptr;
}

ScannerPlugin includeScanner = {
    "include_scanner",
    "cpp,cppm,cpp_pch_src,c,c_pch_src,objcpp,objcpp_pch_src,objc,objc_pch_src,rc",
    openScanner,
    closeScanner,
    next,
    ScannerUsesCppIncludePaths | ScannerRecursiveDependencies};

ScannerPlugin *cppScanners[] = {&includeScanner, nullptr};

static void QbsCppScannerPluginLoad()
{
    qbs::Internal::ScannerPluginManager::instance()->registerPlugins(cppScanners);
}

static void QbsCppScannerPluginUnload() {}

QBS_REGISTER_STATIC_PLUGIN(
    extern "C" CPPSCANNER_EXPORT,
    qbs_cpp_scanner,
    QbsCppScannerPluginLoad,
    QbsCppScannerPluginUnload)
