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
    QStringList scan(const QString &filePath, const char *fileTags, const QVariantMap &properties)
        const override;
    QStringList collectSearchPaths(
        const QVariantMap &properties, const QStringList &productBuildDirectories) const override;

private:
    static QString getCompiledModuleSuffix(const QVariantMap &properties);
    static QStringList collectCppIncludePaths(const QVariantMap &properties);
    static bool modulesEnabled(const QVariantMap &properties);
};

QStringList CppScannerPlugin::scan(
    const QString &filePath, const char *fileTags, const QVariantMap &properties) const
{
    qbs::Internal::CppScannerContext context;
    const bool ok = qbs::Internal::scanCppFile(context, filePath, fileTags, false, true);
    if (!ok)
        return {};

    const QString baseDir = QFileInfo(filePath).path();
    const QString compiledModuleSuffix = getCompiledModuleSuffix(properties);

    QStringList results;
    results.reserve(context.includedFiles.size() + context.requiresModules.size());

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

        results.append(includePath);
    }

    for (const auto &module : context.requiresModules) {
        QString modulePath = QString::fromUtf8(module.data(), module.size());
        if (!modulePath.isEmpty()) {
            // Convert module name to file path
            modulePath = modulePath.replace(QLatin1Char(':'), QLatin1Char('-'))
                         + compiledModuleSuffix;
            results.append(modulePath);
        }
    }

    return results;
}

QStringList CppScannerPlugin::collectSearchPaths(
    const QVariantMap &properties, const QStringList &productBuildDirectories) const
{
    QStringList result = collectCppIncludePaths(properties);
    if (modulesEnabled(properties)) {
        // Add cxx-modules subdirectory for each product build directory
        for (const QString &buildDir : productBuildDirectories) {
            result << buildDir + QStringLiteral("/cxx-modules");
        }
    }
    return result;
}

QStringList CppScannerPlugin::collectCppIncludePaths(const QVariantMap &properties)
{
    QStringList result;
    const QVariantMap cpp = properties.value(QStringLiteral("cpp")).toMap();
    if (cpp.empty())
        return result;

    result << cpp.value(QStringLiteral("includePaths")).toStringList();
    const bool useSystemHeaders
        = cpp.value(QStringLiteral("treatSystemHeadersAsDependencies")).toBool();
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
