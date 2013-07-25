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

#include "itemreaderastvisitor.h"
#include "asttools.h"
#include "builtindeclarations.h"
#include "identifiersearch.h"
#include "itemreader.h"
#include <jsextensions/jsextensions.h>
#include <parser/qmljsast_p.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <logging/translator.h>

#include <QDirIterator>
#include <QFileInfo>
#include <QStringList>

using namespace QbsQmlJS;

namespace qbs {
namespace Internal {

ItemReaderASTVisitor::ItemReaderASTVisitor(ItemReader *reader, ItemReaderResult *result)
    : m_reader(reader)
    , m_readerResult(result)
    , m_languageVersion(ImportVersion::fromString(reader->builtins()->languageVersion()))
    , m_item(0)
    , m_sourceValue(0)
{
}

ItemReaderASTVisitor::~ItemReaderASTVisitor()
{
}

bool ItemReaderASTVisitor::visit(AST::UiProgram *ast)
{
    Q_UNUSED(ast);
    m_sourceValue.clear();
    m_file = FileContext::create();
    m_file->m_filePath = m_filePath;

    if (Q_UNLIKELY(!ast->members->member))
        throw ErrorInfo(Tr::tr("No root item found in %1.").arg(m_filePath));

    return true;
}

static void collectPrototypes(const QString &path, const QString &as,
                              QHash<QStringList, QString> *prototypeToFile)
{
    QDirIterator dirIter(path, QStringList("*.qbs"));
    while (dirIter.hasNext()) {
        const QString filePath = dirIter.next();
        const QString fileName = dirIter.fileName();

        if (fileName.size() <= 4)
            continue;

        const QString componentName = fileName.left(fileName.size() - 4);
        // ### validate componentName

        if (!componentName.at(0).isUpper())
            continue;

        QStringList prototypeName;
        if (!as.isEmpty())
            prototypeName.append(as);
        prototypeName.append(componentName);

        prototypeToFile->insert(prototypeName, filePath);
    }
}

bool ItemReaderASTVisitor::visit(AST::UiImportList *uiImportList)
{
    const QString path = FileInfo::path(m_filePath);

    // files in the same directory are available as prototypes
    collectPrototypes(path, QString(), &m_typeNameToFile);

    QSet<QString> importAsNames;
    QHash<QString, JsImport> jsImports;

    for (const AST::UiImportList *it = uiImportList; it; it = it->next) {
        const AST::UiImport *const import = it->import;

        QStringList importUri;
        bool isBase = false;
        if (import->importUri) {
            importUri = toStringList(import->importUri);
            isBase = (importUri.size() == 1 && importUri.first() == QLatin1String("qbs"))
                    || (importUri.size() == 2 && importUri.first() == QLatin1String("qbs")
                        && importUri.last() == QLatin1String("base"));
            if (isBase)
                checkImportVersion(import->versionToken);
            else if (import->versionToken.length)
                m_reader->logger().printWarning(ErrorInfo(Tr::tr("Superfluous version specification."),
                                                    toCodeLocation(import->versionToken)));
        }

        QString as;
        if (isBase) {
            if (Q_UNLIKELY(!import->importId.isNull())) {
                throw ErrorInfo(Tr::tr("Import of qbs.base must have no 'as <Name>'"),
                            toCodeLocation(import->importIdToken));
            }
        } else {
            if (importUri.count() == 2 && importUri.first() == QLatin1String("qbs")) {
                const QString extensionName = importUri.last();
                if (JsExtensions::hasExtension(extensionName)) {
                    if (Q_UNLIKELY(!import->importId.isNull())) {
                        throw ErrorInfo(Tr::tr("Import of built-in extension '%1' "
                                               "must not have 'as' specifier.").arg(extensionName));
                    }
                    if (Q_UNLIKELY(m_file->m_jsExtensions.contains(extensionName))) {
                        m_reader->logger().printWarning(Tr::tr("Built-in extension '%1' already "
                                                               "imported.").arg(extensionName));
                    } else {
                        m_file->m_jsExtensions << extensionName;
                    }
                    continue;
                }
            }

            if (import->importId.isNull()) {
                if (!import->fileName.isNull()) {
                    throw ErrorInfo(Tr::tr("File imports require 'as <Name>'"),
                                    toCodeLocation(import->importToken));
                }
                if (importUri.isEmpty()) {
                    throw ErrorInfo(Tr::tr("Invalid import URI."),
                                    toCodeLocation(import->importToken));
                }
                as = importUri.last();
            } else {
                as = import->importId.toString();
            }

            if (Q_UNLIKELY(importAsNames.contains(as))) {
                throw ErrorInfo(Tr::tr("Can't import into the same name more than once."),
                            toCodeLocation(import->importIdToken));
            }
            if (Q_UNLIKELY(JsExtensions::hasExtension(as))) {
                throw ErrorInfo(Tr::tr("Cannot reuse the name of built-in extension '%1'.")
                                .arg(as));
            }
            importAsNames.insert(as);
        }

        if (!import->fileName.isNull()) {
            QString name = FileInfo::resolvePath(path, import->fileName.toString());

            QFileInfo fi(name);
            if (Q_UNLIKELY(!fi.exists()))
                throw ErrorInfo(Tr::tr("Can't find imported file %0.").arg(name),
                            CodeLocation(m_filePath, import->fileNameToken.startLine,
                                         import->fileNameToken.startColumn));
            name = fi.canonicalFilePath();
            if (fi.isDir()) {
                collectPrototypes(name, as, &m_typeNameToFile);
            } else {
                if (name.endsWith(".js", Qt::CaseInsensitive)) {
                    JsImport &jsImport = jsImports[as];
                    jsImport.scopeName = as;
                    jsImport.fileNames.append(name);
                    jsImport.location = toCodeLocation(import->firstSourceLocation());
                } else if (name.endsWith(".qbs", Qt::CaseInsensitive)) {
                    m_typeNameToFile.insert(QStringList(as), name);
                } else {
                    throw ErrorInfo(Tr::tr("Can only import .qbs and .js files"),
                                CodeLocation(m_filePath, import->fileNameToken.startLine,
                                             import->fileNameToken.startColumn));
                }
            }
        } else if (!importUri.isEmpty()) {
            const QString importPath = isBase
                    ? QLatin1String("qbs/base") : importUri.join(QDir::separator());
            bool found = false;
            foreach (const QString &searchPath, m_reader->searchPaths()) {
                const QFileInfo fi(FileInfo::resolvePath(
                                       FileInfo::resolvePath(searchPath, "imports"), importPath));
                if (fi.isDir()) {
                    // ### versioning, qbsdir file, etc.
                    const QString &resultPath = fi.absoluteFilePath();
                    collectPrototypes(resultPath, as, &m_typeNameToFile);

                    QDirIterator dirIter(resultPath, QStringList("*.js"));
                    while (dirIter.hasNext()) {
                        dirIter.next();
                        JsImport &jsImport = jsImports[as];
                        if (jsImport.scopeName.isNull()) {
                            jsImport.scopeName = as;
                            jsImport.location = toCodeLocation(import->firstSourceLocation());
                        }
                        jsImport.fileNames.append(dirIter.filePath());
                    }
                    found = true;
                    break;
                }
            }
            if (Q_UNLIKELY(!found)) {
                throw ErrorInfo(Tr::tr("import %1 not found").arg(importUri.join(".")),
                            toCodeLocation(import->fileNameToken));
            }
        }
    }

    for (QHash<QString, JsImport>::const_iterator it = jsImports.constBegin();
         it != jsImports.constEnd(); ++it)
    {
        m_file->m_jsImports += it.value();
    }

    return false;
}

bool ItemReaderASTVisitor::visit(AST::UiObjectDefinition *ast)
{
    const QString typeName = ast->qualifiedTypeNameId->name.toString();

    Item *item = Item::create(m_reader->m_pool);
    item->m_file = m_file;
    item->m_parent = m_item;
    item->m_typeName = typeName;
    item->m_location = ::qbs::Internal::toCodeLocation(m_file->filePath(),
                                                       ast->qualifiedTypeNameId->identifierToken);

    if (m_item) {
        // Add this item to the children of the parent item.
        m_item->m_children += item;
    } else {
        // This is the root item.
        m_item = item;
        m_readerResult->rootItem = item;
    }

    if (ast->initializer) {
        qSwap(m_item, item);
        ast->initializer->accept(this);
        qSwap(m_item, item);
    }

    m_reader->m_builtins->setupItemForBuiltinType(item);

    if (item->typeName() != QLatin1String("Properties")
            && item->typeName() != QLatin1String("SubProject")) {
        setupAlternatives(item);
    }

    // resolve inheritance
    const QStringList fullTypeName = toStringList(ast->qualifiedTypeNameId);
    const QString baseTypeFileName = m_typeNameToFile.value(fullTypeName);
    if (!baseTypeFileName.isEmpty()) {
        const ItemReaderResult baseFile = m_reader->internalReadFile(baseTypeFileName);
        mergeItem(item, baseFile.rootItem, baseFile);
        if (baseFile.rootItem->m_file->m_idScope) {
            // Make ids from the derived file visible in the base file.
            // ### Do we want to turn off this feature? It's QMLish but kind of strange.
            ensureIdScope(item->m_file);
            baseFile.rootItem->m_file->m_idScope->setPrototype(item->m_file->m_idScope);
        }
    }

    return false;
}

void ItemReaderASTVisitor::checkDuplicateBinding(Item *item, const QStringList &bindingName,
                                  const AST::SourceLocation &sourceLocation)
{
    if (Q_UNLIKELY(item->properties().contains(bindingName.last()))) {
        QString msg = Tr::tr("Duplicate binding for '%1'");
        throw ErrorInfo(msg.arg(bindingName.join(".")),
                    qbs::Internal::toCodeLocation(m_file->filePath(), sourceLocation));
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
    p.name = ast->name.toString();
    p.type = PropertyDeclaration::propertyTypeFromString(ast->memberType.toString());
    if (p.type == PropertyDeclaration::UnknownType)
        throw ErrorInfo(Tr::tr("Unknown type '%1' in property declaration.")
                        .arg(ast->memberType.toString()), toCodeLocation(ast->typeToken));
    if (ast->typeModifier.compare(QLatin1String("list")))
        p.flags |= PropertyDeclaration::ListProperty;
    else if (Q_UNLIKELY(!ast->typeModifier.isEmpty()))
        throw ErrorInfo(Tr::tr("public member with type modifier '%1' not supported").arg(
                        ast->typeModifier.toString()));

    m_item->m_propertyDeclarations.insert(p.name, p);

    JSSourceValuePtr value = JSSourceValue::create();
    value->setFile(m_file);
    if (ast->statement) {
        m_sourceValue.swap(value);
        visitStatement(ast->statement);
        m_sourceValue.swap(value);
        const QStringList bindingName(p.name);
        checkDuplicateBinding(m_item, bindingName, ast->colonToken);
    }

    m_item->m_properties.insert(p.name, value);
    return false;
}

bool ItemReaderASTVisitor::visit(AST::UiScriptBinding *ast)
{
    QBS_CHECK(ast->qualifiedId);
    QBS_CHECK(!ast->qualifiedId->name.isEmpty());

    const QStringList bindingName = toStringList(ast->qualifiedId);

    if (bindingName.length() == 1 && bindingName.first() == QLatin1String("id")) {
        AST::ExpressionStatement *expStmt =
                AST::cast<AST::ExpressionStatement *>(ast->statement);
        if (Q_UNLIKELY(!expStmt))
            throw ErrorInfo(Tr::tr("id: must be followed by identifier"));
        AST::IdentifierExpression *idExp =
                AST::cast<AST::IdentifierExpression *>(expStmt->expression);
        if (Q_UNLIKELY(!idExp || idExp->name.isEmpty()))
            throw ErrorInfo(Tr::tr("id: must be followed by identifier"));
        m_item->m_id = idExp->name.toString();
        ensureIdScope(m_file);
        m_file->m_idScope->m_properties[m_item->m_id] = ItemValue::create(m_item);
        return false;
    }

    JSSourceValuePtr value = JSSourceValue::create();
    value->setFile(m_file);
    m_sourceValue.swap(value);
    visitStatement(ast->statement);
    m_sourceValue.swap(value);

    Item *targetItem = targetItemForBinding(m_item, bindingName, value->location());
    checkDuplicateBinding(targetItem, bindingName, ast->qualifiedId->identifierToken);
    targetItem->m_properties.insert(bindingName.last(), value);
    return false;
}

bool ItemReaderASTVisitor::visit(AST::FunctionDeclaration *ast)
{
    FunctionDeclaration f;
    if (Q_UNLIKELY(ast->name.isNull()))
        throw ErrorInfo(Tr::tr("function decl without name"));
    f.setName(ast->name.toString());

    // remove the name
    QString funcNoName = textOf(m_sourceCode, ast);
    funcNoName.replace(QRegExp("^(\\s*function\\s*)\\w*"), "(\\1");
    funcNoName.append(")");
    f.setSourceCode(funcNoName);

    f.setLocation(toCodeLocation(ast->firstSourceLocation()));
    m_item->m_functions += f;
    return false;
}

bool ItemReaderASTVisitor::visitStatement(AST::Statement *statement)
{
    QBS_CHECK(statement);
    QBS_CHECK(m_sourceValue);

    QString sourceCode = textOf(m_sourceCode, statement);
    if (AST::cast<AST::Block *>(statement)) {
        // rewrite blocks to be able to use return statements in property assignments
        sourceCode.prepend("(function()");
        sourceCode.append(")()");
        m_sourceValue->m_hasFunctionForm = true;
    }

    m_sourceValue->setSourceCode(sourceCode);
    m_sourceValue->setLocation(toCodeLocation(statement->firstSourceLocation()));

    IdentifierSearch idsearch;
    idsearch.add(QLatin1String("base"), &m_sourceValue->m_sourceUsesBase);
    idsearch.add(QLatin1String("outer"), &m_sourceValue->m_sourceUsesOuter);
    idsearch.start(statement);
    return false;
}

CodeLocation ItemReaderASTVisitor::toCodeLocation(AST::SourceLocation location) const
{
    return CodeLocation(m_filePath, location.startLine, location.startColumn);
}

Item *ItemReaderASTVisitor::targetItemForBinding(Item *item,
                                                   const QStringList &bindingName,
                                                   const CodeLocation &bindingLocation)
{
    Item *targetItem = item;
    const int c = bindingName.count() - 1;
    for (int i = 0; i < c; ++i) {
        ValuePtr v = targetItem->m_properties.value(bindingName.at(i));
        if (!v) {
            Item *newItem = Item::create(m_reader->m_pool);
            v = ItemValue::create(newItem);
            targetItem->m_properties.insert(bindingName.at(i), v);
        }
        if (Q_UNLIKELY(v->type() != Value::ItemValueType)) {
            QString msg = Tr::tr("Binding to non-item property.");
            throw ErrorInfo(msg, bindingLocation);
        }
        ItemValuePtr jsv = v.staticCast<ItemValue>();
        targetItem = jsv->item();
    }
    return targetItem;
}

void ItemReaderASTVisitor::checkImportVersion(const AST::SourceLocation &versionToken) const
{
    if (!versionToken.length)
        return;
    const QString importVersionString = m_sourceCode.mid(versionToken.offset, versionToken.length);
    const ImportVersion importVersion
            = ImportVersion::fromString(importVersionString, toCodeLocation(versionToken));
    if (Q_UNLIKELY(importVersion != m_languageVersion))
        throw ErrorInfo(Tr::tr("Incompatible qbs version %1. This is qbs %2.").arg(
                        importVersionString, m_reader->builtins()->languageVersion()),
                    toCodeLocation(versionToken));
}

void ItemReaderASTVisitor::mergeItem(Item *dst, const Item *src,
                                     const ItemReaderResult &baseFile)
{
    if (!src->typeName().isEmpty())
        dst->setTypeName(src->typeName());

    int insertPos = 0;
    for (int i = 0; i < src->m_children.count(); ++i) {
        Item *child = src->m_children.at(i);
        dst->m_children.insert(insertPos++, child);
        child->m_parent = dst;
    }

    for (QMap<QString, ValuePtr>::const_iterator it = src->m_properties.constBegin();
         it != src->m_properties.constEnd(); ++it)
    {
        ValuePtr &v = dst->m_properties[it.key()];
        if (v) {
            if (v->type() == it.value()->type()) {
                if (v->type() == Value::JSSourceValueType) {
                    JSSourceValuePtr sv = v.staticCast<JSSourceValue>();
                    while (sv->baseValue())
                        sv = sv->baseValue();
                    const JSSourceValuePtr baseValue = it.value().staticCast<JSSourceValue>();
                    sv->setBaseValue(baseValue);
                    for (QList<JSSourceValue::Alternative>::iterator it
                            = sv->m_alternatives.begin(); it != sv->m_alternatives.end(); ++it) {
                        JSSourceValue::Alternative &alternative = *it;
                        alternative.value->setBaseValue(baseValue);
                    }
                } else if (v->type() == Value::ItemValueType) {
                    QBS_CHECK(v.staticCast<ItemValue>()->item());
                    QBS_CHECK(it.value().staticCast<const ItemValue>()->item());
                    mergeItem(v.staticCast<ItemValue>()->item(),
                              it.value().staticCast<const ItemValue>()->item(),
                              baseFile);
                } else {
                    QBS_CHECK(!"unexpected value type");
                }
            }
        } else {
            v = it.value();
        }
    }

    for (QMap<QString, PropertyDeclaration>::const_iterator it
            = src->m_propertyDeclarations.constBegin();
            it != src->m_propertyDeclarations.constEnd(); ++it) {
        dst->m_propertyDeclarations[it.key()] = it.value();
    }
    foreach (const JSSourceValuePtr &valueWithAlternatives,
            baseFile.conditionalValuesPerScopeItem.value(src)) {
        replaceConditionScopes(valueWithAlternatives, dst);
    }
}

void ItemReaderASTVisitor::ensureIdScope(const FileContextPtr &file)
{
    if (!file->m_idScope) {
        file->m_idScope = Item::create(m_reader->m_pool);
        file->m_idScope->m_typeName = QLatin1String("IdScope");
    }
}

void ItemReaderASTVisitor::setupAlternatives(Item *item)
{
    QList<Item *>::iterator it = item->m_children.begin();
    while (it != item->m_children.end()) {
        Item *child = *it;
        if (child->typeName() == QLatin1String("Properties")) {
            handlePropertiesBlock(item, child);
            it = item->m_children.erase(it);
        } else {
            ++it;
        }
    }
}

void ItemReaderASTVisitor::replaceConditionScopes(const JSSourceValuePtr &value,
                                                  Item *newScope)
{
    for (QList<JSSourceValue::Alternative>::iterator it
            = value->m_alternatives.begin(); it != value->m_alternatives.end(); ++it)
        it->conditionScopeItem = newScope;
}

class PropertiesBlockConverter
{
public:
    PropertiesBlockConverter(const QString &condition, Item *propertiesBlockContainer,
                             const Item *propertiesBlock,
                             QSet<JSSourceValuePtr> *valuesWithAlternatives)
        : m_propertiesBlockContainer(propertiesBlockContainer)
        , m_propertiesBlock(propertiesBlock)
        , m_valuesWithAlternatives(valuesWithAlternatives)
    {
        m_alternative.condition = condition;
        m_alternative.conditionScopeItem = propertiesBlockContainer;
    }

    void operator()()
    {
        apply(m_propertiesBlockContainer, m_propertiesBlock);
    }

private:
    JSSourceValue::Alternative m_alternative;
    Item *m_propertiesBlockContainer;
    const Item *m_propertiesBlock;
    QSet<JSSourceValuePtr> *m_valuesWithAlternatives;

    void apply(Item *a, const Item *b)
    {
        for (QMap<QString, ValuePtr>::const_iterator it = b->properties().constBegin();
                it != b->properties().constEnd(); ++it) {
            if (b == m_propertiesBlock && it.key() == QLatin1String("condition"))
                continue;
            if (it.value()->type() == Value::ItemValueType) {
                apply(a->itemProperty(it.key(), true)->item(),
                      it.value().staticCast<ItemValue>()->item());
            } else if (it.value()->type() == Value::JSSourceValueType) {
                ValuePtr aval = a->property(it.key());
                if (Q_UNLIKELY(aval && aval->type() != Value::JSSourceValueType))
                    throw ErrorInfo(Tr::tr("Incompatible value type in unconditional value at %1.").arg(
                                    aval->location().toString()));
                apply(it.key(), a, aval.staticCast<JSSourceValue>(),
                      it.value().staticCast<JSSourceValue>());
            } else {
                QBS_CHECK(!"Unexpected value type in conditional value.");
            }
        }
    }

    void apply(const QString &propertyName, Item *item, JSSourceValuePtr value,
               const JSSourceValuePtr &conditionalValue)
    {
        QBS_ASSERT(!value || value->file() == conditionalValue->file(), return);
        if (!value) {
            value = JSSourceValue::create();
            value->setFile(conditionalValue->file());
            item->setProperty(propertyName, value);
            value->setSourceCode(QLatin1String("undefined"));
        }
        m_alternative.value = conditionalValue;
        value->addAlternative(m_alternative);
        m_valuesWithAlternatives->insert(value);
    }
};

void ItemReaderASTVisitor::handlePropertiesBlock(Item *item, const Item *block)
{
    ValuePtr value = block->property(QLatin1String("condition"));
    if (Q_UNLIKELY(!value))
        throw ErrorInfo(Tr::tr("Properties.condition must be provided."),
                    block->location());
    if (Q_UNLIKELY(value->type() != Value::JSSourceValueType))
        throw ErrorInfo(Tr::tr("Properties.condition must be a value binding."),
                    block->location());
    JSSourceValuePtr srcval = value.staticCast<JSSourceValue>();
    const QString condition = srcval->sourceCode();
    PropertiesBlockConverter convertBlock(condition, item, block,
                                          &m_readerResult->conditionalValuesPerScopeItem[item]);
    convertBlock();
}

} // namespace Internal
} // namespace qbs
