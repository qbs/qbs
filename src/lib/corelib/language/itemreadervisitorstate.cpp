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

#include "asttools.h"
#include "filecontext.h"
#include "itemreaderastvisitor.h"

#include <logging/translator.h>
#include <parser/qmljsengine_p.h>
#include <parser/qmljslexer_p.h>
#include <parser/qmljsparser_p.h>
#include <tools/error.h>

#include <QtCore/qshareddata.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qtextstream.h>

namespace qbs {
namespace Internal {

class ASTCacheValueData : public QSharedData
{
    Q_DISABLE_COPY(ASTCacheValueData)
public:
    ASTCacheValueData()
        : ast(nullptr)
        , processing(false)
    {
    }

    QString code;
    QbsQmlJS::Engine engine;
    QbsQmlJS::AST::UiProgram *ast;
    bool processing;
};

class ASTCacheValue
{
public:
    ASTCacheValue()
        : d(new ASTCacheValueData)
    {
    }

    ASTCacheValue(const ASTCacheValue &other) = default;

    void setProcessingFlag(bool b) { d->processing = b; }
    bool isProcessing() const { return d->processing; }

    void setCode(const QString &code) { d->code = code; }
    QString code() const { return d->code; }

    QbsQmlJS::Engine *engine() const { return &d->engine; }

    void setAst(QbsQmlJS::AST::UiProgram *ast) { d->ast = ast; }
    QbsQmlJS::AST::UiProgram *ast() const { return d->ast; }
    bool isValid() const { return d->ast; }

private:
    QExplicitlySharedDataPointer<ASTCacheValueData> d;
};

class ItemReaderVisitorState::ASTCache : public QHash<QString, ASTCacheValue> {};


ItemReaderVisitorState::ItemReaderVisitorState(Logger &logger)
    : m_logger(logger)
    , m_astCache(new ASTCache)
{

}

ItemReaderVisitorState::~ItemReaderVisitorState()
{
    delete m_astCache;
}

Item *ItemReaderVisitorState::readFile(const QString &filePath, const QStringList &searchPaths,
                                  ItemPool *itemPool)
{
    ASTCacheValue &cacheValue = (*m_astCache)[filePath];
    if (cacheValue.isValid()) {
        if (Q_UNLIKELY(cacheValue.isProcessing()))
            throw ErrorInfo(Tr::tr("Loop detected when importing '%1'.").arg(filePath));
    } else {
        QFile file(filePath);
        if (Q_UNLIKELY(!file.open(QFile::ReadOnly)))
            throw ErrorInfo(Tr::tr("Cannot open '%1'.").arg(filePath));

        m_filesRead.insert(filePath);
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        const QString &code = stream.readAll();
        QbsQmlJS::Lexer lexer(cacheValue.engine());
        lexer.setCode(code, 1);
        QbsQmlJS::Parser parser(cacheValue.engine());

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

        cacheValue.setCode(code);
        cacheValue.setAst(parser.ast());
    }

    const FileContextPtr file = FileContext::create();
    file->setFilePath(QFileInfo(filePath).absoluteFilePath());
    file->setContent(cacheValue.code());
    file->setSearchPaths(searchPaths);

    ItemReaderASTVisitor astVisitor(*this, file, itemPool, m_logger);
    {
        class ProcessingFlagManager {
        public:
            ProcessingFlagManager(ASTCacheValue &v) : m_cacheValue(v) { v.setProcessingFlag(true); }
            ~ProcessingFlagManager() { m_cacheValue.setProcessingFlag(false); }
        private:
            ASTCacheValue &m_cacheValue;
        } processingFlagManager(cacheValue);
        cacheValue.ast()->accept(&astVisitor);
    }
    astVisitor.checkItemTypes();
    return astVisitor.rootItem();
}

void ItemReaderVisitorState::cacheDirectoryEntries(const QString &dirPath, const QStringList &entries)
{
    m_directoryEntries.insert(dirPath, entries);
}

bool ItemReaderVisitorState::findDirectoryEntries(const QString &dirPath, QStringList *entries) const
{
    const auto it = m_directoryEntries.constFind(dirPath);
    if (it == m_directoryEntries.constEnd())
        return false;
    *entries = it.value();
    return true;
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
