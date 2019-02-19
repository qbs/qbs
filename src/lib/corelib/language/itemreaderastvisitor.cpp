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

#include "itemreaderastvisitor.h"

#include "astimportshandler.h"
#include "astpropertiesitemhandler.h"
#include "asttools.h"
#include "builtindeclarations.h"
#include "filecontext.h"
#include "identifiersearch.h"
#include "item.h"
#include "itemreadervisitorstate.h"
#include "value.h"

#include <api/languageinfo.h>
#include <jsextensions/jsextensions.h>
#include <parser/qmljsast_p.h>
#include <tools/codelocation.h>
#include <tools/error.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/stringconstants.h>
#include <logging/translator.h>

#include <algorithm>

using namespace QbsQmlJS;

namespace qbs {
namespace Internal {

ItemReaderASTVisitor::ItemReaderASTVisitor(ItemReaderVisitorState &visitorState,
        FileContextPtr file, ItemPool *itemPool, Logger &logger)
    : m_visitorState(visitorState)
    , m_file(std::move(file))
    , m_itemPool(itemPool)
    , m_logger(logger)
{
}

bool ItemReaderASTVisitor::visit(AST::UiProgram *uiProgram)
{
    ASTImportsHandler importsHandler(m_visitorState, m_logger, m_file);
    importsHandler.handleImports(uiProgram->imports);
    m_typeNameToFile = importsHandler.typeNameFileMap();
    return true;
}

static ItemValuePtr findItemProperty(const Item *container, const Item *item)
{
    ItemValuePtr itemValue;
    const auto &srcprops = container->properties();
    auto it = std::find_if(srcprops.begin(), srcprops.end(), [item] (const ValuePtr &v) {
        return v->type() == Value::ItemValueType
                && std::static_pointer_cast<ItemValue>(v)->item() == item;
    });
    if (it != srcprops.end())
        itemValue = std::static_pointer_cast<ItemValue>(it.value());
    return itemValue;
}

bool ItemReaderASTVisitor::visit(AST::UiObjectDefinition *ast)
{
    const QString typeName = ast->qualifiedTypeNameId->name.toString();
    const CodeLocation itemLocation = toCodeLocation(ast->qualifiedTypeNameId->identifierToken);
    const Item *baseItem = nullptr;
    Item *mostDerivingItem = nullptr;

    Item *item = Item::create(m_itemPool, ItemType::Unknown);
    item->setFile(m_file);
    item->setLocation(itemLocation);

    // Inheritance resolving, part 1: Find out our actual type name (needed for setting
    // up children and alternatives).
    const QStringList fullTypeName = toStringList(ast->qualifiedTypeNameId);
    const QString baseTypeFileName = m_typeNameToFile.value(fullTypeName);
    ItemType itemType;
    if (!baseTypeFileName.isEmpty()) {
        const bool isMostDerivingItem = (m_visitorState.mostDerivingItem() == nullptr);
        if (isMostDerivingItem)
            m_visitorState.setMostDerivingItem(item);
        mostDerivingItem = m_visitorState.mostDerivingItem();
        baseItem = m_visitorState.readFile(baseTypeFileName, m_file->searchPaths(), m_itemPool);
        if (isMostDerivingItem)
            m_visitorState.setMostDerivingItem(nullptr);
        QBS_CHECK(baseItem->type() <= ItemType::LastActualItem);
        itemType = baseItem->type();
    } else {
        if (fullTypeName.size() > 1) {
            throw ErrorInfo(Tr::tr("Invalid item '%1'. Did you mean to set a module property?")
                            .arg(fullTypeName.join(QLatin1Char('.'))), itemLocation);
        }
        itemType = BuiltinDeclarations::instance().typeForName(typeName, itemLocation);
        checkDeprecationStatus(itemType, typeName, itemLocation);
        if (itemType == ItemType::Properties && m_item && m_item->type() == ItemType::SubProject)
            itemType = ItemType::PropertiesInSubProject;
    }

    item->m_type = itemType;

    if (m_item)
        Item::addChild(m_item, item); // Add this item to the children of the parent item.
    else
        m_item = item; // This is the root item.

    if (ast->initializer) {
        Item *mdi = m_visitorState.mostDerivingItem();
        m_visitorState.setMostDerivingItem(nullptr);
        qSwap(m_item, item);
        const ItemType oldInstanceItemType = m_instanceItemType;
        if (itemType == ItemType::Parameters || itemType == ItemType::Depends)
            m_instanceItemType = ItemType::ModuleParameters;
        ast->initializer->accept(this);
        m_instanceItemType = oldInstanceItemType;
        qSwap(m_item, item);
        m_visitorState.setMostDerivingItem(mdi);
    }

    ASTPropertiesItemHandler(item).handlePropertiesItems();

    // Inheritance resolving, part 2 (depends on alternatives having been set up).
    if (baseItem) {
        inheritItem(item, baseItem);
        if (baseItem->file()->idScope()) {
            // Make ids from the derived file visible in the base file.
            // ### Do we want to turn off this feature? It's QMLish but kind of strange.
            item->file()->ensureIdScope(m_itemPool);
            baseItem->file()->idScope()->setPrototype(item->file()->idScope());

            // Replace the base item with the most deriving item.
            ItemValuePtr baseItemIdValue = findItemProperty(baseItem->file()->idScope(), baseItem);
            if (baseItemIdValue)
                baseItemIdValue->setItem(mostDerivingItem);
        }
    } else {
        // Only the item at the top of the inheritance chain is a built-in item.
        // We cannot do this in "part 1", because then the visitor would complain about duplicate
        // bindings.
        item->setupForBuiltinType(m_logger);
    }

    return false;
}

void ItemReaderASTVisitor::checkDuplicateBinding(Item *item, const QStringList &bindingName,
                                                 const AST::SourceLocation &sourceLocation)
{
    if (Q_UNLIKELY(item->hasOwnProperty(bindingName.last()))) {
        QString msg = Tr::tr("Duplicate binding for '%1'");
        throw ErrorInfo(msg.arg(bindingName.join(QLatin1Char('.'))),
                    toCodeLocation(sourceLocation));
    }
}

bool ItemReaderASTVisitor::visit(AST::UiPublicMember *ast)
{
    PropertyDeclaration p;
    if (Q_UNLIKELY(ast->name.isEmpty()))
        throw ErrorInfo(Tr::tr("public member without name"));
    if (Q_UNLIKELY(ast->memberType.isEmpty()))
        throw ErrorInfo(Tr::tr("public member without type"));
    if (Q_UNLIKELY(ast->type == AST::UiPublicMember::Signal))
        throw ErrorInfo(Tr::tr("public member with signal type not supported"));
    p.setName(ast->name.toString());
    p.setType(PropertyDeclaration::propertyTypeFromString(ast->memberType.toString()));
    if (p.type() == PropertyDeclaration::UnknownType) {
        throw ErrorInfo(Tr::tr("Unknown type '%1' in property declaration.")
                        .arg(ast->memberType.toString()), toCodeLocation(ast->typeToken));
    }
    if (Q_UNLIKELY(!ast->typeModifier.isEmpty())) {
        throw ErrorInfo(Tr::tr("public member with type modifier '%1' not supported").arg(
                        ast->typeModifier.toString()));
    }
    if (ast->isReadonlyMember)
        p.setFlags(PropertyDeclaration::ReadOnlyFlag);

    m_item->m_propertyDeclarations.insert(p.name(), p);

    const JSSourceValuePtr value = JSSourceValue::create();
    value->setFile(m_file);
    if (ast->statement) {
        handleBindingRhs(ast->statement, value);
        const QStringList bindingName(p.name());
        checkDuplicateBinding(m_item, bindingName, ast->colonToken);
    }

    m_item->setProperty(p.name(), value);
    return false;
}

bool ItemReaderASTVisitor::visit(AST::UiScriptBinding *ast)
{
    QBS_CHECK(ast->qualifiedId);
    QBS_CHECK(!ast->qualifiedId->name.isEmpty());

    const QStringList bindingName = toStringList(ast->qualifiedId);

    if (bindingName.length() == 1 && bindingName.front() == QStringLiteral("id")) {
        const auto * const expStmt = AST::cast<AST::ExpressionStatement *>(ast->statement);
        if (Q_UNLIKELY(!expStmt))
            throw ErrorInfo(Tr::tr("id: must be followed by identifier"));
        const auto * const idExp = AST::cast<AST::IdentifierExpression *>(expStmt->expression);
        if (Q_UNLIKELY(!idExp || idExp->name.isEmpty()))
            throw ErrorInfo(Tr::tr("id: must be followed by identifier"));
        m_item->m_id = idExp->name.toString();
        m_file->ensureIdScope(m_itemPool);
        ItemValueConstPtr existingId = m_file->idScope()->itemProperty(m_item->id());
        if (existingId) {
            ErrorInfo e(Tr::tr("The id '%1' is not unique.").arg(m_item->id()));
            e.append(Tr::tr("First occurrence is here."), existingId->item()->location());
            e.append(Tr::tr("Next occurrence is here."), m_item->location());
            throw e;
        }
        m_file->idScope()->setProperty(m_item->id(), ItemValue::create(m_item));
        return false;
    }

    const JSSourceValuePtr value = JSSourceValue::create();
    handleBindingRhs(ast->statement, value);

    Item * const targetItem = targetItemForBinding(bindingName, value);
    checkDuplicateBinding(targetItem, bindingName, ast->qualifiedId->identifierToken);
    targetItem->setProperty(bindingName.last(), value);
    return false;
}

bool ItemReaderASTVisitor::handleBindingRhs(AST::Statement *statement,
                                            const JSSourceValuePtr &value)
{
    QBS_CHECK(statement);
    QBS_CHECK(value);

    if (AST::cast<AST::Block *>(statement))
        value->m_flags |= JSSourceValue::HasFunctionForm;

    value->setFile(m_file);
    value->setSourceCode(textRefOf(m_file->content(), statement));
    value->setLocation(statement->firstSourceLocation().startLine,
                       statement->firstSourceLocation().startColumn);

    bool usesBase, usesOuter, usesOriginal;
    IdentifierSearch idsearch;
    idsearch.add(StringConstants::baseVar(), &usesBase);
    idsearch.add(StringConstants::outerVar(), &usesOuter);
    idsearch.add(StringConstants::originalVar(), &usesOriginal);
    idsearch.start(statement);
    if (usesBase)
        value->m_flags |= JSSourceValue::SourceUsesBase;
    if (usesOuter)
        value->m_flags |= JSSourceValue::SourceUsesOuter;
    if (usesOriginal)
        value->m_flags |= JSSourceValue::SourceUsesOriginal;
    return false;
}

CodeLocation ItemReaderASTVisitor::toCodeLocation(const AST::SourceLocation &location) const
{
    return CodeLocation(m_file->filePath(), location.startLine, location.startColumn);
}

Item *ItemReaderASTVisitor::targetItemForBinding(const QStringList &bindingName,
                                                 const JSSourceValueConstPtr &value)
{
    Item *targetItem = m_item;
    const int c = bindingName.size() - 1;
    for (int i = 0; i < c; ++i) {
        ValuePtr v = targetItem->ownProperty(bindingName.at(i));
        if (!v) {
            const ItemType itemType = i < c - 1 ? ItemType::ModulePrefix : m_instanceItemType;
            Item *newItem = Item::create(m_itemPool, itemType);
            newItem->setLocation(value->location());
            v = ItemValue::create(newItem);
            targetItem->setProperty(bindingName.at(i), v);
        }
        if (Q_UNLIKELY(v->type() != Value::ItemValueType)) {
            QString msg = Tr::tr("Binding to non-item property.");
            throw ErrorInfo(msg, value->location());
        }
        targetItem = std::static_pointer_cast<ItemValue>(v)->item();
    }
    return targetItem;
}

void ItemReaderASTVisitor::inheritItem(Item *dst, const Item *src)
{
    int insertPos = 0;
    for (Item *child : qAsConst(src->m_children)) {
        dst->m_children.insert(insertPos++, child);
        child->m_parent = dst;
    }

    for (const PropertyDeclaration &pd : src->propertyDeclarations()) {
        if (pd.flags().testFlag(PropertyDeclaration::ReadOnlyFlag)
                && dst->hasOwnProperty(pd.name())) {
            throw ErrorInfo(Tr::tr("Cannot set read-only property '%1'.").arg(pd.name()),
                            dst->property(pd.name())->location());
        }
        dst->setPropertyDeclaration(pd.name(), pd);
    }

    for (auto it = src->properties().constBegin(); it != src->properties().constEnd(); ++it) {
        ValuePtr &v = dst->m_properties[it.key()];
        if (!v) {
            v = it.value();
            continue;
        }
        if (v->type() == Value::ItemValueType && it.value()->type() != Value::ItemValueType)
            throw ErrorInfo(Tr::tr("Binding to non-item property."), v->location());
        if (v->type() != it.value()->type())
            continue;
        switch (v->type()) {
        case Value::JSSourceValueType: {
            JSSourceValuePtr sv = std::static_pointer_cast<JSSourceValue>(v);
            QBS_CHECK(!sv->baseValue());
            const JSSourceValuePtr baseValue = std::static_pointer_cast<JSSourceValue>(it.value());
            sv->setBaseValue(baseValue);
            for (const JSSourceValue::Alternative &alt : sv->m_alternatives)
                alt.value->setBaseValue(baseValue);
            break;
        }
        case Value::ItemValueType:
            inheritItem(std::static_pointer_cast<ItemValue>(v)->item(),
                        std::static_pointer_cast<const ItemValue>(it.value())->item());
            break;
        default:
            QBS_CHECK(!"unexpected value type");
        }
    }
}

void ItemReaderASTVisitor::checkDeprecationStatus(ItemType itemType, const QString &itemName,
                                                  const CodeLocation &itemLocation)
{
    const ItemDeclaration itemDecl = BuiltinDeclarations::instance().declarationsForType(itemType);
    const DeprecationInfo &di = itemDecl.deprecationInfo();
    if (!di.isValid())
        return;
    if (di.removalVersion() <= LanguageInfo::qbsVersion()) {
        QString message = Tr::tr("The item '%1' cannot be used anymore. "
                "It was removed in qbs %2.")
                .arg(itemName, di.removalVersion().toString());
        ErrorInfo error(message, itemLocation);
        if (!di.additionalUserInfo().isEmpty())
            error.append(di.additionalUserInfo());
        throw error;
    }
    QString warning = Tr::tr("The item '%1' is deprecated and will be removed in "
                             "qbs %2.").arg(itemName, di.removalVersion().toString());
    ErrorInfo error(warning, itemLocation);
    if (!di.additionalUserInfo().isEmpty())
        error.append(di.additionalUserInfo());
    m_logger.printWarning(error);
}

void ItemReaderASTVisitor::doCheckItemTypes(const Item *item)
{
    const ItemDeclaration decl = BuiltinDeclarations::instance().declarationsForType(item->type());
    for (const Item * const child : item->children()) {
        if (!decl.isChildTypeAllowed(child->type())) {
            throw ErrorInfo(Tr::tr("Items of type '%1' cannot contain items of type '%2'.")
                            .arg(item->typeName(), child->typeName()), child->location());
        }
        doCheckItemTypes(child);
    }
}

} // namespace Internal
} // namespace qbs
