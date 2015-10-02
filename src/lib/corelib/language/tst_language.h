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

#ifndef TST_LANGUAGE_H
#define TST_LANGUAGE_H

#include <language/forward_decls.h>
#include <language/loader.h>
#include <logging/ilogsink.h>
#include <tools/setupprojectparameters.h>
#include <tools/qbs_export.h>
#include <QtTest>

namespace qbs {
namespace Internal {

class QBS_EXPORT TestLanguage : public QObject
{
    Q_OBJECT
public:
    TestLanguage(ILogSink *logSink);
    ~TestLanguage();

private:
    ILogSink *m_logSink;
    Logger m_logger;
    ScriptEngine *m_engine;
    Loader *loader;
    TopLevelProjectPtr project;
    SetupProjectParameters defaultParameters;
    const QString m_wildcardsTestDirPath;

    QHash<QString, ResolvedProductPtr> productsFromProject(ResolvedProjectPtr project);
    ResolvedModuleConstPtr findModuleByName(ResolvedProductPtr product, const QString &name);
    QVariant productPropertyValue(ResolvedProductPtr product, QString propertyName);
    void handleInitCleanupDataTags(const char *projectFileName, bool *handled);

private slots:
    void initTestCase();
    void cleanupTestCase();

    void baseProperty();
    void buildConfigStringListSyntax();
    void builtinFunctionInSearchPathsProperty();
    void canonicalArchitecture();
    void conditionalDepends();
    void dependencyOnAllProfiles();
    void environmentVariable();
    void erroneousFiles_data();
    void erroneousFiles();
    void exports();
    void fileContextProperties();
    void fileTags_data();
    void fileTags();
    void groupConditions_data();
    void groupConditions();
    void groupName();
    void getNativeSetting();
    void homeDirectory();
    void identifierSearch_data();
    void identifierSearch();
    void idUsage();
    void importCollection();
    void invalidBindingInDisabledItem();
    void itemPrototype();
    void itemScope();
    void jsExtensions();
    void jsImportUsedInMultipleScopes_data();
    void jsImportUsedInMultipleScopes();
    void moduleProperties_data();
    void moduleProperties();
    void moduleScope();
    void modules_data();
    void modules();
    void outerInGroup();
    void pathProperties();
    void productConditions();
    void productDirectories();
    void profileValuesAndOverriddenValues();
    void propertiesBlocks_data();
    void propertiesBlocks();
    void propertiesBlockInGroup();
    void qbsPropertiesInProjectCondition();
    void defaultValue();
    void defaultValue_data();
    void qualifiedId();
    void recursiveProductDependencies();
    void rfc1034Identifier();
    void wildcards_data();
    void wildcards();
};

} // namespace Internal
} // namespace qbs

#endif // TST_LANGUAGE_H
