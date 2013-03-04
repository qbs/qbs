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

#include "item.h"
#include "filecontext.h"
#include <parser/qmljsastvisitor_p.h>
#include <QHash>

namespace qbs {
namespace Internal {

class ItemReader;

class ItemReaderASTVisitor : public QbsQmlJS::AST::Visitor
{
public:
    ItemReaderASTVisitor(ItemReader *reader);
    ~ItemReaderASTVisitor();

    void setFilePath(const QString &filePath) { m_filePath = filePath; }
    void setSourceCode(const QString &sourceCode) { m_sourceCode = sourceCode; }
    void setInRecursion(bool inRecursion) { m_inRecursion = inRecursion; }
    ItemPtr rootItem() const { return m_item; }

    bool visit(QbsQmlJS::AST::UiProgram *ast);
    bool visit(QbsQmlJS::AST::UiImportList *uiImportList);
    bool visit(QbsQmlJS::AST::UiObjectDefinition *ast);
    bool visit(QbsQmlJS::AST::UiPublicMember *ast);
    bool visit(QbsQmlJS::AST::UiScriptBinding *ast);
    bool visit(QbsQmlJS::AST::Statement *statement);
    bool visit(QbsQmlJS::AST::FunctionDeclaration *ast);

private:
    CodeLocation toCodeLocation(QbsQmlJS::AST::SourceLocation location) const;
    ItemPtr targetItemForBinding(const ItemPtr &item, const QStringList &binding,
                                 const CodeLocation &bindingLocation);
    static void mergeItem(const ItemPtr &dst, const ItemConstPtr &src);
    static void ensureIdScope(const FileContextPtr &file);
    static void setupAlternatives(const ItemPtr &item);
    static void handlePropertiesBlock(const ItemPtr &item, const ItemConstPtr &block);

    ItemReader *m_reader;
    QString m_filePath;
    QString m_sourceCode;
    bool m_inRecursion;
    FileContextPtr m_file;
    QHash<QStringList, QString> m_typeNameToFile;
    ItemPtr m_item;
    JSSourceValuePtr m_sourceValue;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_ITEMREADERASTVISITOR_H
