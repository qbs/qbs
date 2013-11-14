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

#include "projectfileupdater.h"

#include "projectdata.h"
#include "qmljsrewriter.h"

#include <language/asttools.h>
#include <logging/translator.h>
#include <parser/qmljsast_p.h>
#include <parser/qmljsastvisitor_p.h>
#include <parser/qmljsengine_p.h>
#include <parser/qmljslexer_p.h>
#include <parser/qmljsparser_p.h>
#include <tools/qbsassert.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>

using namespace QbsQmlJS;
using namespace AST;

namespace qbs {
namespace Internal {

class ItemFinder : public Visitor
{
public:
    ItemFinder(const CodeLocation &cl) : m_cl(cl), m_item(0) { }

    UiObjectDefinition *item() const { return m_item; }

private:
    bool visit(UiObjectDefinition *ast)
    {
        if (toCodeLocation(m_cl.fileName(), ast->firstSourceLocation()) == m_cl) {
            m_item = ast;
            return false;
        }
        return true;
    }

    const CodeLocation m_cl;
    UiObjectDefinition *m_item;
};

class FilesBindingFinder : public Visitor
{
public:
    FilesBindingFinder(const UiObjectDefinition *startItem)
        : m_startItem(startItem), m_binding(0)
    {
    }

    UiScriptBinding *binding() const { return m_binding; }

private:
    bool visit(UiObjectDefinition *ast)
    {
        // We start with the direct parent of the binding, so do not descend into any
        // other item.
        return ast == m_startItem;
    }

    bool visit(UiScriptBinding *ast)
    {
        if (ast->qualifiedId->name.toString() != QLatin1String("files"))
            return true;
        m_binding = ast;
        return false;
    }

    const UiObjectDefinition * const m_startItem;
    UiScriptBinding *m_binding;
};


ProjectFileUpdater::ProjectFileUpdater(const QString &projectFile) : m_projectFile(projectFile)
{
}

void ProjectFileUpdater::apply()
{
    QFile file(m_projectFile);
    if (!file.open(QFile::ReadOnly)) {
        throw ErrorInfo(Tr::tr("File '%1' cannot be opened for reading: %2")
                        .arg(m_projectFile, file.errorString()));
    }
    QString content = QString::fromLocal8Bit(file.readAll());
    file.close();
    Engine engine;
    Lexer lexer(&engine);
    lexer.setCode(content, 1);
    Parser parser(&engine);
    if (!parser.parse()) {
        QList<DiagnosticMessage> parserMessages = parser.diagnosticMessages();
        if (!parserMessages.isEmpty()) {
            ErrorInfo errorInfo;
            errorInfo.append(Tr::tr("Failure parsing project file."));
            foreach (const DiagnosticMessage &msg, parserMessages)
                errorInfo.append(msg.message, toCodeLocation(file.fileName(), msg.loc));
            throw errorInfo;
        }
    }

    doApply(content, parser.ast());

    if (!file.open(QFile::WriteOnly)) {
        throw ErrorInfo(Tr::tr("File '%1' cannot be opened for writing: %2")
                        .arg(m_projectFile, file.errorString()));
    }
    file.resize(0);
    file.write(content.toLocal8Bit());
}


ProjectFileGroupInserter::ProjectFileGroupInserter(const ProductData &product,
                                                   const QString &groupName)
    : ProjectFileUpdater(product.location().fileName())
    , m_product(product)
    , m_groupName(groupName)
{
}

void ProjectFileGroupInserter::doApply(QString &fileContent, UiProgram *ast)
{
    ItemFinder itemFinder(m_product.location());
    ast->accept(&itemFinder);
    if (!itemFinder.item()) {
        throw ErrorInfo(Tr::tr("The project file parser failed to find the product item."),
                        CodeLocation(projectFile()));
    }

    ChangeSet changeSet;
    Rewriter rewriter(fileContent, &changeSet, QStringList());
    QString groupItemString;
    const int productItemIndentation
            = itemFinder.item()->qualifiedTypeNameId->firstSourceLocation().startColumn - 1;
    const int groupItemIndentation = productItemIndentation + 4;
    const QString groupItemIndentationString = QString(groupItemIndentation, QLatin1Char(' '));
    groupItemString += groupItemIndentationString + QLatin1String("Group {\n");
    groupItemString += groupItemIndentationString + groupItemIndentationString
            + QLatin1String("name: \"") + m_groupName + QLatin1String("\"\n");
    groupItemString += groupItemIndentationString + groupItemIndentationString
            + QLatin1String("files: []\n");
    groupItemString += groupItemIndentationString + QLatin1Char('}');
    rewriter.addObject(itemFinder.item()->initializer, groupItemString);

    int lineOffset = 3 + 1; // Our text + a leading newline that is always added by the rewriter.
    const QList<ChangeSet::EditOp> &editOps = changeSet.operationList();
    QBS_CHECK(editOps.count() == 1);
    const ChangeSet::EditOp &insertOp = editOps.first();
    setLineOffset(lineOffset);

    int insertionLine = fileContent.left(insertOp.pos1).count(QLatin1Char('\n'));
    for (int i = 0; i < insertOp.text.count() && insertOp.text.at(i) == QLatin1Char('\n'); ++i)
        ++insertionLine; // To account for newlines prepended by the rewriter.
    ++insertionLine; // To account for zero-based indexing.
    setItemPosition(CodeLocation(projectFile(), insertionLine,
                                 groupItemIndentation + 1));
    changeSet.apply(&fileContent);
}

ProjectFileFilesAdder::ProjectFileFilesAdder(const ProductData &product, const GroupData &group,
                                             const QStringList &files)
    : ProjectFileUpdater(product.location().fileName())
    , m_product(product)
    , m_group(group)
    , m_files(files)
{
}

void ProjectFileFilesAdder::doApply(QString &fileContent, UiProgram *ast)
{
    // Find the item containing the "files" binding.
    ItemFinder itemFinder(m_group.isValid() ? m_group.location() : m_product.location());
    ast->accept(&itemFinder);
    if (!itemFinder.item()) {
        throw ErrorInfo(Tr::tr("The project file parser failed to find the item."),
                        CodeLocation(projectFile()));
    }

    const int itemIndentation
            = itemFinder.item()->qualifiedTypeNameId->firstSourceLocation().startColumn - 1;
    const int bindingIndentation = itemIndentation + 4;
    const int arrayElemIndentation = bindingIndentation + 4;
    QString newFilesString;
    const QDir projectFileDir = QFileInfo(projectFile()).dir();
    foreach (const QString &absFilePath, m_files) {
        newFilesString += QString(arrayElemIndentation, QLatin1Char(' '));
        newFilesString += QLatin1Char('"');
        newFilesString += projectFileDir.relativeFilePath(absFilePath);
        newFilesString += QLatin1Char('"');
        newFilesString += QLatin1String(",\n");
    }
    newFilesString.chop(2); // Trailing comma and newline.

    // Now get the binding itself.
    FilesBindingFinder bindingFinder(itemFinder.item());
    itemFinder.item()->accept(&bindingFinder);

    ChangeSet changeSet;
    Rewriter rewriter(fileContent, &changeSet, QStringList());

    UiScriptBinding * const filesBinding = bindingFinder.binding();
    if (filesBinding) {
        if (filesBinding->statement->kind != Node::Kind_ExpressionStatement)
            throw ErrorInfo(Tr::tr("JavaScript construct in source file is too complex.")); // TODO: rename, add new and concat.
        const ExpressionStatement * const exprStatement
                = static_cast<ExpressionStatement *>(filesBinding->statement);
        switch (exprStatement->expression->kind) {
        case Node::Kind_ArrayLiteral: {
            QString filesString = QLatin1String("[\n");
            const ElementList *elem
                    = static_cast<ArrayLiteral *>(exprStatement->expression)->elements;
            while (elem) {
                const quint32 start = elem->expression->firstSourceLocation().offset;
                const quint32 end = elem->expression->lastSourceLocation().end();
                const QString elemRepr = fileContent.mid(start, end - start);
                filesString += QString(arrayElemIndentation, QLatin1Char(' '));
                filesString += elemRepr;
                filesString += QLatin1String(",\n");
                elem = elem->next;
            }
            filesString += newFilesString;
            filesString += QLatin1Char('\n');
            filesString += QString(bindingIndentation, QLatin1Char(' '));
            filesString += QLatin1Char(']');
            rewriter.changeBinding(itemFinder.item()->initializer, QLatin1String("files"),
                                   filesString, Rewriter::ScriptBinding);
            break;
        }
        case Node::Kind_StringLiteral: {
            const QString existingElement
                    = static_cast<StringLiteral *>(exprStatement->expression)->value.toString();
            QString filesString = QLatin1String("[\n");
            filesString += QString(arrayElemIndentation, QLatin1Char(' '));
            filesString += QLatin1Char('"') + existingElement + QLatin1Char('"');
            filesString += QLatin1String(",\n");
            filesString += newFilesString;
            filesString += QLatin1Char('\n');
            filesString += QString(bindingIndentation, QLatin1Char(' '));
            filesString += QLatin1Char(']');
            rewriter.changeBinding(itemFinder.item()->initializer, QLatin1String("files"),
                                   filesString, Rewriter::ScriptBinding);
            break;
        }
        default:
            qDebug("unsupported kind: %d", exprStatement->expression->kind);
            // TODO: If it's an identifier, call "concat()".
            throw ErrorInfo(Tr::tr("Javascript construct in source file is too complex."));
        }
    } else { // Can happen for the product itself, for which the "files" binding is not mandatory.
        newFilesString.prepend(QLatin1String("[\n"));
        newFilesString += QLatin1Char('\n');
        newFilesString += QString(bindingIndentation, QLatin1Char(' '));
        newFilesString += QLatin1Char(']');
        const QString bindingString = QString(bindingIndentation, QLatin1Char(' '))
                + QLatin1String("files");
        rewriter.addBinding(itemFinder.item()->initializer, bindingString, newFilesString,
                            Rewriter::ScriptBinding);
    }

    const QList<ChangeSet::EditOp> &editOps = changeSet.operationList();
    QBS_CHECK(editOps.count() == 1);
    const ChangeSet::EditOp &insertOp = editOps.first();
    setLineOffset(insertOp.text.count(QLatin1Char('\n')));

    const int insertionLine = fileContent.left(insertOp.pos1 + 1).count(QLatin1Char('\n')) + 2;
    const int insertionColumn = (filesBinding ? arrayElemIndentation : bindingIndentation) + 1;
    setItemPosition(CodeLocation(projectFile(), insertionLine, insertionColumn));
    changeSet.apply(&fileContent);
}


} // namespace Internal
} // namespace qbs
