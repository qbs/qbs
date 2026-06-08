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

#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>

class CppScannerPlugin : public ScannerPlugin
{
public:
    QString name() const override { return QStringLiteral("cpp_include_scanner"); }
    ScannerScanResult scan(
        const QString &filePath,
        const char *fileTags,
        const QVariantMap &properties) const override;
    QStringList collectSearchPaths(
        const QVariantMap &properties,
        const QStringList &productBuildDirectories,
        const QStringList &fileTags) const override;

private:
    static QString getCompiledModuleSuffix(const QVariantMap &properties);
    static bool isPrecompiledHeaderSource(const QStringList &fileTags);
    static QStringList collectCppIncludePaths(const QVariantMap &properties, bool isPchSource);
    static bool modulesEnabled(const QVariantMap &properties);
};

ScannerScanResult CppScannerPlugin::scan(
    const QString &filePath, const char *fileTags, const QVariantMap &properties) const
{
    ScannerScanResult scanResult;
    qbs::Internal::CppScannerContext context;
    const bool ok = qbs::Internal::scanCppFile(context, filePath, fileTags, false, true);
    if (!ok)
        return scanResult;

    const QString baseDir = QFileInfo(filePath).path();
    const QString compiledModuleSuffix = getCompiledModuleSuffix(properties);

    scanResult.dependencies.reserve(context.includedFiles.size() + context.requiresModules.size());

    for (const auto &include : context.includedFiles) {
        QString includePath = QString::fromUtf8(include.fileName.data(), include.fileName.size());
        if (includePath.isEmpty())
            continue;

        // Resolve local includes relative to file directory
        if (include.flags & SC_LOCAL_INCLUDE_FLAG) {
            const QString localPath = baseDir + QLatin1Char('/') + includePath;
            if (QFile::exists(localPath))
                includePath = localPath;
        }

        scanResult.dependencies.append(includePath);
    }

    QStringList requiresModules;
    for (const auto &module : context.requiresModules) {
        QString modulePath = QString::fromUtf8(module.data(), module.size());
        if (modulePath.isEmpty())
            continue;
        requiresModules.append(modulePath);
        // Convert module name to file path
        modulePath = modulePath.replace(QLatin1Char(':'), QLatin1Char('-')) + compiledModuleSuffix;
        scanResult.dependencies.append(modulePath);
    }

    if (!context.providesModule.isEmpty()) {
        scanResult.scannerProperties.insert(
            QStringLiteral("providesModule"), QString::fromUtf8(context.providesModule));
    }
    if (!context.partOfModule.isEmpty()) {
        scanResult.scannerProperties.insert(
            QStringLiteral("partOfModule"), QString::fromUtf8(context.partOfModule));
    }
    if (context.isInterface)
        scanResult.scannerProperties.insert(QStringLiteral("isInterfaceModule"), true);
    if (!requiresModules.isEmpty())
        scanResult.scannerProperties.insert(QStringLiteral("requiresModules"), requiresModules);

    return scanResult;
}

QStringList CppScannerPlugin::collectSearchPaths(
    const QVariantMap &properties,
    const QStringList &productBuildDirectories,
    const QStringList &fileTags) const
{
    QStringList result = collectCppIncludePaths(properties, isPrecompiledHeaderSource(fileTags));
    if (modulesEnabled(properties)) {
        // Add cxx-modules subdirectory for each product build directory
        for (const QString &buildDir : productBuildDirectories) {
            result << buildDir + QStringLiteral("/cxx-modules");
        }
    }
    return result;
}

bool CppScannerPlugin::isPrecompiledHeaderSource(const QStringList &fileTags)
{
    // Keep in sync with the *_pch_src tags in share/qbs/modules/cpp.
    return fileTags.contains(QStringLiteral("c_pch_src"))
           || fileTags.contains(QStringLiteral("cpp_pch_src"))
           || fileTags.contains(QStringLiteral("objc_pch_src"))
           || fileTags.contains(QStringLiteral("objcpp_pch_src"));
}

QStringList CppScannerPlugin::collectCppIncludePaths(
    const QVariantMap &properties, bool isPchSource)
{
    QStringList result;
    const QVariantMap cpp = properties.value(QStringLiteral("cpp")).toMap();
    if (cpp.empty())
        return result;

    result << cpp.value(QStringLiteral("includePaths")).toStringList();
    // System headers are tracked for every artifact when treatSystemHeadersAsDependencies is
    // set, and additionally for precompiled-header sources when pchDependsOnSystemHeaders is
    // set -- a PCH bakes in the system headers it includes, so they are genuine dependencies.
    const bool useSystemHeaders
        = cpp.value(QStringLiteral("treatSystemHeadersAsDependencies")).toBool()
          || (isPchSource && cpp.value(QStringLiteral("pchDependsOnSystemHeaders")).toBool());
    if (useSystemHeaders) {
        result << cpp.value(QStringLiteral("systemIncludePaths")).toStringList()
               << cpp.value(QStringLiteral("distributionIncludePaths")).toStringList()
               << cpp.value(QStringLiteral("compilerIncludePaths")).toStringList();
    }
    result.removeDuplicates();
    return result;
}

bool CppScannerPlugin::modulesEnabled(const QVariantMap &properties)
{
    const QVariantMap cpp = properties.value(QStringLiteral("cpp")).toMap();
    if (cpp.empty())
        return false;
    return cpp.value(QStringLiteral("forceUseCxxModules")).toBool();
}

QString CppScannerPlugin::getCompiledModuleSuffix(const QVariantMap &properties)
{
    const QVariantMap cpp = properties.value(QStringLiteral("cpp")).toMap();
    if (cpp.empty())
        return {};
    return cpp.value(QStringLiteral("compiledModuleSuffix")).toString();
}

static void QbsCppScannerPluginLoad()
{
    qbs::Internal::ScannerPluginManager::instance()->registerScanner(
        std::make_unique<CppScannerPlugin>());
}

static void QbsCppScannerPluginUnload() {}

QBS_REGISTER_STATIC_PLUGIN(
    extern "C" CPPSCANNER_EXPORT,
    qbs_cpp_scanner,
    QbsCppScannerPluginLoad,
    QbsCppScannerPluginUnload)
