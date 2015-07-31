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
#include "itemreadervisitorstate.h"

#include "asttools.h"
#include "itemreaderastvisitor.h"

#include <logging/translator.h>
#include <parser/qmljsengine_p.h>
#include <parser/qmljslexer_p.h>
#include <parser/qmljsparser_p.h>
#include <tools/error.h>

#include <QExplicitlySharedDataPointer>
#include <QFile>
#include <QFileInfo>
#include <QSharedData>
#include <QTextStream>

namespace qbs {
namespace Internal {

class ASTCacheValueData : public QSharedData
{
    Q_DISABLE_COPY(ASTCacheValueData)
public:
    ASTCacheValueData()
        : ast(0)
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

    ASTCacheValue(const ASTCacheValue &other)
        : d(other.d)
    {
    }

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


ItemReaderVisitorState::ItemReaderVisitorState(Logger logger)
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
        const QString code = QTextStream(&file).readAll();
        QbsQmlJS::Lexer lexer(cacheValue.engine());
        lexer.setCode(code, 1);
        QbsQmlJS::Parser parser(cacheValue.engine());

        file.close();
        if (!parser.parse()) {
            QList<QbsQmlJS::DiagnosticMessage> parserMessages = parser.diagnosticMessages();
            if (Q_UNLIKELY(!parserMessages.isEmpty())) {
                ErrorInfo err;
                foreach (const QbsQmlJS::DiagnosticMessage &msg, parserMessages)
                    err.append(msg.message, toCodeLocation(filePath, msg.loc));
                throw err;
            }
        }

        cacheValue.setCode(code);
        cacheValue.setAst(parser.ast());
    }

    ItemReaderASTVisitor astVisitor(*this, itemPool, m_logger, searchPaths);
    astVisitor.setFilePath(QFileInfo(filePath).absoluteFilePath());
    astVisitor.setSourceCode(cacheValue.code());
    cacheValue.setProcessingFlag(true);
    cacheValue.ast()->accept(&astVisitor);
    cacheValue.setProcessingFlag(false);
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


} // namespace Internal
} // namespace qbs
