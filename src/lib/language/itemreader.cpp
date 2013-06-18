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

#include "itemreader.h"
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

class ItemReader::ASTCache : public QHash<QString, ASTCacheValue> {};


ItemReader::ItemReader(BuiltinDeclarations *builtins, const Logger &logger)
    : m_pool(0)
    , m_builtins(builtins)
    , m_logger(logger)
    , m_astCache(new ASTCache)
{
}

ItemReader::~ItemReader()
{
    delete m_astCache;
}

void ItemReader::setSearchPaths(const QStringList &searchPaths)
{
    m_searchPaths = searchPaths;
}

Item *ItemReader::readFile(const QString &filePath)
{
    return internalReadFile(filePath).rootItem;
}

QSet<QString> ItemReader::filesRead() const
{
    return m_filesRead;
}

ItemReaderResult ItemReader::internalReadFile(const QString &filePath)
{
    ASTCacheValue &cacheValue = (*m_astCache)[filePath];
    if (cacheValue.isValid()) {
        if (Q_UNLIKELY(cacheValue.isProcessing()))
            throw ErrorInfo(Tr::tr("Loop detected when importing '%1'.").arg(filePath));
    } else {
        QFile file(filePath);
        if (Q_UNLIKELY(!file.open(QFile::ReadOnly)))
            throw ErrorInfo(Tr::tr("Couldn't open '%1'.").arg(filePath));

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

    ItemReaderResult result;
    ItemReaderASTVisitor itemReader(this, &result);
    itemReader.setFilePath(QFileInfo(filePath).absoluteFilePath());
    itemReader.setSourceCode(cacheValue.code());
    cacheValue.setProcessingFlag(true);
    cacheValue.ast()->accept(&itemReader);
    cacheValue.setProcessingFlag(false);
    return result;
}

} // namespace Internal
} // namespace qbs
