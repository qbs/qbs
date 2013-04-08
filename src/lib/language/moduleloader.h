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
namespace Internal {

class BuiltinDeclarations;
class Evaluator;
class ItemReader;
class ProgressObserver;
class ScriptEngine;

struct ModuleLoaderResult
{
    struct ProductInfo
    {
        struct Dependency
        {
            QString name;
            bool required;
            QString failureMessage;
        };

        QList<Dependency> usedProducts;
        QList<Dependency> usedProductsFromProductModule;
    };

    ItemPtr root;
    QHash<ItemPtr, ProductInfo> productInfos;
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

    ModuleLoaderResult load(const QString &filePath, const QVariantMap &userProperties,
                            bool wrapWithProjectItem = false);

    static QString fullModuleName(const QStringList &moduleName);

private:
    class ContextBase
    {
    public:
        ItemPtr item;
        ItemPtr scope;
        QStringList extraSearchPaths;
        QMap<QString, ItemPtr> moduleItemCache;
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
        QSet<FileContextConstPtr> filesWithProductModule;
    };

    class DependsContext
    {
    public:
        ProductContext *product;
        QList<ModuleLoaderResult::ProductInfo::Dependency> *productDependencies;
    };

    typedef QPair<ItemPtr, ModuleLoaderResult::ProductInfo::Dependency> ProductDependencyResult;
    typedef QList<ProductDependencyResult> ProductDependencyResults;

    void handleProject(ModuleLoaderResult *loadResult, const ItemPtr &item);
    void handleProduct(ProjectContext *projectContext, const ItemPtr &item);
    void createAdditionalModuleInstancesInProduct(ProductContext *productContext);
    void handleGroup(ProductContext *productContext, const ItemPtr &group);
    void handleArtifact(ProductContext *productContext, const ItemPtr &item);
    void handleProductModule(ProductContext *productContext, const ItemPtr &item);
    void propagateModulesFromProduct(ProductContext *productContext, const ItemPtr &item);
    void resolveDependencies(DependsContext *productContext, const ItemPtr &item);
    class ItemModuleList;
    void resolveDependsItem(DependsContext *dependsContext, const ItemPtr &item, const ItemPtr &dependsItem, ItemModuleList *moduleResults, ProductDependencyResults *productResults);
    static ItemPtr moduleInstanceItem(const ItemPtr &item, const QStringList &moduleName);
    ItemPtr loadModule(ProductContext *productContext, const ItemPtr &item, const QString &moduleId, const QStringList &moduleName);
    ItemPtr searchAndLoadModuleFile(ProductContext *productContext, const QStringList &moduleName, const QStringList &extraSearchPaths);
    ItemPtr loadModuleFile(ProductContext *productContext, bool isBaseModule, const QString &filePath);
    void instantiateModule(ProductContext *productContext, const ItemPtr &instanceScope, const ItemPtr &moduleInstance, const ItemPtr &modulePrototype, const QStringList &moduleName);
    void createChildInstances(ProductContext *productContext, const ItemPtr &instance,
                              const ItemPtr &prototype, QHash<ItemPtr, ItemPtr> *prototypeInstanceMap) const;
    void resolveProbes(const ItemPtr &item);
    void resolveProbe(const ItemPtr &parent, const ItemPtr &probe);
    void checkCancelation() const;
    bool checkItemCondition(const ItemPtr &item);
    QStringList readExtraSearchPaths(const ItemPtr &item);
    static ItemPtr wrapWithProject(const ItemPtr &item);
    static QString moduleSubDir(const QStringList &moduleName);
    static void copyProperty(const QString &propertyName, const ItemConstPtr &source, const ItemPtr &destination);
    static void setScopeForDescendants(const ItemPtr &item, const ItemPtr &scope);

    ScriptEngine *m_engine;
    Logger m_logger;
    ProgressObserver *m_progressObserver;
    ItemReader *m_reader;
    Evaluator *m_evaluator;
    QStringList m_moduleSearchPaths;
    QMap<QString, QStringList> m_moduleDirListCache;
    QVariantMap m_userProperties;
};

} // namespace Internal
} // namespace qbs

QT_BEGIN_NAMESPACE
Q_DECLARE_TYPEINFO(qbs::Internal::ModuleLoaderResult::ProductInfo, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(qbs::Internal::ModuleLoaderResult::ProductInfo::Dependency, Q_MOVABLE_TYPE);
QT_END_NAMESPACE

#endif // QBS_MODULELOADER_H
