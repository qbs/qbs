/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2019 Jochen Ulrich <jochenulrich@t-online.de>
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
    void addFileTagToGeneratedArtifact();
    void alwaysRun();
    void alwaysRun_data();
    void artifactsMapChangeTracking();
    void artifactsMapInvalidation();
    void artifactsMapRaceCondition();
    void artifactScanning();
    void assembly();
    void autotestWithDependencies();
    void autotestTimeout();
    void autotestTimeout_data();
    void autotests_data();
    void autotests();
    void auxiliaryInputsFromDependencies();
    void badInterpreter();
    void bomSources();
    void buildDataOfDisabledProduct();
    void buildDirectories();
    void buildEnvChange();
    void buildGraphVersions();
    void buildVariantDefaults_data();
    void buildVariantDefaults();
    void changedFiles_data();
    void changedFiles();
    void changedInputsFromDependencies();
    void changedRuleInputs();
    void changeInDisabledProduct();
    void changeInImportedFile();
    void changeTrackingAndMultiplexing();
    void checkProjectFilePath();
    void checkTimestamps();
    void chooseModuleInstanceByPriority();
    void chooseModuleInstanceByPriority_data();
    void clean();
    void cli();
    void combinedSources();
    void commandFile();
    void compilerDefinesByLanguage();
    void concurrentExecutor();
    void conditionalExport();
    void conditionalFileTagger();
    void configure();
    void conflictingArtifacts();
    void cxxLanguageVersion();
    void cxxLanguageVersion_data();
    void conanfileProbe();
    void cpuFeatures();
    void dependenciesProperty();
    void dependencyScanningLoop();
    void deprecatedProperty();
    void disappearedProfile();
    void discardUnusedData();
    void discardUnusedData_data();
    void driverLinkerFlags();
    void driverLinkerFlags_data();
    void dynamicLibraryInModule();
    void dynamicMultiplexRule();
    void dynamicProject();
    void dynamicRuleOutputs();
    void emptyProfile();
    void enableExceptions();
    void enableExceptions_data();
    void enableRtti();
    void envMerging();
    void envNormalization();
    void erroneousFiles_data();
    void erroneousFiles();
    void errorInfo();
    void escapedLinkerFlags();
    void explicitlyDependsOn();
    void explicitlyDependsOn_data();
    void exportedDependencyInDisabledProduct();
    void exportedDependencyInDisabledProduct_data();
    void exportedPropertyInDisabledProduct();
    void exportedPropertyInDisabledProduct_data();
    void exportRule();
    void exportToOutsideSearchPath();
    void exportsPkgconfig();
    void exportsQbs();
    void externalLibs();
    void fileDependencies();
    void fileTagsFilterMerging();
    void freedesktop();
    void generatedArtifactAsInputToDynamicRule();
    void generateLinkerMapFile();
    void generator();
    void generator_data();
    void groupsInModules();
    void grpc_data();
    void grpc();
    void hostOsProperties();
    void ico();
    void importAssignment();
    void importChangeTracking();
    void importInPropertiesCondition();
    void importSearchPath();
    void importingProduct();
    void importsConflict();
    void includeLookup();
    void innoSetup();
    void innoSetupDependencies();
    void inputTagsChangeTracking_data();
    void inputTagsChangeTracking();
    void inputsFromDependencies();
    void installable();
    void installableAsAuxiliaryInput();
    void installedApp();
    void installDuplicates();
    void installDuplicatesNoError();
    void installedSourceFiles();
    void installedTransformerOutput();
    void installLocations_data();
    void installLocations();
    void installPackage();
    void installRootFromProjectFile();
    void installTree();
    void invalidCommandProperty_data();
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
    void jsExtensionsBinaryFile();
    void lastModuleCandidateBroken();
    void ld();
    void linkerMode();
    void linkerVariant_data();
    void linkerVariant();
    void lexyacc();
    void lexyaccOutputs();
    void lexyaccOutputs_data();
    void linkerLibraryDuplicates();
    void linkerLibraryDuplicates_data();
    void linkerScripts();
    void linkerModuleDefinition();
    void listProducts();
    void listPropertiesWithOuter();
    void listPropertyOrder();
    void loadableModule();
    void localDeployment();
    void makefileGenerator();
    void maximumCLanguageVersion();
    void maximumCxxLanguageVersion();
    void moduleProviders();
    void fallbackModuleProvider_data();
    void fallbackModuleProvider();
    void minimumSystemVersion();
    void minimumSystemVersion_data();
    void missingBuildGraph();
    void missingBuildGraph_data();
    void missingDependency();
    void missingProjectFile();
    void missingOverridePrefix();
    void moduleConditions();
    void movedFileDependency();
    void multipleChanges();
    void multipleConfigurations();
    void multiplexedTool();
    void nestedGroups();
    void nestedProperties();
    void newOutputArtifact();
    void noExportedSymbols_data();
    void noExportedSymbols();
    void noProfile();
    void noSuchProfile();
    void nodejs();
    void nonBrokenFilesInBrokenProduct();
    void nonDefaultProduct();
    void notAlwaysUpdated();
    void nsis();
    void nsisDependencies();
    void outOfDateMarking();
    void outputArtifactAutoTagging();
    void outputRedirection();
    void overrideProjectProperties();
    void pathProbe_data();
    void pathProbe();
    void pchChangeTracking();
    void perGroupDefineInExportItem();
    void pkgConfigProbe();
    void pkgConfigProbe_data();
    void pkgConfigProbeSysroot();
    void pluginDependency();
    void precompiledAndPrefixHeaders();
    void precompiledHeaderAndRedefine();
    void preventFloatingPointValues();
    void probeChangeTracking();
    void probeProperties();
    void probesAndShadowProducts();
    void probeInExportedModule();
    void probesAndArrayProperties();
    void probesInNestedModules();
    void productDependenciesByType();
    void productProperties();
    void propertyAssignmentOnNonPresentModule();
    void propertyAssignmentInFailedModule();
    void propertyChanges();
    void propertyEvaluationContext();
    void propertyPrecedence();
    void properQuoting();
    void propertiesInExportItems();
    void protobuf_data();
    void protobuf();
    void pseudoMultiplexing();
    void qbsConfig();
    void qbsSession();
    void qbsVersion();
    void qtBug51237();
    void radAfterIncompleteBuild();
    void radAfterIncompleteBuild_data();
    void recursiveRenaming();
    void recursiveWildcards();
    void referenceErrorInExport();
    void removeDuplicateLibraries_data();
    void removeDuplicateLibraries();
    void reproducibleBuild();
    void reproducibleBuild_data();
    void require();
    void requireDeprecated();
    void rescueTransformerData();
    void responseFiles();
    void retaggedOutputArtifact();
    void ruleConditions();
    void ruleConnectionWithExcludedInputs();
    void ruleCycle();
    void ruleWithNoInputs();
    void ruleWithNonRequiredInputs();
    void sanitizer_data();
    void sanitizer();
    void scannerItem();
    void scanResultInOtherProduct();
    void scanResultInNonDependency();
    void setupBuildEnvironment();
    void setupRunEnvironment();
    void smartRelinking();
    void smartRelinking_data();
    void soVersion();
    void soVersion_data();
    void sourceArtifactChanges();
    void subProfileChangeTracking();
    void successiveChanges();
    void symbolLinkMode();
    void symlinkRemoval();
    void renameDependency();
    void separateDebugInfo();
    void sevenZip();
    void sourceArtifactInInputsFromDependencies();
    void staticLibWithoutSources();
    void suspiciousCalls();
    void suspiciousCalls_data();
    void systemIncludePaths();
    void distributionIncludePaths();
    void systemRunPaths();
    void systemRunPaths_data();
    void tar();
    void textTemplate();
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
    void undefinedTargetPlatform();
    void usingsAsSoleInputsNonMultiplexed();
    void variantSuffix();
    void variantSuffix_data();
    void vcsGit();
    void vcsSubversion();
    void versionCheck();
    void versionCheck_data();
    void versionScript();
    void wholeArchive();
    void wholeArchive_data();
    void wildCardsAndRules();
    void wildcardRenaming();
    void wix();
    void wixDependencies();
    void zip();
    void zip_data();
    void zipInvalid();

private:
    QMap<QString, QString> findCli(int *status);
    QMap<QString, QString> findNodejs(int *status);
    QMap<QString, QString> findTypeScript(int *status);
    QString findArchiver(const QString &fileName, int *status = nullptr);
    static bool lexYaccExist();
    static qbs::Version bisonVersion();
};

#endif // TST_BLACKBOX_H
