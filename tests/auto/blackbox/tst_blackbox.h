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

#ifndef TST_BLACKBOX_H
#define TST_BLACKBOX_H

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QtTest>

class QbsRunParameters
{
public:
    QbsRunParameters()
    {
        init();
    }

    QbsRunParameters(const QString &cmd, const QStringList &args = QStringList())
        : command(cmd), arguments(args)
    {
        init();
    }

    QbsRunParameters(const QStringList &args)
        : arguments(args)
    {
        init();
    }

    void init()
    {
        expectFailure = false;
        useProfile = true;
        environment = QProcessEnvironment::systemEnvironment();
    }

    QString command;
    QStringList arguments;
    QProcessEnvironment environment;
    bool expectFailure;
    bool useProfile;
};

class TestBlackbox : public QObject
{
    Q_OBJECT
    const QString testDataDir;
    const QString testSourceDir;
    const QString qbsExecutableFilePath;
    const QString buildProfileName;
    const QString buildDir;
    const QString defaultInstallRoot;
    const QString buildGraphPath;

public:
    TestBlackbox();

protected:
    int runQbs(const QbsRunParameters &params = QbsRunParameters());
    void rmDirR(const QString &dir);
    static QByteArray unifiedLineEndings(const QByteArray &ba);
    static void sanitizeOutput(QByteArray *ba);

public slots:
    void initTestCase();

private slots:
    void addedFilePersistent();
    void addQObjectMacroToCppFile();
    void baseProperties();
    void buildDirectories();
    void build_project_data();
    void build_project();
    void build_project_dry_run_data();
    void build_project_dry_run();
    void changeDependentLib();
    void changedFiles_data();
    void changedFiles();
    void changeInDisabledProduct();
    void dependenciesProperty();
    void disabledProject();
    void enableAndDisableProduct();
    void duplicateProductNames();
    void duplicateProductNames_data();
    void dynamicLibs();
    void dynamicMultiplexRule();
    void dynamicRuleOutputs();
    void emptyFileTagList();
    void emptySubmodulesList();
    void erroneousFiles_data();
    void erroneousFiles();
    void explicitlyDependsOn();
    void fileDependencies();
    void installedTransformerOutput();
    void jsExtensionsFile();
    void jsExtensionsFileInfo();
    void jsExtensionsProcess();
    void jsExtensionsPropertyList();
    void jsExtensionsTextFile();
    void inheritQbsSearchPaths();
    void mixedBuildVariants();
    void mocCppIncluded();
    void nestedProperties();
    void newOutputArtifactInDependency();
    void newPatternMatch();
    void nonBrokenFilesInBrokenProduct();
    void objC();
    void qmlDebugging();
    void projectWithPropertiesItem();
    void properQuoting();
    void propertiesBlocks();
    void radAfterIncompleteBuild_data();
    void radAfterIncompleteBuild();
    void resolve_project_data();
    void resolve_project();
    void resolve_project_dry_run_data();
    void resolve_project_dry_run();
    void symlinkRemoval();
    void typeChange();
    void usingsAsSoleInputsNonMultiplexed();
    void clean();
    void exportSimple();
    void exportWithRecursiveDepends();
    void fileTagger();
    void rc();
    void removeFileDependency();
    void renameDependency();
    void renameProduct();
    void renameTargetArtifact();
    void softDependency();
    void subProjects();
    void track_qrc();
    void track_qobject_change();
    void trackAddFile();
    void trackExternalProductChanges();
    void trackRemoveFile();
    void trackAddFileTag();
    void trackRemoveFileTag();
    void trackAddMocInclude();
    void trackAddProduct();
    void trackRemoveProduct();
    void transformers();
    void uic();
    void wildcardRenaming();
    void recursiveRenaming();
    void recursiveWildcards();
    void ruleConditions();
    void ruleCycle();
    void trackAddQObjectHeader();
    void trackRemoveQObjectHeader();
    void overrideProjectProperties();
    void productProperties();
    void propertyChanges();
    void installedApp();
    void toolLookup();
    void checkProjectFilePath();
    void missingProfile();
    void testAssembly();
    void testNsis();
    void testEmbedInfoPlist();
    void testWiX();
    void testNodeJs();
    void testTypeScript();
    void testIconset();
    void testIconsetApp();
    void testAssetCatalog();
    void wildCardsAndRules();

private:
    QString uniqueProductName(const QString &productName) const;
    QString productBuildDir(const QString &productName) const;
    QString executableFilePath(const QString &productName) const;

    QByteArray m_qbsStderr;
    QByteArray m_qbsStdout;
};

#endif // TST_BLACKBOX_H
