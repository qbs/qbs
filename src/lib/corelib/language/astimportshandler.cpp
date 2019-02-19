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
#include "astimportshandler.h"

#include "asttools.h"
#include "builtindeclarations.h"
#include "filecontext.h"
#include "itemreadervisitorstate.h"
#include "jsextensions/jsextensions.h"

#include <logging/logger.h>
#include <logging/translator.h>
#include <parser/qmljsast_p.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/qttools.h>
#include <tools/stringconstants.h>
#include <tools/version.h>

#include <QtCore/qdiriterator.h>

namespace qbs {
namespace Internal {

ASTImportsHandler::ASTImportsHandler(ItemReaderVisitorState &visitorState, Logger &logger,
                                     const FileContextPtr &file)
    : m_visitorState(visitorState)
    , m_logger(logger)
    , m_file(file)
    , m_directory(FileInfo::path(m_file->filePath()))
{
}

void ASTImportsHandler::handleImports(const QbsQmlJS::AST::UiImportList *uiImportList)
{
    const auto searchPaths = m_file->searchPaths();
    for (const QString &searchPath : searchPaths)
        collectPrototypes(searchPath + QStringLiteral("/imports"), QString());

    // files in the same directory are available as prototypes
    collectPrototypes(m_directory, QString());

    bool baseImported = false;
    for (const auto *it = uiImportList; it; it = it->next)
        handleImport(it->import, &baseImported);
    if (!baseImported) {
        QStringRef qbsref(&StringConstants::qbsModule());
        QbsQmlJS::AST::UiQualifiedId qbsURI(qbsref);
        qbsURI.finish();
        QbsQmlJS::AST::UiImport imp(&qbsURI);
        handleImport(&imp, &baseImported);
    }

    for (auto it = m_jsImports.constBegin(); it != m_jsImports.constEnd(); ++it)
        m_file->addJsImport(it.value());
}

void ASTImportsHandler::handleImport(const QbsQmlJS::AST::UiImport *import, bool *baseImported)
{
    QStringList importUri;
    bool isBase = false;
    if (import->importUri) {
        importUri = toStringList(import->importUri);
        isBase = (importUri.size() == 1 && importUri.front() == StringConstants::qbsModule())
                || (importUri.size() == 2 && importUri.front() == StringConstants::qbsModule()
                    && importUri.last() == StringConstants::baseVar());
        if (isBase) {
            *baseImported = true;
            checkImportVersion(import->versionToken);
        } else if (import->versionToken.length) {
            m_logger.printWarning(ErrorInfo(Tr::tr("Superfluous version specification."),
                    toCodeLocation(m_file->filePath(), import->versionToken)));
        }
    }

    QString as;
    if (isBase) {
        if (Q_UNLIKELY(!import->importId.isNull())) {
            throw ErrorInfo(Tr::tr("Import of qbs.base must have no 'as <Name>'"),
                        toCodeLocation(m_file->filePath(), import->importIdToken));
        }
    } else {
        if (importUri.size() == 2 && importUri.front() == StringConstants::qbsModule()) {
            const QString extensionName = importUri.last();
            if (JsExtensions::hasExtension(extensionName)) {
                if (Q_UNLIKELY(!import->importId.isNull())) {
                    throw ErrorInfo(Tr::tr("Import of built-in extension '%1' "
                                           "must not have 'as' specifier.").arg(extensionName),
                                    toCodeLocation(m_file->filePath(), import->asToken));
                }
                if (Q_UNLIKELY(m_file->jsExtensions().contains(extensionName))) {
                    m_logger.printWarning(ErrorInfo(Tr::tr("Built-in extension '%1' already "
                                                           "imported.").arg(extensionName),
                                                    toCodeLocation(m_file->filePath(),
                                                                   import->importToken)));
                } else {
                    m_file->addJsExtension(extensionName);
                }
                return;
            }
        }

        if (import->importId.isNull()) {
            if (!import->fileName.isNull()) {
                throw ErrorInfo(Tr::tr("File imports require 'as <Name>'"),
                                toCodeLocation(m_file->filePath(), import->importToken));
            }
            if (importUri.empty()) {
                throw ErrorInfo(Tr::tr("Invalid import URI."),
                                toCodeLocation(m_file->filePath(), import->importToken));
            }
            as = importUri.last();
        } else {
            as = import->importId.toString();
        }

        if (Q_UNLIKELY(JsExtensions::hasExtension(as)))
            throw ErrorInfo(Tr::tr("Cannot reuse the name of built-in extension '%1'.").arg(as),
                            toCodeLocation(m_file->filePath(), import->importIdToken));
        if (Q_UNLIKELY(!m_importAsNames.insert(as).second)) {
            throw ErrorInfo(Tr::tr("Cannot import into the same name more than once."),
                        toCodeLocation(m_file->filePath(), import->importIdToken));
        }
    }

    if (!import->fileName.isNull()) {
        QString filePath = FileInfo::resolvePath(m_directory, import->fileName.toString());

        QFileInfo fi(filePath);
        if (Q_UNLIKELY(!fi.exists()))
            throw ErrorInfo(Tr::tr("Cannot find imported file %0.")
                            .arg(QDir::toNativeSeparators(filePath)),
                            CodeLocation(m_file->filePath(), import->fileNameToken.startLine,
                                         import->fileNameToken.startColumn));
        filePath = fi.canonicalFilePath();
        if (fi.isDir()) {
            collectPrototypesAndJsCollections(filePath, as,
                    toCodeLocation(m_file->filePath(), import->fileNameToken));
        } else {
            if (filePath.endsWith(QStringLiteral(".js"), Qt::CaseInsensitive)) {
                JsImport &jsImport = m_jsImports[as];
                jsImport.scopeName = as;
                jsImport.filePaths.push_back(filePath);
                jsImport.location
                        = toCodeLocation(m_file->filePath(), import->firstSourceLocation());
            } else if (filePath.endsWith(QStringLiteral(".qbs"), Qt::CaseInsensitive)) {
                m_typeNameToFile.insert(QStringList(as), filePath);
            } else {
                throw ErrorInfo(Tr::tr("Can only import .qbs and .js files"),
                            CodeLocation(m_file->filePath(), import->fileNameToken.startLine,
                                         import->fileNameToken.startColumn));
            }
        }
    } else if (!importUri.empty()) {
        const QString importPath = isBase
                ? QStringLiteral("qbs/base") : importUri.join(QDir::separator());
        bool found = m_typeNameToFile.contains(importUri);
        if (!found) {
            const auto searchPaths = m_file->searchPaths();
            for (const QString &searchPath : searchPaths) {
                const QFileInfo fi(FileInfo::resolvePath(
                                       FileInfo::resolvePath(searchPath,
                                                             StringConstants::importsDir()),
                                       importPath));
                if (fi.isDir()) {
                    // ### versioning, qbsdir file, etc.
                    const QString &resultPath = fi.absoluteFilePath();
                    collectPrototypesAndJsCollections(resultPath, as,
                            toCodeLocation(m_file->filePath(), import->fileNameToken));
                    found = true;
                    break;
                }
            }
        }
        if (Q_UNLIKELY(!found)) {
            throw ErrorInfo(Tr::tr("import %1 not found")
                            .arg(importUri.join(QLatin1Char('.'))),
                            toCodeLocation(m_file->filePath(), import->fileNameToken));
        }
    }
}

Version ASTImportsHandler::readImportVersion(const QString &str, const CodeLocation &location)
{
    const Version v = Version::fromString(str);
    if (Q_UNLIKELY(!v.isValid()))
        throw ErrorInfo(Tr::tr("Cannot parse version number in import statement."), location);
    if (Q_UNLIKELY(v.patchLevel() != 0)) {
        throw ErrorInfo(Tr::tr("Version number in import statement cannot have more than "
                               "two components."), location);
    }
    return v;
}

bool ASTImportsHandler::addPrototype(const QString &fileName, const QString &filePath,
                                     const QString &as, bool needsCheck)
{
    if (needsCheck && fileName.size() <= 4)
        return false;

    const QString componentName = fileName.left(fileName.size() - 4);
    // ### validate componentName

    if (needsCheck && !componentName.at(0).isUpper())
        return false;

    QStringList prototypeName;
    if (!as.isEmpty())
        prototypeName.push_back(as);
    prototypeName.push_back(componentName);
    if (!m_typeNameToFile.contains(prototypeName))
        m_typeNameToFile.insert(prototypeName, filePath);
    return true;
}

void ASTImportsHandler::checkImportVersion(const QbsQmlJS::AST::SourceLocation &versionToken) const
{
    if (!versionToken.length)
        return;
    const QString importVersionString
            = m_file->content().mid(versionToken.offset, versionToken.length);
    const Version importVersion = readImportVersion(importVersionString,
                                                    toCodeLocation(m_file->filePath(), versionToken));
    if (Q_UNLIKELY(importVersion != BuiltinDeclarations::instance().languageVersion()))
        throw ErrorInfo(Tr::tr("Incompatible qbs language version %1. This is version %2.").arg(
                            importVersionString,
                            BuiltinDeclarations::instance().languageVersion().toString()),
                        toCodeLocation(m_file->filePath(), versionToken));

}

void ASTImportsHandler::collectPrototypes(const QString &path, const QString &as)
{
    QStringList fileNames; // Yes, file *names*.
    if (m_visitorState.findDirectoryEntries(path, &fileNames)) {
        for (const QString &fileName : qAsConst(fileNames))
            addPrototype(fileName, path + QLatin1Char('/') + fileName, as, false);
        return;
    }

    QDirIterator dirIter(path, StringConstants::qbsFileWildcards());
    while (dirIter.hasNext()) {
        const QString filePath = dirIter.next();
        const QString fileName = dirIter.fileName();
        if (addPrototype(fileName, filePath, as, true))
            fileNames << fileName;
    }
    m_visitorState.cacheDirectoryEntries(path, fileNames);

}

void ASTImportsHandler::collectPrototypesAndJsCollections(const QString &path, const QString &as,
        const CodeLocation &location)
{
    collectPrototypes(path, as);
    QDirIterator dirIter(path, StringConstants::jsFileWildcards());
    while (dirIter.hasNext()) {
        dirIter.next();
        JsImport &jsImport = m_jsImports[as];
        if (jsImport.scopeName.isNull()) {
            jsImport.scopeName = as;
            jsImport.location = location;
        }
        jsImport.filePaths.push_back(dirIter.filePath());
    }
}

} // namespace Internal
} // namespace qbs
