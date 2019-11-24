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

#include "qtmocscanner.h"

#include "artifact.h"
#include "depscanner.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "rawscanresults.h"
#include <language/scriptengine.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <plugins/scanner/scanner.h>
#include <tools/fileinfo.h>
#include <tools/scannerpluginmanager.h>
#include <tools/scripttools.h>

#include <QtCore/qdebug.h>

#include <QtScript/qscriptcontext.h>
#include <QtScript/qscriptengine.h>

namespace qbs {
namespace Internal {

struct CommonFileTags
{
    const FileTag cpp = "cpp";
    const FileTag cppCombine = "cpp.combine";
    const FileTag hpp = "hpp";
    const FileTag moc_cpp = "moc_cpp";
    const FileTag moc_cpp_plugin = "moc_cpp_plugin";
    const FileTag moc_hpp_plugin = "moc_hpp_plugin";
    const FileTag moc_hpp = "moc_hpp";
    const FileTag objcpp = "objcpp";
    const FileTag objcppCombine = "objcpp.combine";
};

Q_GLOBAL_STATIC(CommonFileTags, commonFileTags)

class QtScanner : public DependencyScanner
{
public:
    QtScanner(const DependencyScanner &actualScanner)
        : m_id(QStringLiteral("qt") + actualScanner.id()) {}

private:
    QStringList collectSearchPaths(Artifact *) override { return {}; }
    QStringList collectDependencies(Artifact *, FileResourceBase *, const char *) override
    {
        return {};
    }
    bool recursive() const override { return false; }
    const void *key() const override { return nullptr; }
    QString createId() const override { return m_id; }
    bool areModulePropertiesCompatible(const PropertyMapConstPtr &,
                                       const PropertyMapConstPtr &) const override
    {
        return true;
    }
    bool cacheIsPerFile() const override { return false; }

    const QString m_id;
};

static QString qtMocScannerJsName() { return QStringLiteral("QtMocScanner"); }

QtMocScanner::QtMocScanner(const ResolvedProductPtr &product, QScriptValue targetScriptValue)
    : m_tags(*commonFileTags())
    , m_product(product)
    , m_targetScriptValue(targetScriptValue)
{
    const auto engine = static_cast<ScriptEngine *>(targetScriptValue.engine());
    QScriptValue scannerObj = engine->newObject();
    targetScriptValue.setProperty(qtMocScannerJsName(), scannerObj);
    QScriptValue applyFunction = engine->newFunction(&js_apply, this);
    scannerObj.setProperty(QStringLiteral("apply"), applyFunction);
}

QtMocScanner::~QtMocScanner()
{
    m_targetScriptValue.setProperty(qtMocScannerJsName(), QScriptValue());
}

static RawScanResult runScanner(ScannerPlugin *scanner, const Artifact *artifact)
{
    const QString &filepath = artifact->filePath();
    QtScanner depScanner((PluginDependencyScanner(scanner)));
    RawScanResults &rawScanResults
            = artifact->product->topLevelProject()->buildData->rawScanResults;
    RawScanResults::ScanData &scanData = rawScanResults.findScanData(artifact, &depScanner,
                                                                     artifact->properties);
    if (scanData.lastScanTime < artifact->timestamp()) {
        FileTags tags = artifact->fileTags();
        if (tags.contains(commonFileTags->cppCombine)) {
            tags.remove(commonFileTags->cppCombine);
            tags.insert(commonFileTags->cpp);
        }
        if (tags.contains(commonFileTags->objcppCombine)) {
            tags.remove(commonFileTags->objcppCombine);
            tags.insert(commonFileTags->objcpp);
        }
        const QByteArray tagsForScanner = tags.toStringList().join(QLatin1Char(',')).toLatin1();
        void *opaq = scanner->open(filepath.utf16(), tagsForScanner.constData(),
                                   ScanForDependenciesFlag | ScanForFileTagsFlag);
        if (!opaq || !scanner->additionalFileTags)
            return scanData.rawScanResult;

        scanData.rawScanResult.additionalFileTags.clear();
        scanData.rawScanResult.deps.clear();
        int length = 0;
        const char **szFileTagsFromScanner = scanner->additionalFileTags(opaq, &length);
        if (szFileTagsFromScanner) {
            for (int i = length; --i >= 0;)
                scanData.rawScanResult.additionalFileTags += szFileTagsFromScanner[i];
        }

        QString baseDirOfInFilePath = artifact->dirPath();
        forever {
            int flags = 0;
            const char *szOutFilePath = scanner->next(opaq, &length, &flags);
            if (szOutFilePath == nullptr)
                break;
            QString includedFilePath = QString::fromLocal8Bit(szOutFilePath, length);
            if (includedFilePath.isEmpty())
                continue;
            bool isLocalInclude = (flags & SC_LOCAL_INCLUDE_FLAG);
            if (isLocalInclude) {
                QString localFilePath = FileInfo::resolvePath(baseDirOfInFilePath, includedFilePath);
                if (FileInfo::exists(localFilePath))
                    includedFilePath = localFilePath;
            }
            scanData.rawScanResult.deps.emplace_back(includedFilePath);
        }

        scanner->close(opaq);
        scanData.lastScanTime = FileTime::currentTime();
    }
    return scanData.rawScanResult;
}

void QtMocScanner::findIncludedMocCppFiles()
{
    if (!m_includedMocCppFiles.empty())
        return;

    qCDebug(lcMocScan) << "looking for included moc_XXX.cpp files";

    static const FileTags mocCppTags = {m_tags.cpp, m_tags.objcpp};
    for (Artifact *artifact : m_product->lookupArtifactsByFileTags(mocCppTags)) {
        const RawScanResult scanResult = runScanner(m_cppScanner, artifact);
        for (const RawScannedDependency &dependency : scanResult.deps) {
            QString includedFileName = dependency.fileName();
            if (includedFileName.startsWith(QLatin1String("moc_"))
                    && includedFileName.endsWith(QLatin1String(".cpp"))) {
                qCDebug(lcMocScan) << artifact->fileName() << "includes" << includedFileName;
                includedFileName.remove(0, 4);
                includedFileName.chop(4);
                m_includedMocCppFiles.insert(includedFileName, artifact->fileName());
            }
        }
    }
}

QScriptValue QtMocScanner::js_apply(QScriptContext *ctx, QScriptEngine *engine,
                                    QtMocScanner *that)
{
    QScriptValue input = ctx->argument(0);
    return that->apply(engine, attachedPointer<Artifact>(input));
}

static QScriptValue scannerCountError(QScriptEngine *engine, size_t scannerCount,
        const QString &fileTag)
{
    return engine->currentContext()->throwError(
                Tr::tr("There are %1 scanners for the file tag %2. "
                       "Expected is exactly one.").arg(scannerCount).arg(fileTag));
}

QScriptValue QtMocScanner::apply(QScriptEngine *engine, const Artifact *artifact)
{
    if (!m_cppScanner) {
        auto scanners = ScannerPluginManager::scannersForFileTag(m_tags.cpp);
        if (scanners.size() != 1)
            return scannerCountError(engine, scanners.size(), m_tags.cpp.toString());
        m_cppScanner = scanners.front();
    }

    qCDebug(lcMocScan).noquote() << "scanning" << artifact->toString();

    bool hasQObjectMacro = false;
    bool mustCompile = false;
    bool hasPluginMetaDataMacro = false;
    const bool isHeaderFile = artifact->fileTags().contains(m_tags.hpp);

    RawScanResult scanResult = runScanner(m_cppScanner, artifact);
    if (scanResult.additionalFileTags.empty() && artifact->fileTags().contains("mocable")) {
        if (isHeaderFile) {
            scanResult.additionalFileTags.insert(m_tags.moc_hpp);
        } else if (artifact->fileTags().contains(m_tags.cpp)
                   || artifact->fileTags().contains(m_tags.cppCombine)
                   || artifact->fileTags().contains(m_tags.objcpp)
                   || artifact->fileTags().contains(m_tags.objcppCombine)) {
            scanResult.additionalFileTags.insert(m_tags.moc_cpp);
        }
    }
    if (!scanResult.additionalFileTags.empty()) {
        if (isHeaderFile) {
            if (scanResult.additionalFileTags.contains(m_tags.moc_hpp))
                hasQObjectMacro = true;
            if (scanResult.additionalFileTags.contains(m_tags.moc_hpp_plugin)) {
                hasQObjectMacro = true;
                hasPluginMetaDataMacro = true;
            }
            findIncludedMocCppFiles();
            if (!m_includedMocCppFiles.contains(FileInfo::completeBaseName(artifact->fileName())))
                mustCompile = true;
        } else {
            if (scanResult.additionalFileTags.contains(m_tags.moc_cpp))
                hasQObjectMacro = true;
            if (scanResult.additionalFileTags.contains(m_tags.moc_cpp_plugin)) {
                hasQObjectMacro = true;
                hasPluginMetaDataMacro = true;
            }
        }
    }

    qCDebug(lcMocScan) << "hasQObjectMacro:" << hasQObjectMacro
                          << "mustCompile:" << mustCompile
                          << "hasPluginMetaDataMacro:" << hasPluginMetaDataMacro;

    QScriptValue obj = engine->newObject();
    obj.setProperty(QStringLiteral("hasQObjectMacro"), hasQObjectMacro);
    obj.setProperty(QStringLiteral("mustCompile"), mustCompile);
    obj.setProperty(QStringLiteral("hasPluginMetaDataMacro"), hasPluginMetaDataMacro);
    static_cast<ScriptEngine *>(engine)->setUsesIo();
    return obj;
}

} // namespace Internal
} // namespace qbs
