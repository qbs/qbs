/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#include "automoc.h"
#include "scanresultcache.h"
#include <buildgraph/artifact.h>
#include <tools/error.h>
#include <tools/logger.h>
#include <tools/scannerpluginmanager.h>

namespace qbs {

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

    QList<QPair<Artifact *, FileType> > artifactsToMoc;
    QSet<QString> includedMocCppFiles;
    QHash<QString, Artifact *>::const_iterator it = product->artifacts.begin();
    for (; it != product->artifacts.end(); ++it) {
        Artifact *artifact = it.value();
        if (artifact->artifactType != Artifact::SourceFile)
            continue;
        FileType fileType = UnknownFileType;
        if (artifact->fileTags.contains("hpp"))
            fileType = HppFileType;
        if (artifact->fileTags.contains("cpp"))
            fileType = CppFileType;
        if (fileType == UnknownFileType)
            continue;
        QString mocFileTag;
        bool alreadyMocced = isVictimOfMoc(artifact, fileType, mocFileTag);
        bool hasQObjectMacro;
        apply(artifact, hasQObjectMacro, includedMocCppFiles);
        if (hasQObjectMacro && !alreadyMocced) {
            artifactsToMoc += qMakePair(artifact, fileType);
        } else if (!hasQObjectMacro && alreadyMocced) {
            unmoc(artifact, mocFileTag);
        }
    }

    QMap<QString, QSet<Artifact *> > artifactsPerFileTag;
    for (int i = artifactsToMoc.count(); --i >= 0;) {
        const QPair<Artifact *, FileType> &p = artifactsToMoc.at(i);
        Artifact * const artifact = p.first;
        FileType fileType = p.second;
        QString fileTag;
        if (fileType == CppFileType) {
            fileTag = "moc_cpp";
        } else if (fileType == HppFileType) {
            QString mocFileName = generateMocFileName(artifact, fileType);
            if (includedMocCppFiles.contains(mocFileName))
                fileTag = "moc_hpp_inc";
            else
                fileTag = "moc_hpp";
        }
        artifactsPerFileTag[fileTag].insert(artifact);
    }

    BuildGraph *buildGraph = product->project->buildGraph();
    if (!artifactsPerFileTag.isEmpty()) {
        qbsInfo() << DontPrintLogLevel << "Applying moc rules for '" << product->rProduct->name << "'.";
        buildGraph->applyRules(product.data(), artifactsPerFileTag);
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
        mocFileName = "moc_" + FileInfo::baseName(artifact->fileName) + ".cpp";
        break;
    case CppFileType:
        mocFileName = FileInfo::baseName(artifact->fileName) + ".moc";
        break;
    }
    return mocFileName;
}

void AutoMoc::apply(Artifact *artifact, bool &hasQObjectMacro, QSet<QString> &includedMocCppFiles)
{
    if (qbsLogLevel(LoggerTrace))
        qbsTrace() << "[AUTOMOC] checks " << fileName(artifact);

    hasQObjectMacro = false;
    const int numFileTags = artifact->fileTags.count();
    char **cFileTags = createCFileTags(artifact->fileTags);

    foreach (ScannerPlugin *scanner, scanners()) {
        void *opaq = scanner->open(artifact->fileName.utf16(), cFileTags, numFileTags);
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
            scanResult = m_scanResultCache->value(artifact->fileName);
        if (!scanResult.visited) {
            scanResult.visited = true;
            opaq = scanner->open(artifact->fileName.utf16(), 0, 0);
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
                m_scanResultCache->insert(artifact->fileName, scanResult);
        }

        foreach (const ScanResultCache::Dependency &dependency, scanResult.deps) {
            const QString &includedFilePath = dependency.first;
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
    foundMocFileTag.clear();
    switch (fileType) {
    case UnknownFileType:
        break;
    case HppFileType:
        if (artifact->fileTags.contains("moc_hpp"))
            foundMocFileTag = "moc_hpp";
        else if (artifact->fileTags.contains("moc_hpp_inc"))
            foundMocFileTag = "moc_hpp_inc";
        break;
    case CppFileType:
        if (artifact->fileTags.contains("moc_cpp"))
            foundMocFileTag = "moc_cpp";
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

} // namespace qbs
