/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
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

#include "qtmocscanner.h"

#include "artifact.h"
#include "productbuilddata.h"
#include "scanresultcache.h"
#include <logging/translator.h>
#include <tools/fileinfo.h>
#include <tools/scannerpluginmanager.h>
#include <tools/scripttools.h>

#include <QScriptContext>
#include <QScriptEngine>
#include <QDebug>

namespace qbs {
namespace Internal {

QtMocScanner::QtMocScanner(const ResolvedProductPtr &product, QScriptValue targetScriptValue,
        const Logger &logger)
    : m_product(product)
    , m_targetScriptValue(targetScriptValue)
    , m_logger(logger)
    , m_scanResultCache(new ScanResultCache)
    , m_cppScanner(0)
    , m_hppScanner(0)
{
    QScriptEngine *engine = targetScriptValue.engine();
    QScriptValue scannerObj = engine->newObject();
    targetScriptValue.setProperty(QLatin1String("QtMocScanner"), scannerObj);
    QScriptValue applyFunction = engine->newFunction(&js_apply, this);
    scannerObj.setProperty(QLatin1String("apply"), applyFunction);
}

QtMocScanner::~QtMocScanner()
{
    m_targetScriptValue.setProperty(QLatin1String("QtMocScanner"), QScriptValue());
    delete m_scanResultCache;
}

static ScanResultCache::Result runScanner(ScannerPlugin *scanner, const Artifact *artifact,
        ScanResultCache *scanResultCache)
{
    const QString &filepath = artifact->filePath();
    ScanResultCache::Result scanResult = scanResultCache->value(scanner, filepath);
    if (!scanResult.valid) {
        scanResult.valid = true;
        void *opaq = scanner->open(filepath.utf16(),
                                   ScanForDependenciesFlag | ScanForFileTagsFlag);
        if (!opaq || !scanner->additionalFileTags)
            return scanResult;

        int length = 0;
        const char **szFileTagsFromScanner = scanner->additionalFileTags(opaq, &length);
        if (szFileTagsFromScanner) {
            for (int i = length; --i >= 0;)
                scanResult.additionalFileTags += szFileTagsFromScanner[i];
        }

        QString baseDirOfInFilePath = artifact->dirPath();
        forever {
            int flags = 0;
            const char *szOutFilePath = scanner->next(opaq, &length, &flags);
            if (szOutFilePath == 0)
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
            scanResult.deps += ScanResultCache::Dependency(includedFilePath);
        }

        scanner->close(opaq);
        scanResultCache->insert(scanner, filepath, scanResult);
    }
    return scanResult;
}

void QtMocScanner::findIncludedMocCppFiles()
{
    if (!m_includedMocCppFiles.isEmpty())
        return;

    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[QtMocScanner] looking for included moc_XXX.cpp files";

    foreach (Artifact *artifact, m_product->lookupArtifactsByFileTag("cpp")) {
        const ScanResultCache::Result scanResult
                = runScanner(m_cppScanner, artifact, m_scanResultCache);
        foreach (const ScanResultCache::Dependency &dependency, scanResult.deps) {
            QString includedFileName = dependency.fileName();
            if (includedFileName.startsWith(QLatin1String("moc_"))
                    && includedFileName.endsWith(QLatin1String(".cpp"))) {
                if (m_logger.traceEnabled())
                    m_logger.qbsTrace() << "[QtMocScanner] " << artifact->fileName()
                                        << " includes " << includedFileName;
                includedFileName.remove(0, 4);
                includedFileName.chop(4);
                m_includedMocCppFiles.insert(includedFileName, artifact->fileName());
            }
        }
    }
}

QScriptValue QtMocScanner::js_apply(QScriptContext *ctx, QScriptEngine *engine, void *data)
{
    QtMocScanner *that = reinterpret_cast<QtMocScanner *>(data);
    QScriptValue input = ctx->argument(0);
    return that->apply(engine, attachedPointer<Artifact>(input));
}

static QScriptValue scannerCountError(QScriptEngine *engine, int scannerCount,
        const QString &fileTag)
{
    return engine->currentContext()->throwError(
                Tr::tr("There are %1 scanners for the file tag %2. "
                       "Expected is exactly one.").arg(scannerCount).arg(fileTag));
}

QScriptValue QtMocScanner::apply(QScriptEngine *engine, const Artifact *artifact)
{
    if (!m_cppScanner) {
        QList<ScannerPlugin *> scanners = ScannerPluginManager::scannersForFileTag("cpp");
        if (scanners.count() != 1)
            return scannerCountError(engine, scanners.count(), QLatin1String("cpp"));
        m_cppScanner = scanners.first();
        scanners = ScannerPluginManager::scannersForFileTag("hpp");
        if (scanners.count() != 1)
            return scannerCountError(engine, scanners.count(), QLatin1String("hpp"));
        m_hppScanner = scanners.first();
    }

    findIncludedMocCppFiles();

    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[QtMocScanner] scanning " << artifact->toString();

    bool hasQObjectMacro = false;
    bool mustCompile = false;
    bool hasPluginMetaDataMacro = false;
    const bool isHeaderFile = artifact->fileTags().contains("hpp");

    ScannerPlugin * const scanner = isHeaderFile ? m_hppScanner : m_cppScanner;
    const ScanResultCache::Result scanResult = runScanner(scanner, artifact, m_scanResultCache);
    if (!scanResult.additionalFileTags.isEmpty()) {
        if (isHeaderFile) {
            if (scanResult.additionalFileTags.contains("moc_hpp"))
                hasQObjectMacro = true;
            if (scanResult.additionalFileTags.contains("moc_hpp_plugin")) {
                hasQObjectMacro = true;
                hasPluginMetaDataMacro = true;
            }
            if (!m_includedMocCppFiles.contains(FileInfo::completeBaseName(artifact->fileName())))
                mustCompile = true;
        } else {
            if (scanResult.additionalFileTags.contains("moc_cpp"))
                hasQObjectMacro = true;
        }
    }

    if (m_logger.traceEnabled()) {
        m_logger.qbsTrace() << "[QtMocScanner] hasQObjectMacro: " << hasQObjectMacro
                            << " mustCompile: " << mustCompile
                            << " hasPluginMetaDataMacro: " << hasPluginMetaDataMacro;
    }

    QScriptValue obj = engine->newObject();
    obj.setProperty(QLatin1String("hasQObjectMacro"), hasQObjectMacro);
    obj.setProperty(QLatin1String("mustCompile"), mustCompile);
    obj.setProperty(QLatin1String("hasPluginMetaDataMacro"), hasPluginMetaDataMacro);
    return obj;
}

} // namespace Internal
} // namespace qbs
