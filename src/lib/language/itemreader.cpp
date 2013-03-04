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
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace qbs {
namespace Internal {

ItemReader::ItemReader(BuiltinDeclarations *builtins)
    : m_builtins(builtins)
{
}

void ItemReader::setSearchPaths(const QStringList &searchPaths)
{
    m_searchPaths = searchPaths;
}

ItemPtr ItemReader::readFile(const QString &filePath, bool inRecursion)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
        throw Error(Tr::tr("Couldn't open '%1'.").arg(filePath));

    const QString code = QTextStream(&file).readAll();
    file.close();
    QbsQmlJS::Engine engine;
    QbsQmlJS::Lexer lexer(&engine);
    lexer.setCode(code, 1);
    QbsQmlJS::Parser parser(&engine);
    if (!parser.parse()) {
        QList<QbsQmlJS::DiagnosticMessage> parserMessages = parser.diagnosticMessages();
        if (!parserMessages.isEmpty()) {
            Error err;
            foreach (const QbsQmlJS::DiagnosticMessage &msg, parserMessages)
                err.append(msg.message, toCodeLocation(filePath, msg.loc));
            throw err;
        }
    }

    ItemReaderASTVisitor itemReader(this);
    itemReader.setFilePath(QFileInfo(filePath).absoluteFilePath());
    itemReader.setSourceCode(code);
    itemReader.setInRecursion(inRecursion);
    parser.ast()->accept(&itemReader);
    return itemReader.rootItem();
}

} // namespace Internal
} // namespace qbs
