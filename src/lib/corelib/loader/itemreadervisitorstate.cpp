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
#include "itemreadervisitorstate.h"

#include "itemreaderastvisitor.h"
#include "loaderutils.h"

#include <language/asttools.h>
#include <language/filecontext.h>
#include <logging/translator.h>
#include <parser/qmljsengine_p.h>
#include <parser/qmljslexer_p.h>
#include <parser/qmljsparser_p.h>
#include <tools/error.h>
#include <tools/stringconstants.h>

#include <QtCore/qdiriterator.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qtextstream.h>

namespace qbs {
namespace Internal {

ItemReaderVisitorState::ItemReaderVisitorState(ItemReaderCache &cache, Logger &logger)
    : m_cache(cache), m_logger(logger)
{
}

Item *ItemReaderVisitorState::readFile(const QString &filePath, const QStringList &searchPaths,
                                  ItemPool *itemPool)
{
    const auto setupCacheEntry = [&](ItemReaderCache::AstCacheEntry &entry) {
        QFile file(filePath);
        if (Q_UNLIKELY(!file.open(QFile::ReadOnly)))
            throw ErrorInfo(Tr::tr("Cannot open '%1'.").arg(filePath));

        QTextStream stream(&file);
        setupDefaultCodec(stream);
        const QString &code = stream.readAll();
        QbsQmlJS::Lexer lexer(&entry.engine);
        lexer.setCode(code, 1);
        QbsQmlJS::Parser parser(&entry.engine);

        file.close();
        if (!parser.parse()) {
            const QList<QbsQmlJS::DiagnosticMessage> &parserMessages = parser.diagnosticMessages();
            if (Q_UNLIKELY(!parserMessages.empty())) {
                ErrorInfo err;
                for (const QbsQmlJS::DiagnosticMessage &msg : parserMessages)
                    err.append(msg.message, toCodeLocation(filePath, msg.loc));
                throw err;
            }
        }

        entry.code = code;
        entry.ast = parser.ast();
    };

    ItemReaderCache::AstCacheEntry &cacheEntry = m_cache.retrieveOrSetupCacheEntry(
        filePath, setupCacheEntry);
    const FileContextPtr file = FileContext::create();
    file->setFilePath(QFileInfo(filePath).absoluteFilePath());
    file->setContent(cacheEntry.code);
    file->setSearchPaths(searchPaths);

    ItemReaderASTVisitor astVisitor(*this, file, itemPool, m_logger);
    {
        class ProcessingFlagManager {
        public:
            ProcessingFlagManager(ItemReaderCache::AstCacheEntry &e, const QString &filePath)
                : m_cacheEntry(e)
            {
                if (!e.addProcessingThread())
                    throw ErrorInfo(Tr::tr("Loop detected when importing '%1'.").arg(filePath));
            }
            ~ProcessingFlagManager() { m_cacheEntry.removeProcessingThread(); }

        private:
            ItemReaderCache::AstCacheEntry &m_cacheEntry;
        } processingFlagManager(cacheEntry, filePath);
        cacheEntry.ast->accept(&astVisitor);
    }
    astVisitor.checkItemTypes();
    return astVisitor.rootItem();
}

void ItemReaderVisitorState::findDirectoryEntries(const QString &dirPath, QStringList *entries) const
{
    *entries = m_cache.retrieveOrSetDirectoryEntries(dirPath, [&dirPath] {
        QStringList fileNames;
        QDirIterator dirIter(dirPath, StringConstants::qbsFileWildcards());
        while (dirIter.hasNext()) {
            dirIter.next();
            fileNames << dirIter.fileName();
        }
        return fileNames;
    });
}

Item *ItemReaderVisitorState::mostDerivingItem() const
{
    return m_mostDerivingItem;
}

void ItemReaderVisitorState::setMostDerivingItem(Item *item)
{
    m_mostDerivingItem = item;
}


} // namespace Internal
} // namespace qbs
