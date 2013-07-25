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

#ifndef QBS_MODULELOADER_H
#define QBS_MODULELOADER_H

#include "forward_decls.h"
#include "itempool.h"
#include <logging/logger.h>

#include <QMap>
#include <QSet>
#include <QStringList>
#include <QVariantMap>

QT_BEGIN_NAMESPACE
class QScriptContext;
class QScriptEngine;
QT_END_NAMESPACE

namespace qbs {

class CodeLocation;

namespace Internal {

class BuiltinDeclarations;
class Evaluator;
class Item;
class ItemReader;
class ProgressObserver;
class ScriptEngine;

struct ModuleLoaderResult
{
    ModuleLoaderResult()
        : itemPool(new ItemPool), root(0)
    {}

    struct ProductInfo
    {
        struct Dependency
        {
            QString name;
            bool required;
            QString failureMessage;
        };

        QList<Dependency> usedProducts;
        QList<Dependency> usedProductsFromExportItem;
    };

    QSharedPointer<ItemPool> itemPool;
    Item *root;
    QHash<Item *, ProductInfo> productInfos;
    QSet<QString> qbsFiles;
};

/*
 * Loader stage II. Responsible for
 *      - loading modules and module dependencies,
 *      - project references,
 *      - Probe items.
 */
class ModuleLoader
{
public:
    ModuleLoader(ScriptEngine *engine, BuiltinDeclarations *builtins, const Logger &logger);
    ~ModuleLoader();

    void setProgressObserver(ProgressObserver *progressObserver);
    void setSearchPaths(const QStringList &searchPaths);
    Evaluator *evaluator() const { return m_evaluator; }

    ModuleLoaderResult load(const QString &filePath,
            const QVariantMap &overriddenProperties, const QVariantMap &buildConfigProperties,
            bool wrapWithProjectItem = false);

    static QString fullModuleName(const QStringList &moduleName);
    static void overrideItemProperties(Item *item, const QString &buildConfigKey,
            const QVariantMap &buildConfig);

private:
    class ContextBase
    {
    public:
        ContextBase()
            : item(0), scope(0)
        {}

        Item *item;
        Item *scope;
        QStringList extraSearchPaths;
        QMap<QString, Item *> moduleItemCache;
    };

    class ProjectContext : public ContextBase
    {
    public:
        ModuleLoaderResult *result;
    };

    class ProductContext : public ContextBase
    {
    public:
        ProjectContext *project;
        ModuleLoaderResult::ProductInfo info;
        QSet<FileContextConstPtr> filesWithExportItem;
        QList<Item *> exportItems;
    };

    class DependsContext
    {
    public:
        ProductContext *product;
        QList<ModuleLoaderResult::ProductInfo::Dependency> *productDependencies;
    };

    typedef QPair<Item *, ModuleLoaderResult::ProductInfo::Dependency> ProductDependencyResult;
    typedef QList<ProductDependencyResult> ProductDependencyResults;

    void handleProject(ModuleLoaderResult *loadResult, Item *item,
            const QSet<QString> &referencedFilePaths);
    void handleProduct(ProjectContext *projectContext, Item *item);
    void handleSubProject(ProjectContext *projectContext, Item *item,
            const QSet<QString> &referencedFilePaths);
    void createAdditionalModuleInstancesInProduct(ProductContext *productContext);
    void handleGroup(ProductContext *productContext, Item *group);
    void handleArtifact(ProductContext *productContext, Item *item);
    void deferExportItem(ProductContext *productContext, Item *item);
    void handleProductModule(ProductContext *productContext, Item *item);   // ### remove in 1.2
    void mergeExportItems(ProductContext *productContext);
    void propagateModulesFromProduct(ProductContext *productContext, Item *item);
    void resolveDependencies(DependsContext *productContext, Item *item);
    class ItemModuleList;
    void resolveDependsItem(DependsContext *dependsContext, Item *item, Item *dependsItem, ItemModuleList *moduleResults, ProductDependencyResults *productResults);
    Item *moduleInstanceItem(Item *item, const QStringList &moduleName);
    Item *loadModule(ProductContext *productContext, Item *item,
            const CodeLocation &dependsItemLocation, const QString &moduleId, const QStringList &moduleName);
    Item *searchAndLoadModuleFile(ProductContext *productContext,
            const CodeLocation &dependsItemLocation, const QStringList &moduleName,
            const QStringList &extraSearchPaths);
    Item *loadModuleFile(ProductContext *productContext, const QString &fullModuleName, bool isBaseModule, const QString &filePath);
    void loadBaseModule(ProductContext *productContext, Item *item);
    void instantiateModule(ProductContext *productContext, Item *instanceScope, Item *moduleInstance, Item *modulePrototype, const QStringList &moduleName);
    void createChildInstances(ProductContext *productContext, Item *instance,
                              Item *prototype, QHash<Item *, Item *> *prototypeInstanceMap) const;
    void resolveProbes(Item *item);
    void resolveProbe(Item *parent, Item *probe);
    void checkCancelation() const;
    bool checkItemCondition(Item *item);
    QStringList readExtraSearchPaths(Item *item);
    void copyProperties(const Item *sourceProject, Item *targetProject);
    Item *wrapWithProject(Item *item);
    static QString findExistingModulePath(const QString &searchPath,
            const QStringList &moduleName);
    static void copyProperty(const QString &propertyName, const Item *source, Item *destination);
    static void setScopeForDescendants(Item *item, Item *scope);

    ScriptEngine *m_engine;
    ItemPool *m_pool;
    Logger m_logger;
    ProgressObserver *m_progressObserver;
    ItemReader *m_reader;
    Evaluator *m_evaluator;
    QStringList m_moduleSearchPaths;
    QMap<QString, QStringList> m_moduleDirListCache;
    QVariantMap m_overriddenProperties;
    QVariantMap m_buildConfigProperties;
};

} // namespace Internal
} // namespace qbs

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(qbs::Internal::ModuleLoaderResult::ProductInfo, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(qbs::Internal::ModuleLoaderResult::ProductInfo::Dependency, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

#endif // QBS_MODULELOADER_H
