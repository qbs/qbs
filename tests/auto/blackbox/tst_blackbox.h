/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef TST_BLACKBOX_H
#define TST_BLACKBOX_H

#include "tst_blackboxbase.h"

class TestBlackbox : public TestBlackboxBase
{
    Q_OBJECT

public:
    TestBlackbox();

private slots:
    void alwaysRun();
    void alwaysRun_data();
    void artifactScanning();
    void assembly();
    void auxiliaryInputsFromDependencies();
    void badInterpreter();
    void buildDirectories();
    void buildEnvChange();
    void buildGraphVersions();
    void changedFiles_data();
    void changedFiles();
    void changeInDisabledProduct();
    void changeInImportedFile();
    void changeTrackingAndMultiplexing();
    void checkProjectFilePath();
    void checkTimestamps();
    void clean();
    void cli();
    void combinedSources();
    void commandFile();
    void concurrentExecutor();
    void conditionalExport();
    void conditionalFileTagger();
    void conflictingArtifacts();
    void cxxLanguageVersion();
    void cxxLanguageVersion_data();
    void dependenciesProperty();
    void dependencyProfileMismatch();
    void deprecatedProperty();
    void disappearedProfile();
    void dynamicMultiplexRule();
    void dynamicProject();
    void dynamicRuleOutputs();
    void enableExceptions();
    void enableExceptions_data();
    void enableRtti();
    void erroneousFiles_data();
    void erroneousFiles();
    void errorInfo();
    void escapedLinkerFlags();
    void exportRule();
    void exportToOutsideSearchPath();
    void externalLibs();
    void fileDependencies();
    void generatedArtifactAsInputToDynamicRule();
    void groupsInModules();
    void importInPropertiesCondition();
    void importingProduct();
    void importsConflict();
    void innoSetup();
    void inputsFromDependencies();
    void installable();
    void installedApp();
    void installDuplicates();
    void installedSourceFiles();
    void installedTransformerOutput();
    void installPackage();
    void installRootFromProjectFile();
    void installTree();
    void invalidCommandProperty();
    void invalidExtensionInstantiation();
    void invalidExtensionInstantiation_data();
    void invalidInstallDir();
    void invalidLibraryNames();
    void invalidLibraryNames_data();
    void jsExtensionsFile();
    void jsExtensionsFileInfo();
    void jsExtensionsProcess();
    void jsExtensionsPropertyList();
    void jsExtensionsTemporaryDir();
    void jsExtensionsTextFile();
    void ld();
    void linkerMode();
    void lexyacc();
    void linkerScripts();
    void listPropertiesWithOuter();
    void listPropertyOrder();
    void loadableModule();
    void missingDependency();
    void missingProjectFile();
    void missingOverridePrefix();
    void multipleChanges();
    void nestedGroups();
    void nestedProperties();
    void newOutputArtifact();
    void nodejs();
    void nonBrokenFilesInBrokenProduct();
    void nonDefaultProduct();
    void nsis();
    void outputArtifactAutoTagging();
    void overrideProjectProperties();
    void pchChangeTracking();
    void perGroupDefineInExportItem();
    void pkgConfigProbe();
    void pkgConfigProbe_data();
    void pkgConfigProbeSysroot();
    void pluginDependency();
    void probeChangeTracking();
    void probeProperties();
    void probeInExportedModule();
    void probesAndArrayProperties();
    void probesInNestedModules();
    void productDependenciesByType();
    void productProperties();
    void propertyAssignmentOnNonPresentModule();
    void propertyChanges();
    void propertyPrecedence();
    void properQuoting();
    void propertiesInExportItems();
    void qbsVersion();
    void qtBug51237();
    void radAfterIncompleteBuild();
    void radAfterIncompleteBuild_data();
    void recursiveRenaming();
    void recursiveWildcards();
    void referenceErrorInExport();
    void reproducibleBuild();
    void reproducibleBuild_data();
    void require();
    void requireDeprecated();
    void rescueTransformerData();
    void responseFiles();
    void ruleConditions();
    void ruleCycle();
    void ruleWithNoInputs();
    void ruleWithNonRequiredInputs();
    void smartRelinking();
    void smartRelinking_data();
    void soVersion();
    void soVersion_data();
    void subProfileChangeTracking();
    void successiveChanges();
    void symbolLinkMode();
    void symlinkRemoval();
    void renameDependency();
    void separateDebugInfo();
    void sevenZip();
    void staticLibWithoutSources();
    void suspiciousCalls();
    void suspiciousCalls_data();
    void systemRunPaths();
    void systemRunPaths_data();
    void tar();
    void toolLookup();
    void topLevelSearchPath();
    void trackAddFile();
    void trackAddFileTag();
    void trackAddProduct();
    void trackExternalProductChanges();
    void trackGroupConditionChange();
    void trackRemoveFile();
    void trackRemoveFileTag();
    void trackRemoveProduct();
    void transitiveOptionalDependencies();
    void typescript();
    void usingsAsSoleInputsNonMultiplexed();
    void versionCheck();
    void versionCheck_data();
    void versionScript();
    void wholeArchive();
    void wholeArchive_data();
    void wildCardsAndRules();
    void wildcardRenaming();
    void wix();
    void zip();
    void zip_data();
    void zipInvalid();

private:
    QMap<QString, QString> findCli(int *status);
    QMap<QString, QString> findNodejs(int *status);
    QMap<QString, QString> findTypeScript(int *status);
    QString findArchiver(const QString &fileName, int *status = nullptr);
};

#endif // TST_BLACKBOX_H
