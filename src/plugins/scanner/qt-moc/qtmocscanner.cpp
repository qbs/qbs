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

#if defined(WIN32) || defined(_WIN32)
#define SCANNER_EXPORT __declspec(dllexport)
#else
#define SCANNER_EXPORT __attribute__((visibility("default")))
#endif

#include "../scanner.h"

#include <cppscanner/cppscanner.h>
#include <tools/qbspluginmanager.h>
#include <tools/scannerpluginmanager.h>

#include <QtCore/qfileinfo.h>
#include <QtCore/qglobal.h>
#include <QtCore/qset.h>

#include <tools/span.h>

namespace qbs::Internal {

span<const std::string_view> additionalFileTags(const CppScannerContext &context)
{
    static const std::string_view thMocCpp[] = {"moc_cpp"};
    static const std::string_view thMocHpp[] = {"moc_hpp"};
    static const std::string_view thMocPluginHpp[] = {"moc_hpp_plugin"};
    static const std::string_view thMocPluginCpp[] = {"moc_cpp_plugin"};

    if (context.hasQObjectMacro) {
        switch (context.fileType) {
        case CppScannerContext::FT_CPP:
        case CppScannerContext::FT_CPPM:
        case CppScannerContext::FT_OBJCPP:
            return {context.hasPluginMetaDataMacro ? thMocPluginCpp : thMocCpp, 1};
        case CppScannerContext::FT_HPP:
            return {context.hasPluginMetaDataMacro ? thMocPluginHpp : thMocHpp, 1};

        default:
            break;
        }
    }
    return {};
}

} // namespace qbs::Internal

class MocScannerPlugin : public ScannerPlugin
{
public:
    QString name() const override { return QStringLiteral("qt_moc_scanner"); }
    ScannerScanResult scan(
        const QString &filePath,
        const char *fileTags,
        const QVariantMap &properties) const override;
    QStringList collectSearchPaths(
        const QVariantMap &properties,
        const QStringList &productBuildDirectories,
        const QStringList &fileTags) const override;
};

static QString normalizedMocScanTags(const char *fileTags)
{
    QString tags = QString::fromLatin1(fileTags);
    tags.replace(QStringLiteral("cpp.combine"), QStringLiteral("cpp"));
    tags.replace(QStringLiteral("objcpp.combine"), QStringLiteral("objcpp"));
    return tags;
}

ScannerScanResult MocScannerPlugin::scan(
    const QString &filePath, const char *fileTags, const QVariantMap &properties) const
{
    Q_UNUSED(properties);

    ScannerScanResult scanResult;
    qbs::Internal::CppScannerContext context;
    const QString tags = normalizedMocScanTags(fileTags);
    const bool ok = qbs::Internal::scanCppFile(
        context, filePath, tags.toLatin1().constData(), true, true);
    if (!ok)
        return scanResult;

    QSet<QString> additionalTags;
    for (const auto tag : qbs::Internal::additionalFileTags(context))
        additionalTags.insert(QString::fromUtf8(tag.data(), int(tag.size())));

    if (additionalTags.isEmpty() && tags.contains(QStringLiteral("mocable"))) {
        if (tags.contains(QStringLiteral("hpp")))
            additionalTags.insert(QStringLiteral("moc_hpp"));
        else if (
            tags.contains(QStringLiteral("cpp")) || tags.contains(QStringLiteral("cppm"))
            || tags.contains(QStringLiteral("objcpp"))) {
            additionalTags.insert(QStringLiteral("moc_cpp"));
        }
    }

    bool hasQObjectMacro = false;
    bool hasPluginMetaDataMacro = context.hasPluginMetaDataMacro;
    const bool isHeader = tags.contains(QStringLiteral("hpp"));

    if (!additionalTags.isEmpty()) {
        if (isHeader) {
            if (additionalTags.contains(QStringLiteral("moc_hpp")))
                hasQObjectMacro = true;
            if (additionalTags.contains(QStringLiteral("moc_hpp_plugin"))) {
                hasQObjectMacro = true;
                hasPluginMetaDataMacro = true;
            }
        } else {
            if (additionalTags.contains(QStringLiteral("moc_cpp")))
                hasQObjectMacro = true;
            if (additionalTags.contains(QStringLiteral("moc_cpp_plugin"))) {
                hasQObjectMacro = true;
                hasPluginMetaDataMacro = true;
            }
        }
    }

    if (hasQObjectMacro)
        scanResult.scannerProperties.insert(QStringLiteral("hasQObjectMacro"), true);
    if (hasPluginMetaDataMacro)
        scanResult.scannerProperties.insert(QStringLiteral("hasPluginMetaDataMacro"), true);

    const bool isCppSource = tags.contains(QStringLiteral("cpp"))
                             || tags.contains(QStringLiteral("cppm"))
                             || tags.contains(QStringLiteral("objcpp"));
    if (isCppSource) {
        QStringList includedMocCppBaseNames;
        for (const auto &include : context.includedFiles) {
            const QString includePath = QString::fromUtf8(
                include.fileName.data(), include.fileName.size());
            if (includePath.isEmpty())
                continue;
            const QString fileName = QFileInfo(includePath).fileName();
            if (fileName.startsWith(QLatin1String("moc_"))
                && fileName.endsWith(QLatin1String(".cpp"))) {
                QString baseName = fileName;
                baseName.chop(4);
                baseName.remove(0, 4);
                includedMocCppBaseNames.append(baseName);
            }
        }
        if (!includedMocCppBaseNames.isEmpty()) {
            scanResult.scannerProperties.insert(
                QStringLiteral("includedMocCppBaseNames"), includedMocCppBaseNames);
        }
    }

    return scanResult;
}

QStringList MocScannerPlugin::collectSearchPaths(
    const QVariantMap &properties,
    const QStringList &productBuildDirectories,
    const QStringList &fileTags) const
{
    Q_UNUSED(properties);
    Q_UNUSED(productBuildDirectories);
    Q_UNUSED(fileTags);
    return {};
}

static void QbsQtMocScannerPluginLoad()
{
    qbs::Internal::ScannerPluginManager::instance()->registerScanner(
        std::make_unique<MocScannerPlugin>());
}

static void QbsQtMocScannerPluginUnload() {}

QBS_REGISTER_STATIC_PLUGIN(
    extern "C" SCANNER_EXPORT,
    qbs_qt_moc_scanner,
    QbsQtMocScannerPluginLoad,
    QbsQtMocScannerPluginUnload)
