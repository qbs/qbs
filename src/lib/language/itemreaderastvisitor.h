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

#ifndef QBS_ITEMREADERASTVISITOR_H
#define QBS_ITEMREADERASTVISITOR_H

#include "importversion.h"
#include "item.h"
#include "filecontext.h"
#include <parser/qmljsastvisitor_p.h>
#include <QHash>

namespace qbs {
namespace Internal {

class ItemReader;
struct ItemReaderResult;

class ItemReaderASTVisitor : public QbsQmlJS::AST::Visitor
{
public:
    ItemReaderASTVisitor(ItemReader *reader, ItemReaderResult *result);
    ~ItemReaderASTVisitor();

    void setFilePath(const QString &filePath) { m_filePath = filePath; }
    void setSourceCode(const QString &sourceCode) { m_sourceCode = sourceCode; }

    bool visit(QbsQmlJS::AST::UiProgram *ast);
    bool visit(QbsQmlJS::AST::UiImportList *uiImportList);
    bool visit(QbsQmlJS::AST::UiObjectDefinition *ast);
    bool visit(QbsQmlJS::AST::UiPublicMember *ast);
    bool visit(QbsQmlJS::AST::UiScriptBinding *ast);
    bool visit(QbsQmlJS::AST::FunctionDeclaration *ast);

private:
    bool visitStatement(QbsQmlJS::AST::Statement *statement);
    CodeLocation toCodeLocation(QbsQmlJS::AST::SourceLocation location) const;
    void checkDuplicateBinding(Item *item, const QStringList &bindingName,
            const QbsQmlJS::AST::SourceLocation &sourceLocation);
    Item *targetItemForBinding(Item *item, const QStringList &binding,
                                 const CodeLocation &bindingLocation);
    void checkImportVersion(const QbsQmlJS::AST::SourceLocation &versionToken) const;
    static void mergeItem(Item *dst, const Item *src,
                          const ItemReaderResult &baseFile);
    void ensureIdScope(const FileContextPtr &file);
    void setupAlternatives(Item *item);
    static void replaceConditionScopes(const JSSourceValuePtr &value, Item *newScope);
    void handlePropertiesBlock(Item *item, const Item *block);

    ItemReader *m_reader;
    ItemReaderResult *m_readerResult;
    const ImportVersion m_languageVersion;
    QString m_filePath;
    QString m_sourceCode;
    FileContextPtr m_file;
    QHash<QStringList, QString> m_typeNameToFile;
    Item *m_item;
    JSSourceValuePtr m_sourceValue;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_ITEMREADERASTVISITOR_H
