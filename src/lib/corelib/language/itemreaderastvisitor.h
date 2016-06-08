/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
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

#ifndef QBS_ITEMREADERASTVISITOR_H
#define QBS_ITEMREADERASTVISITOR_H

#include "forward_decls.h"
#include "itemtype.h"

#include <logging/logger.h>
#include <parser/qmljsastvisitor_p.h>

#include <QHash>
#include <QStringList>

namespace qbs {
class CodeLocation;

namespace Internal {
class Item;
class ItemPool;
class ItemReaderVisitorState;

class ItemReaderASTVisitor : public QbsQmlJS::AST::Visitor
{
public:
    ItemReaderASTVisitor(ItemReaderVisitorState &visitorState, const FileContextPtr &file,
                         ItemPool *itemPool, Logger logger);

    Item *rootItem() const { return m_item; }

private:
    bool visit(QbsQmlJS::AST::UiImportList *uiImportList) override;
    bool visit(QbsQmlJS::AST::UiObjectDefinition *ast) override;
    bool visit(QbsQmlJS::AST::UiPublicMember *ast) override;
    bool visit(QbsQmlJS::AST::UiScriptBinding *ast) override;
    bool visit(QbsQmlJS::AST::FunctionDeclaration *ast) override;

    bool handleBindingRhs(QbsQmlJS::AST::Statement *statement, const JSSourceValuePtr &value);
    CodeLocation toCodeLocation(const QbsQmlJS::AST::SourceLocation &location) const;
    void checkDuplicateBinding(Item *item, const QStringList &bindingName,
            const QbsQmlJS::AST::SourceLocation &sourceLocation);
    Item *targetItemForBinding(const QStringList &binding, const JSSourceValueConstPtr &value);
    static void inheritItem(Item *dst, const Item *src);
    void checkDeprecationStatus(ItemType itemType, const QString &itemName,
                                const CodeLocation &itemLocation);

    ItemReaderVisitorState &m_visitorState;
    const FileContextPtr m_file;
    ItemPool * const m_itemPool;
    Logger m_logger;
    QHash<QStringList, QString> m_typeNameToFile;
    Item *m_item = nullptr;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_ITEMREADERASTVISITOR_H
