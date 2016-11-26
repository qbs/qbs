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
    void baseValidation();
    void buildConfigStringListSyntax();
    void builtinFunctionInSearchPathsProperty();
    void canonicalArchitecture();
    void conditionalDepends();
    void dependencyOnAllProfiles();
    void derivedSubProject();
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
    void idUniqueness();
    void importCollection();
    void invalidBindingInDisabledItem();
    void itemPrototype();
    void itemScope();
    void jsExtensions();
    void jsImportUsedInMultipleScopes_data();
    void jsImportUsedInMultipleScopes();
    void moduleProperties_data();
    void moduleProperties();
    void modulePropertiesInGroups();
    void moduleScope();
    void modules_data();
    void modules();
    void nonRequiredProducts();
    void nonRequiredProducts_data();
    void outerInGroup();
    void pathProperties();
    void productConditions();
    void productDirectories();
    void profileValuesAndOverriddenValues();
    void propertiesBlocks_data();
    void propertiesBlocks();
    void propertiesBlockInGroup();
    void qbsPropertiesInProjectCondition();
    void relaxedErrorMode();
    void relaxedErrorMode_data();
    void requiredAndNonRequiredDependencies();
    void requiredAndNonRequiredDependencies_data();
    void throwingProbe();
    void throwingProbe_data();
    void defaultValue();
    void defaultValue_data();
    void qualifiedId();
    void recursiveProductDependencies();
    void rfc1034Identifier();
    void versionCompare();
    void wildcards_data();
    void wildcards();
};

} // namespace Internal
} // namespace qbs

#endif // TST_LANGUAGE_H
