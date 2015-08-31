/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
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

#ifndef PROJECTRESOLVER_H
#define PROJECTRESOLVER_H

#include "filetags.h"
#include "moduleloader.h"

#include <logging/logger.h>
#include <tools/setupprojectparameters.h>

#include <QMap>
#include <QSet>

namespace qbs {
namespace Internal {

class Evaluator;
class Item;
class ProgressObserver;
class ScriptEngine;
class QualifiedIdSet;

class ProjectResolver
{
public:
    ProjectResolver(ModuleLoader *ldr, const Logger &logger);
    ~ProjectResolver();

    void setProgressObserver(ProgressObserver *observer);
    TopLevelProjectPtr resolve(ModuleLoaderResult &loadResult,
                               const SetupProjectParameters &setupParameters);

    static void applyFileTaggers(const SourceArtifactPtr &artifact,
            const ResolvedProductConstPtr &product, const Logger &logger);
    static SourceArtifactPtr createSourceArtifact(const ResolvedProductConstPtr &rproduct,
            const PropertyMapPtr &properties, const QString &fileName,
            const FileTags &fileTags, bool overrideTags, QList<SourceArtifactPtr> &artifactList);

private:
    struct ProjectContext
    {
        ResolvedProjectPtr project;
        QList<FileTaggerConstPtr> fileTaggers;
        ModuleLoaderResult *loadResult;
        QList<RulePtr> rules;
        ResolvedModulePtr dummyModule;
    };

    struct ProductContext
    {
        ResolvedProductPtr product;
        QString buildDirectory;
        FileTags additionalFileTags;
        Item *item;
        typedef QPair<ArtifactPropertiesPtr, CodeLocation> ArtifactPropertiesInfo;
        QHash<QStringList, ArtifactPropertiesInfo> artifactPropertiesPerFilter;
        QHash<QString, CodeLocation> sourceArtifactLocations;
    };

    struct ModuleContext
    {
        ResolvedModulePtr module;
    };

    void checkCancelation() const;
    QString verbatimValue(const ValueConstPtr &value, bool *propertyWasSet = 0) const;
    QString verbatimValue(Item *item, const QString &name, bool *propertyWasSet = 0) const;
    ScriptFunctionPtr scriptFunctionValue(Item *item, const QString &name) const;
    ResolvedFileContextPtr resolvedFileContext(const FileContextConstPtr &ctx) const;
    void ignoreItem(Item *item, ProjectContext *projectContext);
    void resolveTopLevelProject(Item *item, ProjectContext *projectContext);
    void resolveProject(Item *item, ProjectContext *projectContext);
    void resolveSubProject(Item *item, ProjectContext *projectContext);
    void resolveProduct(Item *item, ProjectContext *projectContext);
    void resolveModules(const Item *item, ProjectContext *projectContext);
    void resolveModule(const QualifiedId &moduleName, Item *item, bool isProduct,
                       ProjectContext *projectContext);
    void resolveGroup(Item *item, ProjectContext *projectContext);
    void resolveRule(Item *item, ProjectContext *projectContext);
    void resolveRuleArtifact(const RulePtr &rule, Item *item);
    static void resolveRuleArtifactBinding(const RuleArtifactPtr &ruleArtifact, Item *item,
                                           const QStringList &namePrefix,
                                           QualifiedIdSet *seenBindings);
    void resolveFileTagger(Item *item, ProjectContext *projectContext);
    void resolveTransformer(Item *item, ProjectContext *projectContext);
    void resolveScanner(Item *item, ProjectContext *projectContext);
    void resolveProductDependencies(ProjectContext *projectContext);
    void postProcess(const ResolvedProductPtr &product, ProjectContext *projectContext) const;
    void applyFileTaggers(const ResolvedProductPtr &product) const;
    QVariantMap evaluateModuleValues(Item *item, bool lookupPrototype = true) const;

    struct EvalResult {
        EvalResult(const QVariant &v, bool s) : value(v), strongPrecedence(s) {}
        EvalResult() : strongPrecedence(false) {}
        QVariant value;
        bool strongPrecedence;
    };

    QVariantMap evaluateProperties(Item *item, bool lookupPrototype = true) const;
    QVariantMap evaluateProperties(Item *item, Item *propertiesContainer, const QVariantMap &tmplt,
            bool lookupPrototype = true) const;
    QVariantMap createProductConfig() const;
    QString convertPathProperty(const QString &path, const QString &dirPath) const;
    QStringList convertPathListProperty(const QStringList &paths, const QString &dirPath) const;
    ProjectContext createProjectContext(ProjectContext *parentProjectContext) const;
    QList<ResolvedProductPtr> getProductDependencies(const ResolvedProductConstPtr &product,
            ModuleLoaderResult::ProductInfo *productInfo);
    static void matchArtifactProperties(const ResolvedProductPtr &product,
            const QList<SourceArtifactPtr> &artifacts);

    Evaluator *m_evaluator;
    Logger m_logger;
    ScriptEngine *m_engine;
    ProgressObserver *m_progressObserver;
    ProductContext *m_productContext;
    ModuleContext *m_moduleContext;
    QMap<QString, ResolvedProductPtr> m_productsByName;
    QHash<QString, QList<ResolvedProductPtr> > m_productsByType;
    QHash<ResolvedProductPtr, Item *> m_productItemMap;
    mutable QHash<FileContextConstPtr, ResolvedFileContextPtr> m_fileContextMap;
    SetupProjectParameters m_setupParams;

    typedef void (ProjectResolver::*ItemFuncPtr)(Item *item, ProjectContext *projectContext);
    typedef QMap<QByteArray, ItemFuncPtr> ItemFuncMap;
    void callItemFunction(const ItemFuncMap &mappings, Item *item, ProjectContext *projectContext);
};

} // namespace Internal
} // namespace qbs

#endif // PROJECTRESOLVER_H
