/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "automoc.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "buildgraph.h"
#include "rulesapplicator.h"
#include "scanresultcache.h"
#include <buildgraph/artifact.h>
#include <buildgraph/transformer.h>
#include <language/language.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/scannerpluginmanager.h>

namespace qbs {
namespace Internal {

static char **createCFileTags(const FileTags &fileTags)
{
    if (fileTags.isEmpty())
        return 0;

    char **buf = new char*[fileTags.count()];
    size_t i = 0;
    foreach (const FileTag &fileTag, fileTags) {
        buf[i] = qstrdup(fileTag.toString().toLocal8Bit().data());
        ++i;
    }
    return buf;
}

static void freeCFileTags(char **cFileTags, int numFileTags)
{
    if (!cFileTags)
        return;
    for (int i = numFileTags; --i >= 0;)
        delete[] cFileTags[i];
    delete[] cFileTags;
}

AutoMoc::AutoMoc(const Logger &logger, QObject *parent)
    : QObject(parent)
    , m_scanResultCache(0)
    , m_logger(logger)
{
}

void AutoMoc::setScanResultCache(ScanResultCache *scanResultCache)
{
    m_scanResultCache = scanResultCache;
}

void AutoMoc::apply(const ResolvedProductPtr &product)
{
    if (scanners().isEmpty())
        throw ErrorInfo("C++ scanner cannot be loaded.");

    Artifact *pluginMetaDataFile = 0;
    Artifact *pchFile = 0;
    QList<QPair<Artifact *, FileType> > artifactsToMoc;
    QSet<QString> includedMocCppFiles;
    const FileTime currentTime = FileTime::currentTime();
    ArtifactList::const_iterator it = product->buildData->artifacts.begin();
    for (; it != product->buildData->artifacts.end(); ++it) {
        Artifact *artifact = *it;
        if (!pchFile || !pluginMetaDataFile) {
            foreach (const FileTag &fileTag, artifact->fileTags) {
                if (fileTag == "c++_pch")
                    pchFile = artifact;
                else if (fileTag == "qt_plugin_metadata")
                    pluginMetaDataFile = artifact;
            }
        }

        if (!pluginMetaDataFile && artifact->fileTags.contains("qt_plugin_metadata")) {
            if (m_logger.debugEnabled()) {
                m_logger.qbsDebug() << "[AUTOMOC] found Qt plugin metadata file "
                                    << artifact->filePath();
            }
            pluginMetaDataFile = artifact;
        }
        if (artifact->artifactType != Artifact::SourceFile)
            continue;
        if (artifact->timestamp() < artifact->autoMocTimestamp)
            continue;
        artifact->autoMocTimestamp = currentTime;
        const FileType fileType = AutoMoc::fileType(artifact);
        if (fileType == UnknownFileType)
            continue;
        FileTag mocFileTag;
        bool alreadyMocced = isVictimOfMoc(artifact, fileType, mocFileTag);
        bool hasQObjectMacro;
        scan(artifact, hasQObjectMacro, includedMocCppFiles);
        if (hasQObjectMacro && !alreadyMocced) {
            artifactsToMoc += qMakePair(artifact, fileType);
        } else if (!hasQObjectMacro && alreadyMocced) {
            unmoc(artifact, mocFileTag);
        }
    }

    Artifact *pluginHeaderFile = 0;
    ArtifactsPerFileTagMap artifactsPerFileTag;
    for (int i = artifactsToMoc.count(); --i >= 0;) {
        const QPair<Artifact *, FileType> &p = artifactsToMoc.at(i);
        Artifact * const artifact = p.first;
        FileType fileType = p.second;
        foreach (const FileTag &fileTag, artifact->fileTags) {
            if (fileTag == "moc_hpp") {
                const QString mocFileName = generateMocFileName(artifact, fileType);
                if (includedMocCppFiles.contains(mocFileName)) {
                    FileTag newFileTag = "moc_hpp_inc";
                    artifact->fileTags -= fileTag;
                    artifact->fileTags += newFileTag;
                    artifactsPerFileTag[newFileTag].insert(artifact);
                    continue;
                }
            } else if (fileTag == "moc_plugin_hpp") {
                if (m_logger.debugEnabled()) {
                    m_logger.qbsDebug() << "[AUTOMOC] found Qt plugin header file "
                                        << artifact->filePath();
                }
                FileTag newFileTag = "moc_hpp";
                artifact->fileTags -= fileTag;
                artifact->fileTags += newFileTag;
                artifactsPerFileTag[newFileTag].insert(artifact);
                pluginHeaderFile = artifact;
            }
            artifactsPerFileTag[fileTag].insert(artifact);
        }
    }

    if (pchFile)
        artifactsPerFileTag["c++_pch"] += pchFile;
    if (!artifactsPerFileTag.isEmpty()) {
        emit reportCommandDescription(QLatin1String("automoc"),
                                      Tr::tr("Applying moc rules for '%1'.")
                                      .arg(product->name));
        RulesApplicator(product, artifactsPerFileTag, m_logger).applyAllRules();
    }
    if (pluginHeaderFile && pluginMetaDataFile) {
        // Make every artifact that is dependent of the header file also
        // dependent of the plugin metadata file.
        foreach (Artifact *outputOfHeader, pluginHeaderFile->parents)
            loggedConnect(outputOfHeader, pluginMetaDataFile, m_logger);
    }

    product->topLevelProject()->buildData->updateNodesThatMustGetNewTransformer(m_logger);
}

QString AutoMoc::generateMocFileName(Artifact *artifact, FileType fileType)
{
    QString mocFileName;
    switch (fileType) {
    case UnknownFileType:
        break;
    case HppFileType:
        mocFileName = "moc_" + FileInfo::baseName(artifact->filePath()) + ".cpp";
        break;
    case CppFileType:
        mocFileName = FileInfo::baseName(artifact->filePath()) + ".moc";
        break;
    }
    return mocFileName;
}

AutoMoc::FileType AutoMoc::fileType(Artifact *artifact)
{
    foreach (const FileTag &fileTag, artifact->fileTags)
        if (fileTag == "hpp")
            return HppFileType;
        else if (fileTag == "cpp")
            return CppFileType;
    return UnknownFileType;
}

void AutoMoc::scan(Artifact *artifact, bool &hasQObjectMacro, QSet<QString> &includedMocCppFiles)
{
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[AUTOMOC] checks " << relativeArtifactFileName(artifact);

    hasQObjectMacro = false;
    const int numFileTags = artifact->fileTags.count();
    char **cFileTags = createCFileTags(artifact->fileTags);

    foreach (ScannerPlugin *scanner, scanners()) {
        void *opaq = scanner->open(artifact->filePath().utf16(), cFileTags, numFileTags);
        if (!opaq || !scanner->additionalFileTags)
            continue;

        // HACK: misuse the file dependency scanner as provider for file tags
        int length = 0;
        const char **szFileTagsFromScanner = scanner->additionalFileTags(opaq, &length);
        if (szFileTagsFromScanner && length > 0) {
            for (int i=length; --i >= 0;) {
                artifact->fileTags.insert(szFileTagsFromScanner[i]);
                if (m_logger.traceEnabled())
                    m_logger.qbsTrace() << "[AUTOMOC] finds Q_OBJECT macro";
                const QByteArray fileTagFromScanner
                        = QByteArray::fromRawData(szFileTagsFromScanner[i],
                                                  qstrlen(szFileTagsFromScanner[i]));
                if (fileTagFromScanner.startsWith("moc"))
                    hasQObjectMacro = true;
            }
        }

        scanner->close(opaq);

        ScanResultCache::Result scanResult;
        if (m_scanResultCache)
            scanResult = m_scanResultCache->value(artifact->filePath());
        if (!scanResult.valid) {
            scanResult.valid = true;
            opaq = scanner->open(artifact->filePath().utf16(), 0, 0);
            if (!opaq)
                continue;

            forever {
                int flags = 0;
                const char *szOutFilePath = scanner->next(opaq, &length, &flags);
                if (szOutFilePath == 0)
                    break;
                QString includedFilePath = QString::fromLocal8Bit(szOutFilePath, length);
                if (includedFilePath.isEmpty())
                    continue;
                bool isLocalInclude = (flags & SC_LOCAL_INCLUDE_FLAG);
                scanResult.deps += ScanResultCache::Dependency(includedFilePath, isLocalInclude);
            }

            scanner->close(opaq);
            if (m_scanResultCache)
                m_scanResultCache->insert(artifact->filePath(), scanResult);
        }

        foreach (const ScanResultCache::Dependency &dependency, scanResult.deps) {
            const QString &includedFilePath = dependency.filePath();
            if (includedFilePath.startsWith("moc_") && includedFilePath.endsWith(".cpp")) {
                if (m_logger.traceEnabled())
                    m_logger.qbsTrace() << "[AUTOMOC] finds included file: " << includedFilePath;
                includedMocCppFiles += includedFilePath;
            }
        }
    }

    freeCFileTags(cFileTags, numFileTags);
}

static FileTags provideMocHeaderFileTags()
{
    FileTags fileTags;
    fileTags << "moc_hpp" << "moc_hpp_inc" << "moc_plugin_hpp";
    return fileTags;
}

bool AutoMoc::isVictimOfMoc(Artifact *artifact, FileType fileType, FileTag &foundMocFileTag)
{
    static const FileTags mocHeaderFileTags = provideMocHeaderFileTags();
    static const FileTag mocCppFileTag = "moc_cpp";
    foundMocFileTag.clear();
    switch (fileType) {
    case UnknownFileType:
        break;
    case HppFileType:
        foreach (const FileTag &fileTag, artifact->fileTags) {
            if (mocHeaderFileTags.contains(fileTag)) {
                foundMocFileTag = fileTag;
                break;
            }
        }
        break;
    case CppFileType:
        if (artifact->fileTags.contains(mocCppFileTag))
            foundMocFileTag = mocCppFileTag;
        break;
    }
    return foundMocFileTag.isValid();
}

void AutoMoc::unmoc(Artifact *artifact, const FileTag &mocFileTag)
{
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[AUTOMOC] unmoc'ing " << relativeArtifactFileName(artifact);

    artifact->fileTags.remove(mocFileTag);

    Artifact *generatedMocArtifact = 0;
    foreach (Artifact *parent, artifact->parents) {
        foreach (const FileTag &fileTag, parent->fileTags) {
            if (fileTag == "hpp" || fileTag == "cpp") {
                generatedMocArtifact = parent;
                break;
            }
        }
    }

    if (!generatedMocArtifact) {
        m_logger.qbsTrace() << "[AUTOMOC] generated moc artifact could not be found";
        return;
    }

    TopLevelProject * const project = artifact->product->topLevelProject();
    if (mocFileTag == "moc_hpp") {
        Artifact *mocObjArtifact = 0;
        foreach (Artifact *parent, generatedMocArtifact->parents) {
            foreach (const FileTag &fileTag, parent->fileTags) {
                if (fileTag == "obj" || fileTag == "fpicobj") {
                    mocObjArtifact = parent;
                    break;
                }
            }
        }

        if (!mocObjArtifact) {
            m_logger.qbsTrace() << "[AUTOMOC] generated moc obj artifact could not be found";
        } else {
            if (m_logger.traceEnabled()) {
                m_logger.qbsTrace() << "[AUTOMOC] removing moc obj artifact "
                                    << relativeArtifactFileName(mocObjArtifact);
            }
            project->buildData->removeArtifact(mocObjArtifact, m_logger);
        }
    }

    if (m_logger.traceEnabled()) {
        m_logger.qbsTrace() << "[AUTOMOC] removing generated artifact "
                            << relativeArtifactFileName(generatedMocArtifact);
    }
    project->buildData->removeArtifact(generatedMocArtifact, m_logger);
    delete generatedMocArtifact;
}

QList<ScannerPlugin *> AutoMoc::scanners() const
{
    if (m_scanners.isEmpty())
        m_scanners = ScannerPluginManager::scannersForFileTag("hpp");

    return m_scanners;
}

} // namespace Internal
} // namespace qbs
