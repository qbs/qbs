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

#ifndef QBS_TST_API_H
#define QBS_TST_API_H

#include <tools/buildoptions.h>

#include <QtCore/qobject.h>
#include <QtCore/qvariant.h>

namespace qbs {
class ErrorInfo;
class SetupProjectParameters;
}

class BuildDescriptionReceiver;
class LogSink;
class ProcessResultReceiver;
class TaskReceiver;

class TestApi : public QObject
{
    Q_OBJECT

public:
    TestApi();
    ~TestApi() override;

private slots:
    void initTestCase();
    void init();

    void addQObjectMacroToCppFile();
    void addedFilePersistent();
    void baseProperties();
    void buildErrorCodeLocation();
    void buildGraphInfo();
    void buildGraphLocking();
    void buildProject();
    void buildProject_data();
    void buildProjectDryRun();
    void buildProjectDryRun_data();
    void buildSingleFile();
    void canonicalToolchainList();
#ifdef QBS_ENABLE_PROJECT_FILE_UPDATES
    void changeContent();
#endif
    void changeDependentLib();
    void checkOutputs();
    void checkOutputs_data();
    void commandExtraction();
    void dependencyOnMultiplexedType();
    void disabledInstallGroup();
    void disabledProduct();
    void disabledProject();
    void duplicateProductNames();
    void duplicateProductNames_data();
    void emptyFileTagList();
    void emptySubmodulesList();
    void enableAndDisableProduct();
    void errorInSetupRunEnvironment();
    void excludedInputs();
    void explicitlyDependsOn();
    void exportSimple();
    void exportWithRecursiveDepends();
    void fallbackGcc();
    void fileTagger();
    void fileTagsFilterOverride();
    void generatedFilesList();
    void infiniteLoopBuilding();
    void infiniteLoopBuilding_data();
    void infiniteLoopResolving();
    void inheritQbsSearchPaths();
    void installableFiles();
    void isRunnable();
    void linkDynamicLibs();
    void linkDynamicAndStaticLibs();
    void linkStaticAndDynamicLibs();
    void listBuildSystemFiles();
    void localProfiles();
    void localProfiles_data();
    void missingSourceFile();
    void mocCppIncluded();
    void multiArch();
    void multiplexing();
    void newOutputArtifactInDependency();
    void newPatternMatch();
    void nonexistingProjectPropertyFromCommandLine();
    void nonexistingProjectPropertyFromProduct();
    void objC();
    void projectDataAfterProductInvalidation();
    void processResult();
    void processResult_data();
    void projectInvalidation();
    void projectLocking();
    void projectPropertiesByName();
    void projectWithPropertiesItem();
    void projectWithProbeAndProfileItem();
    void propertiesBlocks();
    void rc();
    void referencedFileErrors();
    void referencedFileErrors_data();
    void references();
    void relaxedModeRecovery();
    void removeFileDependency();
    void renameProduct();
    void renameTargetArtifact();
    void resolveProject();
    void resolveProject_data();
    void resolveProjectDryRun();
    void resolveProjectDryRun_data();
    void restoredWarnings();
    void ruleConflict();
    void runEnvForDisabledProduct();
    void softDependency();
    void sourceFileInBuildDir();
    void subProjects();
    void targetArtifactStatus_data();
    void targetArtifactStatus();
    void timeout();
    void timeout_data();
    void toolInModule();
    void trackAddQObjectHeader();
    void trackRemoveQObjectHeader();
    void transformerData();
    void transformers();
    void typeChange();
    void uic();

private:
    qbs::SetupProjectParameters defaultSetupParameters(const QString &projectFileOrDir) const;
    qbs::ErrorInfo doBuildProject(const QString &projectFilePath,
                                  BuildDescriptionReceiver *buildDescriptionReceiver = 0,
                                  ProcessResultReceiver *procResultReceiver = 0,
                                  TaskReceiver *taskReceiver = 0,
                                  const qbs::BuildOptions &options = qbs::BuildOptions(),
                                  const QVariantMap &overriddenValues = QVariantMap());

    LogSink * const m_logSink;
    const QString m_sourceDataDir;
    const QString m_workingDataDir;
};

#endif // Include guard.
