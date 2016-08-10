/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company.  For licensing terms and
** conditions see http://www.qt.io/terms-conditions.  For further information
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
    void assembly();
    void assetCatalog();
    void assetCatalog_data();
    void badInterpreter();
    void buildDirectories();
    void bundleStructure();
    void bundleStructure_data();
    void changedFiles_data();
    void changedFiles();
    void changeInDisabledProduct();
    void changeInImportedFile();
    void checkProjectFilePath();
    void clean();
    void cli();
    void concurrentExecutor();
    void conditionalExport();
    void conflictingArtifacts();
    void dbusAdaptors();
    void dbusInterfaces();
    void dependenciesProperty();
    void dependencyProfileMismatch();
    void deploymentTarget();
    void deploymentTarget_data();
    void dynamicMultiplexRule();
    void dynamicRuleOutputs();
    void embedInfoPlist();
    void enableExceptions();
    void enableExceptions_data();
    void enableRtti();
    void erroneousFiles_data();
    void erroneousFiles();
    void errorInfo();
    void exportRule();
    void exportToOutsideSearchPath();
    void fileDependencies();
    void frameworkStructure();
    void groupsInModules();
    void iconset();
    void iconsetApp();
    void importingProduct();
    void infoPlist();
    void inputsFromDependencies();
    void installable();
    void installedApp();
    void installDuplicates();
    void installedSourceFiles();
    void installedTransformerOutput();
    void installPackage();
    void installTree();
    void invalidCommandProperty();
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
    void lrelease();
    void missingDependency();
    void missingProfile();
    void mixedBuildVariants();
    void mocFlags();
    void multipleChanges();
    void nestedGroups();
    void nestedProperties();
    void newOutputArtifact();
    void nodejs();
    void nonBrokenFilesInBrokenProduct();
    void nonDefaultProduct();
    void nsis();
    void objcArc();
    void outputArtifactAutoTagging();
    void overrideProjectProperties();
    void pchChangeTracking();
    void pkgConfigProbe();
    void pkgConfigProbe_data();
    void pkgConfigProbeSysroot();
    void pluginMetaData();
    void probeChangeTracking();
    void probeProperties();
    void probeInExportedModule();
    void probesAndArrayProperties();
    void probesInNestedModules();
    void productDependenciesByType();
    void productProperties();
    void propertyChanges();
    void propertyPrecedence();
    void properQuoting();
    void qbsVersion();
    void qmlDebugging();
    void qobjectInObjectiveCpp();
    void radAfterIncompleteBuild();
    void recursiveRenaming();
    void recursiveWildcards();
    void referenceErrorInExport();
    void reproducibleBuild();
    void reproducibleBuild_data();
    void responseFiles();
    void ruleConditions();
    void ruleCycle();
    void ruleWithNoInputs();
    void subProfileChangeTracking();
    void successiveChanges();
    void symlinkRemoval();
    void renameDependency();
    void separateDebugInfo();
    void sevenZip();
    void systemRunPaths();
    void systemRunPaths_data();
    void tar();
    void toolLookup();
    void topLevelSearchPath();
    void track_qobject_change();
    void track_qrc();
    void trackAddFile();
    void trackAddFileTag();
    void trackAddMocInclude();
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
    void wildCardsAndRules();
    void wildcardRenaming();
    void wix();
    void xcode();
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
