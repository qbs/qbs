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

#ifndef QBS_ITEMREADERASTVISITOR_H
#define QBS_ITEMREADERASTVISITOR_H

#include "forward_decls.h"
#include "itemtype.h"

#include <logging/logger.h>
#include <parser/qmljsastvisitor_p.h>

#include <QtCore/qhash.h>
#include <QtCore/qstringlist.h>

namespace qbs {
class CodeLocation;

namespace Internal {
class Item;
class ItemPool;
class ItemReaderVisitorState;

class ItemReaderASTVisitor : public QbsQmlJS::AST::Visitor
{
public:
    ItemReaderASTVisitor(ItemReaderVisitorState &visitorState, FileContextPtr file,
                         ItemPool *itemPool, Logger &logger);
    void checkItemTypes() { doCheckItemTypes(rootItem()); }

    Item *rootItem() const { return m_item; }

private:
    bool visit(QbsQmlJS::AST::UiProgram *uiProgram) override;
    bool visit(QbsQmlJS::AST::UiObjectDefinition *ast) override;
    bool visit(QbsQmlJS::AST::UiPublicMember *ast) override;
    bool visit(QbsQmlJS::AST::UiScriptBinding *ast) override;

    bool handleBindingRhs(QbsQmlJS::AST::Statement *statement, const JSSourceValuePtr &value);
    CodeLocation toCodeLocation(const QbsQmlJS::AST::SourceLocation &location) const;
    void checkDuplicateBinding(Item *item, const QStringList &bindingName,
            const QbsQmlJS::AST::SourceLocation &sourceLocation);
    Item *targetItemForBinding(const QStringList &binding, const JSSourceValueConstPtr &value);
    static void inheritItem(Item *dst, const Item *src);
    void checkDeprecationStatus(ItemType itemType, const QString &itemName,
                                const CodeLocation &itemLocation);
    void doCheckItemTypes(const Item *item);

    ItemReaderVisitorState &m_visitorState;
    const FileContextPtr m_file;
    ItemPool * const m_itemPool;
    Logger &m_logger;
    QHash<QStringList, QString> m_typeNameToFile;
    Item *m_item = nullptr;
    ItemType m_instanceItemType = ItemType::ModuleInstance;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_ITEMREADERASTVISITOR_H
