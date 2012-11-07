/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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
#include "scanresultcache.h"
#include <buildgraph/artifact.h>
#include <buildgraph/transformer.h>
#include <logging/logger.h>
#include <tools/error.h>
#include <tools/scannerpluginmanager.h>

namespace qbs {
namespace Internal {

AutoMoc::AutoMoc()
    : m_scanResultCache(0)
{
}

void AutoMoc::setScanResultCache(ScanResultCache *scanResultCache)
{
    m_scanResultCache = scanResultCache;
}

void AutoMoc::apply(BuildProduct::Ptr product)
{
    if (scanners().isEmpty())
        throw Error("C++ scanner cannot be loaded.");

    Artifact *pluginMetaDataFile = 0;
    Artifact *pchFile = 0;
    QList<QPair<Artifact *, FileType> > artifactsToMoc;
    QSet<QString> includedMocCppFiles;
    ArtifactList::const_iterator it = product->artifacts.begin();
    for (; it != product->artifacts.end(); ++it) {
        Artifact *artifact = *it;
        if (!pchFile || !pluginMetaDataFile) {
            foreach (const QString &fileTag, artifact->fileTags) {
                if (fileTag == QLatin1String("c++_pch"))
                    pchFile = artifact;
                else if (fileTag == QLatin1String("qt_plugin_metadata"))
                    pluginMetaDataFile = artifact;
            }
        }

        if (!pluginMetaDataFile && artifact->fileTags.contains(QLatin1String("qt_plugin_metadata"))) {
            if (qbsLogLevel(LoggerDebug))
                qbsDebug() << "[AUTOMOC] found Qt plugin metadata file " << artifact->filePath();
            pluginMetaDataFile = artifact;
        }
        if (artifact->artifactType != Artifact::SourceFile)
            continue;
        const FileType fileType = AutoMoc::fileType(artifact);
        if (fileType == UnknownFileType)
            continue;
        QString mocFileTag;
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
        foreach (const QString &fileTag, artifact->fileTags) {
            if (fileTag == QLatin1String("moc_hpp")) {
                const QString mocFileName = generateMocFileName(artifact, fileType);
                if (includedMocCppFiles.contains(mocFileName)) {
                    QString newFileTag = QLatin1String("moc_hpp_inc");
                    artifact->fileTags -= fileTag;
                    artifact->fileTags += newFileTag;
                    artifactsPerFileTag[newFileTag].insert(artifact);
                    continue;
                }
            } else if (fileTag == QLatin1String("moc_plugin_hpp")) {
                if (qbsLogLevel(LoggerDebug))
                    qbsDebug() << "[AUTOMOC] found Qt plugin header file " << artifact->filePath();
                QString newFileTag = QLatin1String("moc_hpp");
                artifact->fileTags -= fileTag;
                artifact->fileTags += newFileTag;
                artifactsPerFileTag[newFileTag].insert(artifact);
                pluginHeaderFile = artifact;
            }
            artifactsPerFileTag[fileTag].insert(artifact);
        }
    }

    if (pchFile)
        artifactsPerFileTag[QLatin1String("c++_pch")] += pchFile;
    BuildGraph *buildGraph = product->project->buildGraph();
    if (!artifactsPerFileTag.isEmpty()) {
        qbsInfo() << DontPrintLogLevel << "Applying moc rules for '" << product->rProduct->name << "'.";
        buildGraph->applyRules(product.data(), artifactsPerFileTag);
    }
    if (pluginHeaderFile && pluginMetaDataFile) {
        // Make every artifact that is dependent of the header file also
        // dependent of the plugin metadata file.
        foreach (Artifact *outputOfHeader, pluginHeaderFile->parents)
            buildGraph->loggedConnect(outputOfHeader, pluginMetaDataFile);
    }

    buildGraph->updateNodesThatMustGetNewTransformer();
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
    foreach (const QString &fileTag, artifact->fileTags)
        if (fileTag == QLatin1String("hpp"))
            return HppFileType;
        else if (fileTag == QLatin1String("cpp"))
            return CppFileType;
    return UnknownFileType;
}

void AutoMoc::scan(Artifact *artifact, bool &hasQObjectMacro, QSet<QString> &includedMocCppFiles)
{
    if (qbsLogLevel(LoggerTrace))
        qbsTrace() << "[AUTOMOC] checks " << fileName(artifact);

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
                const QString fileTagFromScanner = QString::fromLocal8Bit(szFileTagsFromScanner[i]);
                artifact->fileTags.insert(fileTagFromScanner);
                if (qbsLogLevel(LoggerTrace))
                    qbsTrace() << "[AUTOMOC] finds Q_OBJECT macro";
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
                if (qbsLogLevel(LoggerTrace))
                    qbsTrace() << "[AUTOMOC] finds included file: " << includedFilePath;
                includedMocCppFiles += includedFilePath;
            }
        }
    }

    freeCFileTags(cFileTags, numFileTags);
}

bool AutoMoc::isVictimOfMoc(Artifact *artifact, FileType fileType, QString &foundMocFileTag)
{
    static const QSet<QString> mocHeaderFileTags = QSet<QString>() << QLatin1String("moc_hpp")
                                                                   << QLatin1String("moc_hpp_inc")
                                                                   << QLatin1String("moc_plugin_hpp");
    static const QString mocCppFileTag = QLatin1String("moc_cpp");
    foundMocFileTag.clear();
    switch (fileType) {
    case UnknownFileType:
        break;
    case HppFileType:
        foreach (const QString &fileTag, artifact->fileTags) {
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
    return !foundMocFileTag.isEmpty();
}

void AutoMoc::unmoc(Artifact *artifact, const QString &mocFileTag)
{
    if (qbsLogLevel(LoggerTrace))
        qbsTrace() << "[AUTOMOC] unmoc'ing " << fileName(artifact);

    artifact->fileTags.remove(mocFileTag);

    Artifact *generatedMocArtifact = 0;
    foreach (Artifact *parent, artifact->parents) {
        foreach (const QString &fileTag, parent->fileTags) {
            if (fileTag == "hpp" || fileTag == "cpp") {
                generatedMocArtifact = parent;
                break;
            }
        }
    }

    if (!generatedMocArtifact) {
        qbsTrace() << "[AUTOMOC] generated moc artifact could not be found";
        return;
    }

    BuildGraph *buildGraph = artifact->project->buildGraph();
    if (mocFileTag == "moc_hpp") {
        Artifact *mocObjArtifact = 0;
        foreach (Artifact *parent, generatedMocArtifact->parents) {
            foreach (const QString &fileTag, parent->fileTags) {
                if (fileTag == "obj" || fileTag == "fpicobj") {
                    mocObjArtifact = parent;
                    break;
                }
            }
        }

        if (!mocObjArtifact) {
            qbsTrace() << "[AUTOMOC] generated moc obj artifact could not be found";
        } else {
            if (qbsLogLevel(LoggerTrace))
                qbsTrace() << "[AUTOMOC] removing moc obj artifact " << fileName(mocObjArtifact);
            buildGraph->remove(mocObjArtifact);
        }
    }

    if (qbsLogLevel(LoggerTrace))
        qbsTrace() << "[AUTOMOC] removing generated artifact " << fileName(generatedMocArtifact);
    buildGraph->remove(generatedMocArtifact);
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
