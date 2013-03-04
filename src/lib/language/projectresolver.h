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

#ifndef PROJECTRESOLVER_H
#define PROJECTRESOLVER_H

#include "evaluator.h"
#include "filetags.h"
#include "language.h"
#include <logging/logger.h>

#include <QMap>
#include <QSet>

namespace qbs {
namespace Internal {

class BuiltinDeclarations;
class Evaluator;
class ModuleLoader;
class ProgressObserver;
class ScriptEngine;
struct ModuleLoaderResult;

class ProjectResolver
{
public:
    ProjectResolver(ModuleLoader *ldr, const BuiltinDeclarations *builtins, const Logger &logger);
    ~ProjectResolver();

    void setProgressObserver(ProgressObserver *observer);
    ResolvedProjectPtr resolve(ModuleLoaderResult &loadResult, const QString &buildRoot, const QVariantMap &buildConfiguration);

private:
    struct ProjectContext
    {
        ResolvedProjectPtr project;
        QSet<FileTaggerConstPtr> fileTaggers;
        ModuleLoaderResult *loadResult;
        QList<RulePtr> rules;
        ResolvedModulePtr dummyModule;
        QMap<QString, ResolvedProductPtr> productsByName;
        QHash<ResolvedProductPtr, ItemPtr> productItemMap;
        QMap<QString, QVariantMap> productModules;
    };

    struct ProductContext
    {
        ResolvedProductPtr product;
        ItemPtr item;
    };

    struct ModuleContext
    {
        ResolvedModulePtr module;
    };

    void checkCancelation() const;
    bool boolValue(const ItemConstPtr &item, const QString &name, bool defaultValue = false) const;
    FileTags fileTagsValue(const ItemConstPtr &item, const QString &name) const;
    QString stringValue(const ItemConstPtr &item, const QString &name, const QString &defaultValue = QString()) const;
    QStringList stringListValue(const ItemConstPtr &item, const QString &name) const;
    QString verbatimValue(const ValueConstPtr &value) const;
    QString verbatimValue(const ItemPtr &item, const QString &name) const;
    void ignoreItem(const ItemPtr &item);
    void resolveProject(const ItemPtr &item);
    void resolveProduct(const ItemPtr &item);
    void resolveModule(const QStringList &moduleName, const ItemPtr &item);
    void resolveGroup(const ItemPtr &item);
    void resolveRule(const ItemPtr &item);
    void resolveRuleArtifact(const RulePtr &rule, const ItemPtr &item, bool *hasAlwaysUpdatedArtifact);
    static void resolveRuleArtifactBinding(const RuleArtifactPtr &ruleArtifact, const ItemPtr &item,
                                           const QStringList &namePrefix);
    void resolveFileTagger(const ItemPtr &item);
    void resolveTransformer(const ItemPtr &item);
    void resolveProductModule(const ItemPtr &item);
    void resolveProductDependencies();
    void postProcess(const ResolvedProductPtr &product) const;
    void applyFileTaggers(const ResolvedProductPtr &product) const;
    void applyFileTaggers(const SourceArtifactPtr &artifact,
                          const ResolvedProductConstPtr &product) const;
    QVariantMap evaluateModuleValues(const ItemPtr &item) const;
    void evaluateModuleValues(const ItemPtr &item, QVariantMap *modulesMap) const;
    QVariantMap evaluateProperties(const ItemPtr &item) const;
    QVariantMap evaluateProperties(const ItemPtr &item,
                                       const ItemPtr &propertiesContainer,
                                       const QVariantMap &tmplt) const;
    QVariantMap createProductConfig() const;
    QString convertPathProperty(const QString &path, const QString &dirPath) const;
    QStringList convertPathListProperty(const QStringList &paths, const QString &dirPath) const;

    ModuleLoader *m_moduleLoader;
    const BuiltinDeclarations *m_builtins;
    Evaluator *m_evaluator;
    Logger m_logger;
    ScriptEngine *m_engine;
    ProgressObserver *m_progressObserver;
    QString m_buildRoot;
    QVariantMap m_buildConfiguration;
    ProjectContext *m_projectContext;
    ProductContext *m_productContext;
    ModuleContext *m_moduleContext;
    QSet<QString> m_groupPropertyDeclarations;

    typedef void (ProjectResolver::*ItemFuncPtr)(const ItemPtr &item);
    typedef QMap<QByteArray, ItemFuncPtr> ItemFuncMap;
    void callItemFunction(const ItemFuncMap &mappings, const ItemPtr &item);
};

} // namespace Internal
} // namespace qbs

#endif // PROJECTRESOLVER_H
