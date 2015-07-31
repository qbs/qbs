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

#ifndef QBS_TST_API_H
#define QBS_TST_API_H

#include <tools/buildoptions.h>

#include <QObject>
#include <QVariant>

namespace qbs {
class ErrorInfo;
class SetupProjectParameters;
}

class LogSink;

class TestApi : public QObject
{
    Q_OBJECT

public:
    TestApi();
    ~TestApi();

private slots:
    void initTestCase();

    void addQObjectMacroToCppFile();
    void addedFilePersistent();
    void baseProperties();
    void buildGraphLocking();
    void buildProject();
    void buildProject_data();
    void buildProjectDryRun();
    void buildProjectDryRun_data();
    void buildSingleFile();
#ifdef QBS_ENABLE_PROJECT_FILE_UPDATES
    void changeContent();
#endif
    void changeDependentLib();
    void commandExtraction();
    void disabledInstallGroup();
    void disabledProduct();
    void disabledProject();
    void duplicateProductNames();
    void duplicateProductNames_data();
    void dynamicLibs();
    void emptyFileTagList();
    void emptySubmodulesList();
    void enableAndDisableProduct();
    void explicitlyDependsOn();
    void exportSimple();
    void exportWithRecursiveDepends();
    void fileTagger();
    void fileTagsFilterOverride();
    void infiniteLoopBuilding();
    void infiniteLoopBuilding_data();
    void infiniteLoopResolving();
    void inheritQbsSearchPaths();
    void installableFiles();
    void isRunnable();
    void listBuildSystemFiles();
    void mocCppIncluded();
    void multiArch();
    void newOutputArtifactInDependency();
    void newPatternMatch();
    void nonexistingProjectPropertyFromCommandLine();
    void nonexistingProjectPropertyFromProduct();
    void objC();
    void projectInvalidation();
    void projectLocking();
    void projectPropertiesByName();
    void projectWithPropertiesItem();
    void propertiesBlocks();
    void rc();
    void references();
    void removeFileDependency();
    void renameProduct();
    void renameTargetArtifact();
    void resolveProject();
    void resolveProject_data();
    void resolveProjectDryRun();
    void resolveProjectDryRun_data();
    void softDependency();
    void sourceFileInBuildDir();
    void subProjects();
    void trackAddQObjectHeader();
    void trackRemoveQObjectHeader();
    void transformers();
    void typeChange();
    void uic();

private:
    qbs::SetupProjectParameters defaultSetupParameters(const QString &projectFilePath) const;
    qbs::ErrorInfo doBuildProject(const QString &projectFilePath,
                                  QObject *buildDescriptionReceiver = 0,
                                  QObject *procResultReceiver = 0,
                                  QObject *taskReceiver = 0,
                                  const qbs::BuildOptions &options = qbs::BuildOptions(),
                                  const QVariantMap overriddenValues = QVariantMap());

    LogSink * const m_logSink;
    const QString m_sourceDataDir;
    const QString m_workingDataDir;
};

#endif // Include guard.
