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

#include <QtCore/qrandom.h>
#include <QtCore/qtemporarydir.h>
#include <QtTest/qtest.h>

class TestLanguage : public QObject
{
    Q_OBJECT
public:
    TestLanguage(qbs::ILogSink *logSink, qbs::Settings *settings);
    ~TestLanguage();

private:
    qbs::ILogSink *m_logSink;
    qbs::Settings * const m_settings;
    qbs::Internal::Logger m_logger;
    qbs::Internal::ScriptEngine *m_engine;
    qbs::Internal::Loader *loader;
    qbs::Internal::TopLevelProjectPtr project;
    qbs::SetupProjectParameters defaultParameters;
    const QString m_wildcardsTestDirPath;

    QHash<QString, qbs::Internal::ResolvedProductPtr> productsFromProject(
            qbs::Internal::ResolvedProjectPtr project);
    qbs::Internal::ResolvedModuleConstPtr findModuleByName(
            qbs::Internal::ResolvedProductPtr product, const QString &name);
    QVariant productPropertyValue(qbs::Internal::ResolvedProductPtr product, QString propertyName);
    void handleInitCleanupDataTags(const char *projectFileName, bool *handled);

private slots:
    void init();
    void initTestCase();
    void cleanupTestCase();

    void additionalProductTypes();
    void baseProperty();
    void baseValidation();
    void brokenDependencyCycle();
    void brokenDependencyCycle_data();
    void buildConfigStringListSyntax();
    void builtinFunctionInSearchPathsProperty();
    void chainedProbes();
    void canonicalArchitecture();
    void conditionalDepends();
    void delayedError();
    void delayedError_data();
    void dependencyOnAllProfiles();
    void derivedSubProject();
    void disabledSubProject();
    void dottedNames_data();
    void dottedNames();
    void emptyJsFile();
    void enumerateProjectProperties();
    void evalErrorInNonPresentModule_data();
    void evalErrorInNonPresentModule();
    void environmentVariable();
    void errorInDisabledProduct();
    void erroneousFiles_data();
    void erroneousFiles();
    void exports();
    void fileContextProperties();
    void fileInProductAndModule_data();
    void fileInProductAndModule();
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
    void inheritedPropertiesItems_data();
    void inheritedPropertiesItems();
    void invalidBindingInDisabledItem();
    void invalidOverrides();
    void invalidOverrides_data();
    void itemPrototype();
    void itemScope();
    void jsExtensions();
    void jsImportUsedInMultipleScopes_data();
    void jsImportUsedInMultipleScopes();
    void moduleMergingVariantValues();
    void modulePrioritizationBySearchPath_data();
    void modulePrioritizationBySearchPath();
    void moduleProperties_data();
    void moduleProperties();
    void modulePropertiesInGroups();
    void modulePropertyOverridesPerProduct();
    void moduleScope();
    void modules_data();
    void modules();
    void multiplexedExports();
    void multiplexingByProfile();
    void multiplexingByProfile_data();
    void nonApplicableModulePropertyInProfile();
    void nonApplicableModulePropertyInProfile_data();
    void nonRequiredProducts();
    void nonRequiredProducts_data();
    void outerInGroup();
    void overriddenPropertiesAndPrototypes();
    void overriddenPropertiesAndPrototypes_data();
    void overriddenVariantProperty();
    void parameterTypes();
    void pathProperties();
    void productConditions();
    void productDirectories();
    void profileValuesAndOverriddenValues();
    void projectFileLookup();
    void projectFileLookup_data();
    void propertiesBlocks_data();
    void propertiesBlocks();
    void propertiesBlockInGroup();
    void propertiesItemInModule();
    void propertyAssignmentInExportedGroup();
    void qbs1275();
    void qbsPropertiesInProjectCondition();
    void qbsPropertyConvenienceOverride();
    void relaxedErrorMode();
    void relaxedErrorMode_data();
    void requiredAndNonRequiredDependencies();
    void requiredAndNonRequiredDependencies_data();
    void suppressedAndNonSuppressedErrors();
    void throwingProbe();
    void throwingProbe_data();
    void defaultValue();
    void defaultValue_data();
    void qualifiedId();
    void recursiveProductDependencies();
    void rfc1034Identifier();
    void useInternalProfile();
    void versionCompare();
    void wildcards_data();
    void wildcards();

private:
    QTemporaryDir m_tempDir;
    QRandomGenerator m_rand;
};

#endif // TST_LANGUAGE_H

