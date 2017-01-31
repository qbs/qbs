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
#include <tools/hostosinfo.h>
#include <tools/jsliterals.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>

#include <QtCore/qfile.h>

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
        if (toCodeLocation(m_cl.filePath(), ast->firstSourceLocation()) == m_cl) {
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

ProjectFileUpdater::LineEndingType ProjectFileUpdater::guessLineEndingType(const QByteArray &text)
{
    char before = 0;
    int lfCount = 0;
    int crlfCount = 0;
    int i = text.indexOf('\n');
    while (i != -1) {
        if (i > 0)
            before = text.at(i - 1);
        if (before == '\r')
            ++crlfCount;
        else
            ++lfCount;
        i = text.indexOf('\n', i + 1);
    }
    if (lfCount == 0 && crlfCount == 0)
        return UnknownLineEndings;
    if (crlfCount == 0)
        return UnixLineEndings;
    if (lfCount == 0)
        return WindowsLineEndings;
    return MixedLineEndings;
}

void ProjectFileUpdater::convertToUnixLineEndings(QByteArray *text, LineEndingType oldLineEndings)
{
    if (oldLineEndings == UnixLineEndings)
        return;
    text->replace("\r\n", "\n");
}

void ProjectFileUpdater::convertFromUnixLineEndings(QByteArray *text, LineEndingType newLineEndings)
{
    if (newLineEndings == WindowsLineEndings
            || (newLineEndings != UnixLineEndings && HostOsInfo::isWindowsHost())) {
        text->replace('\n', "\r\n");
    }
}

void ProjectFileUpdater::apply()
{
    QFile file(m_projectFile);
    if (!file.open(QFile::ReadOnly)) {
        throw ErrorInfo(Tr::tr("File '%1' cannot be opened for reading: %2")
                        .arg(m_projectFile, file.errorString()));
    }
    QByteArray rawContent = file.readAll();
    const LineEndingType origLineEndingType = guessLineEndingType(rawContent);
    convertToUnixLineEndings(&rawContent, origLineEndingType);
    QString content = QString::fromUtf8(rawContent);

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
            for (const DiagnosticMessage &msg : qAsConst(parserMessages))
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
    rawContent = content.toUtf8();
    convertFromUnixLineEndings(&rawContent, origLineEndingType);
    file.write(rawContent);
}


ProjectFileGroupInserter::ProjectFileGroupInserter(const ProductData &product,
                                                   const QString &groupName)
    : ProjectFileUpdater(product.location().filePath())
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

static QString getNodeRepresentation(const QString &fileContent, const QbsQmlJS::AST::Node *node)
{
    const quint32 start = node->firstSourceLocation().offset;
    const quint32 end = node->lastSourceLocation().end();
    return fileContent.mid(start, end - start);
}

static const ChangeSet::EditOp &getEditOp(const ChangeSet &changeSet)
{
    const QList<ChangeSet::EditOp> &editOps = changeSet.operationList();
    QBS_CHECK(editOps.count() == 1);
    return editOps.first();
}

static int getLineOffsetForChangedBinding(const ChangeSet &changeSet, const QString &oldRhs)
{
    return getEditOp(changeSet).text.count(QLatin1Char('\n')) - oldRhs.count(QLatin1Char('\n'));
}

static int getBindingLine(const ChangeSet &changeSet, const QString &fileContent)
{
    return fileContent.left(getEditOp(changeSet).pos1 + 1).count(QLatin1Char('\n')) + 1;
}


ProjectFileFilesAdder::ProjectFileFilesAdder(const ProductData &product, const GroupData &group,
                                             const QStringList &files)
    : ProjectFileUpdater(product.location().filePath())
    , m_product(product)
    , m_group(group)
    , m_files(files)
{
}

static QString &addToFilesRepr(QString &filesRepr, const QString &fileRepr, int indentation)
{
    filesRepr += QString(indentation, QLatin1Char(' '));
    filesRepr += fileRepr;
    filesRepr += QLatin1String(",\n");
    return filesRepr;
}

static QString &addToFilesRepr(QString &filesRepr, const QStringList &filePaths, int indentation)
{
    for (const QString &f : filePaths)
        addToFilesRepr(filesRepr, toJSLiteral(f), indentation);
    return filesRepr;
}

static QString &completeFilesRepr(QString &filesRepr, int indentation)
{
    return filesRepr.prepend(QLatin1String("[\n")).append(QString(indentation, QLatin1Char(' ')))
            .append(QLatin1Char(']'));
}

void ProjectFileFilesAdder::doApply(QString &fileContent, UiProgram *ast)
{
    if (m_files.isEmpty())
        return;
    QStringList sortedFiles = m_files;
    sortedFiles.sort();

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

    // Now get the binding itself.
    FilesBindingFinder bindingFinder(itemFinder.item());
    itemFinder.item()->accept(&bindingFinder);

    ChangeSet changeSet;
    Rewriter rewriter(fileContent, &changeSet, QStringList());

    UiScriptBinding * const filesBinding = bindingFinder.binding();
    if (filesBinding) {
        QString filesRepresentation;
        if (filesBinding->statement->kind != QbsQmlJS::AST::Node::Kind_ExpressionStatement)
            throw ErrorInfo(Tr::tr("JavaScript construct in source file is too complex.")); // TODO: rename, add new and concat.
        const ExpressionStatement * const exprStatement
                = static_cast<ExpressionStatement *>(filesBinding->statement);
        switch (exprStatement->expression->kind) {
        case QbsQmlJS::AST::Node::Kind_ArrayLiteral: {
            const ElementList *elem
                    = static_cast<ArrayLiteral *>(exprStatement->expression)->elements;
            QStringList oldFileReprs;
            while (elem) {
                oldFileReprs << getNodeRepresentation(fileContent, elem->expression);
                elem = elem->next;
            }

            // Insert new files "sorted", but do not change the order of existing files.
            const QString firstNewFileRepr = toJSLiteral(sortedFiles.first());
            while (!oldFileReprs.isEmpty()) {
                if (oldFileReprs.first() > firstNewFileRepr)
                    break;
                addToFilesRepr(filesRepresentation, oldFileReprs.takeFirst(), arrayElemIndentation);
            }
            addToFilesRepr(filesRepresentation, sortedFiles, arrayElemIndentation);
            while (!oldFileReprs.isEmpty())
                addToFilesRepr(filesRepresentation, oldFileReprs.takeFirst(), arrayElemIndentation);
            completeFilesRepr(filesRepresentation, bindingIndentation);
            break;
        }
        case QbsQmlJS::AST::Node::Kind_StringLiteral: {
            const QString existingElement
                    = static_cast<StringLiteral *>(exprStatement->expression)->value.toString();
            sortedFiles << existingElement;
            sortedFiles.sort();
            addToFilesRepr(filesRepresentation, sortedFiles, arrayElemIndentation);
            completeFilesRepr(filesRepresentation, bindingIndentation);
            break;
        }
        default: {
            // Note that we can often do better than simply concatenating: For instance,
            // in the case where the existing list is of the form ["a", "b"].concat(myProperty),
            // we could keep on parsing until we find the array literal and then merge it with
            // the new files, preventing cascading concat() calls.
            // But this is not essential and can be implemented when we have some downtime.
            const QString rhsRepr = getNodeRepresentation(fileContent, exprStatement->expression);
            addToFilesRepr(filesRepresentation, sortedFiles, arrayElemIndentation);
            completeFilesRepr(filesRepresentation, bindingIndentation);

            // It cannot be the other way around, since the existing right-hand side could
            // have string type.
            filesRepresentation += QString::fromLatin1(".concat(%1)").arg(rhsRepr);

        }
        }
        rewriter.changeBinding(itemFinder.item()->initializer, QLatin1String("files"),
                               filesRepresentation, Rewriter::ScriptBinding);
    } else { // Can happen for the product itself, for which the "files" binding is not mandatory.
        QString filesRepresentation;
        addToFilesRepr(filesRepresentation, sortedFiles, arrayElemIndentation);
        completeFilesRepr(filesRepresentation, bindingIndentation);
        const QString bindingString = QString(bindingIndentation, QLatin1Char(' '))
                + QLatin1String("files");
        rewriter.addBinding(itemFinder.item()->initializer, bindingString, filesRepresentation,
                            Rewriter::ScriptBinding);
    }

    setLineOffset(getLineOffsetForChangedBinding(changeSet,
            filesBinding ? getNodeRepresentation(fileContent, filesBinding->statement)
                         : QString()));
    const int insertionLine = getBindingLine(changeSet, fileContent);
    const int insertionColumn = (filesBinding ? arrayElemIndentation : bindingIndentation) + 1;
    setItemPosition(CodeLocation(projectFile(), insertionLine, insertionColumn));
    changeSet.apply(&fileContent);
}

ProjectFileFilesRemover::ProjectFileFilesRemover(const ProductData &product, const GroupData &group,
                                                 const QStringList &files)
    : ProjectFileUpdater(product.location().filePath())
    , m_product(product)
    , m_group(group)
    , m_files(files)
{
}

void ProjectFileFilesRemover::doApply(QString &fileContent, UiProgram *ast)
{
    if (m_files.isEmpty())
        return;

    // Find the item containing the "files" binding.
    ItemFinder itemFinder(m_group.isValid() ? m_group.location() : m_product.location());
    ast->accept(&itemFinder);
    if (!itemFinder.item()) {
        throw ErrorInfo(Tr::tr("The project file parser failed to find the item."),
                        CodeLocation(projectFile()));
    }

    // Now get the binding itself.
    FilesBindingFinder bindingFinder(itemFinder.item());
    itemFinder.item()->accept(&bindingFinder);
    if (!bindingFinder.binding()) {
        throw ErrorInfo(Tr::tr("Could not find the 'files' binding in the project file."),
                        m_product.location());
    }

    if (bindingFinder.binding()->statement->kind != QbsQmlJS::AST::Node::Kind_ExpressionStatement)
        throw ErrorInfo(Tr::tr("JavaScript construct in source file is too complex."));
    const CodeLocation bindingLocation
            = toCodeLocation(projectFile(), bindingFinder.binding()->firstSourceLocation());

    ChangeSet changeSet;
    Rewriter rewriter(fileContent, &changeSet, QStringList());

    const int itemIndentation
            = itemFinder.item()->qualifiedTypeNameId->firstSourceLocation().startColumn - 1;
    const int bindingIndentation = itemIndentation + 4;
    const int arrayElemIndentation = bindingIndentation + 4;

    const ExpressionStatement * const exprStatement
            = static_cast<ExpressionStatement *>(bindingFinder.binding()->statement);
    switch (exprStatement->expression->kind) {
    case QbsQmlJS::AST::Node::Kind_ArrayLiteral: {
        QStringList filesToRemove = m_files;
        QStringList newFilesList;
        const ElementList *elem = static_cast<ArrayLiteral *>(exprStatement->expression)->elements;
        while (elem) {
            if (elem->expression->kind != QbsQmlJS::AST::Node::Kind_StringLiteral) {
                throw ErrorInfo(Tr::tr("JavaScript construct in source file is too complex."),
                                bindingLocation);
            }
            const QString existingFile
                    = static_cast<StringLiteral *>(elem->expression)->value.toString();
            if (!filesToRemove.removeOne(existingFile))
                newFilesList << existingFile;
            elem = elem->next;
        }
        if (!filesToRemove.isEmpty()) {
            throw ErrorInfo(Tr::tr("The following files were not found in the 'files' list: %1")
                            .arg(filesToRemove.join(QLatin1String(", "))), bindingLocation);
        }
        QString filesString = QLatin1String("[\n");
        for (const QString &file : qAsConst(newFilesList)) {
            filesString += QString(arrayElemIndentation, QLatin1Char(' '));
            filesString += QString::fromLatin1("\"%1\",\n").arg(file);
        }
        filesString += QString(bindingIndentation, QLatin1Char(' '));
        filesString += QLatin1Char(']');
        rewriter.changeBinding(itemFinder.item()->initializer, QLatin1String("files"),
                               filesString, Rewriter::ScriptBinding);
        break;
    }
    case QbsQmlJS::AST::Node::Kind_StringLiteral: {
        if (m_files.count() != 1) {
            throw ErrorInfo(Tr::tr("Was requested to remove %1 files, but there is only "
                                   "one in the list.").arg(m_files.count()), bindingLocation);
        }
        const QString existingFile
                = static_cast<StringLiteral *>(exprStatement->expression)->value.toString();
        if (existingFile != m_files.first()) {
            throw ErrorInfo(Tr::tr("File '%1' could not be found in the 'files' list.")
                            .arg(m_files.first()), bindingLocation);
        }
        rewriter.changeBinding(itemFinder.item()->initializer, QLatin1String("files"),
                               QLatin1String("[]"), Rewriter::ScriptBinding);
        break;
    }
    default:
        throw ErrorInfo(Tr::tr("JavaScript construct in source file is too complex."),
                        bindingLocation);
    }

    setLineOffset(getLineOffsetForChangedBinding(changeSet,
            getNodeRepresentation(fileContent, exprStatement->expression)));
    const int bindingLine = getBindingLine(changeSet, fileContent);
    const int bindingColumn = (bindingFinder.binding()
                               ? arrayElemIndentation : bindingIndentation) + 1;
    setItemPosition(CodeLocation(projectFile(), bindingLine, bindingColumn));
    changeSet.apply(&fileContent);
}


ProjectFileGroupRemover::ProjectFileGroupRemover(const ProductData &product, const GroupData &group)
    : ProjectFileUpdater(product.location().filePath())
    , m_product(product)
    , m_group(group)
{
}

void ProjectFileGroupRemover::doApply(QString &fileContent, UiProgram *ast)
{
    ItemFinder productFinder(m_product.location());
    ast->accept(&productFinder);
    if (!productFinder.item()) {
        throw ErrorInfo(Tr::tr("The project file parser failed to find the product item."),
                        CodeLocation(projectFile()));
    }

    ItemFinder groupFinder(m_group.location());
    productFinder.item()->accept(&groupFinder);
    if (!groupFinder.item()) {
        throw ErrorInfo(Tr::tr("The project file parser failed to find the group item."),
                        m_product.location());
    }

    ChangeSet changeSet;
    Rewriter rewriter(fileContent, &changeSet, QStringList());
    rewriter.removeObjectMember(groupFinder.item(), productFinder.item());

    setItemPosition(m_group.location());
    const QList<ChangeSet::EditOp> &editOps = changeSet.operationList();
    QBS_CHECK(editOps.count() == 1);
    const ChangeSet::EditOp &op = editOps.first();
    const QString removedText = fileContent.mid(op.pos1, op.length1);
    setLineOffset(-removedText.count(QLatin1Char('\n')));

    changeSet.apply(&fileContent);
}

} // namespace Internal
} // namespace qbs
