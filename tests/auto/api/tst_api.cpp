/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "tst_api.h"

#include "../../../src/app/shared/qbssettings.h"

#include <api/jobs.h>
#include <api/project.h>
#include <api/projectdata.h>
#include <logging/ilogsink.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/buildoptions.h>
#include <tools/installoptions.h>
#include <tools/preferences.h>
#include <tools/setupprojectparameters.h>

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QScopedPointer>
#include <QStringList>
#include <QTest>
#include <QTimer>

class LogSink: public qbs::ILogSink
{
    void doPrintWarning(const qbs::ErrorInfo &error) {
        qDebug("%s", qPrintable(error.toString()));
    }
    void doPrintMessage(qbs::LoggerLevel, const QString &, const QString &) { }
};

class BuildDescriptionReceiver : public QObject
{
    Q_OBJECT
public:
    QString descriptions;

private slots:
    void handleDescription(const QString &, const QString &description) {
        descriptions += description;
    }
};

TestApi::TestApi()
    : m_logSink(new LogSink)
    , m_sourceDataDir(QDir::cleanPath(SRCDIR "/testdata"))
    , m_workingDataDir(QCoreApplication::applicationDirPath() + "/../tests/auto/api/testWorkDir")
{
}

TestApi::~TestApi()
{
    delete m_logSink;
}

void TestApi::initTestCase()
{
    QString errorMessage;
    qbs::Internal::removeDirectoryWithContents(m_workingDataDir, &errorMessage);
    QVERIFY2(qbs::Internal::copyFileRecursion(m_sourceDataDir,
            m_workingDataDir, false, &errorMessage), qPrintable(errorMessage));
}

static bool waitForFinished(qbs::AbstractJob *job, int timeout = 0)
{
    QEventLoop loop;
    QObject::connect(job, SIGNAL(finished(bool,qbs::AbstractJob*)), &loop, SLOT(quit()));
    if (timeout > 0) {
        QTimer timer;
        QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
        timer.setSingleShot(true);
        timer.start(timeout);
        loop.exec();
        return timer.isActive(); // Timer ended the loop <=> job did not finish.
    }
    loop.exec();
    return true;
}


void printProjectData(const qbs::ProjectData &project)
{
    foreach (const qbs::ProductData &p, project.products()) {
        qDebug("    Product '%s' at %s", qPrintable(p.name()), qPrintable(p.location().toString()));
        foreach (const qbs::GroupData &g, p.groups()) {
            qDebug("        Group '%s' at %s", qPrintable(g.name()), qPrintable(g.location().toString()));
            qDebug("            Files: %s", qPrintable(g.filePaths().join(QLatin1String(", "))));
        }
    }
}

void TestApi::buildSingleFile()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters();
    const QString projectDirPath = QDir::cleanPath(m_workingDataDir + "/build-single-file");
    setupParams.setProjectFilePath(projectDirPath + "/project.qbs");
    QScopedPointer<qbs::SetupProjectJob> setupJob(qbs::Project::setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(setupJob.data());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    qbs::BuildOptions options;
    options.setDryRun(true);
    options.setFilesToConsider(QStringList(projectDirPath + "/compiled.cpp"));
    options.setActiveFileTags(QStringList("obj"));
    m_logSink->setLogLevel(qbs::LoggerMaxLevel);
    QScopedPointer<qbs::BuildJob> buildJob(project.buildAllProducts(options));
    BuildDescriptionReceiver receiver;
    connect(buildJob.data(), SIGNAL(reportCommandDescription(QString,QString)), &receiver,
            SLOT(handleDescription(QString,QString)));
    waitForFinished(buildJob.data());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    QCOMPARE(receiver.descriptions.count("compiling"), 1);
    QVERIFY2(receiver.descriptions.contains("compiling compiled.cpp"),
             qPrintable(receiver.descriptions));
}

qbs::GroupData findGroup(const qbs::ProductData &product, const QString &name)
{
    foreach (const qbs::GroupData &g, product.groups()) {
        if (g.name() == name)
            return g;
    }
    return qbs::GroupData();
}

void TestApi::changeContent()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters();
    setupParams.setProjectFilePath(QDir::cleanPath(m_workingDataDir +
        "/project-editing/project.qbs"));
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project::setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    qbs::Project project = job->project();
    qbs::ProjectData projectData = project.projectData();
    QCOMPARE(projectData.allProducts().count(), 1);
    qbs::ProductData product = projectData.allProducts().first();
    QCOMPARE(product.groups().count(), 6);

    // Error handling: Invalid product.
    qbs::ErrorInfo errorInfo = project.addGroup(qbs::ProductData(), "blubb");
    QVERIFY(errorInfo.hasError());
    QVERIFY(errorInfo.toString().contains("invalid"));

    // Error handling: Empty group name.
    errorInfo = project.addGroup(product, QString());
    QVERIFY(errorInfo.hasError());
    QVERIFY(errorInfo.toString().contains("empty"));

    errorInfo = project.addGroup(product, "New Group 1");
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()));

    errorInfo = project.addGroup(product, "New Group 2");
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()));

    // Error handling: Group already inserted.
    errorInfo = project.addGroup(product, "New Group 1");
    QVERIFY(errorInfo.hasError());
    QVERIFY(errorInfo.toString().contains("already"));

    // Error handling: Add list of files with double entries.
    errorInfo = project.addFiles(product, qbs::GroupData(), QStringList() << "file.cpp"
                                 << "file.cpp");
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("more than once"), qPrintable(errorInfo.toString()));

    // Add files to empty array literal.
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    QCOMPARE(product.groups().count(), 8);
    qbs::GroupData group = findGroup(product, "New Group 1");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "file.h" << "file.cpp");
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()));

    // Error handling: Add the same file again.
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    QCOMPARE(product.groups().count(), 8);
    group = findGroup(product, "New Group 1");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "file.cpp");
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("already"), qPrintable(errorInfo.toString()));

    // Remove one of the newly added files again.
    errorInfo = project.removeFiles(product, group, QStringList("file.h"));
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()));

    // Error handling: Try to remove the same file again.
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    QCOMPARE(product.groups().count(), 8);
    group = findGroup(product, "New Group 1");
    QVERIFY(group.isValid());
    errorInfo = project.removeFiles(product, group, QStringList() << "file.h");
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("not known"), qPrintable(errorInfo.toString()));

    // Error handling: Try to remove a file from a complex list.
    group = findGroup(product, "Existing Group 2");
    QVERIFY(group.isValid());
    errorInfo = project.removeFiles(product, group, QStringList() << "existingfile2.txt");
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("complex"), qPrintable(errorInfo.toString()));

    // Remove file from product's 'files' binding.
    errorInfo = project.removeFiles(product, qbs::GroupData(), QStringList("main.cpp"));
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()));

    // Add file to non-empty array literal.
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    group = findGroup(product, "Existing Group 1");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "newfile1.txt");
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()));

    // Add files to list represented as a single string.
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    errorInfo = project.addFiles(product, qbs::GroupData(), QStringList() << "newfile2.txt");
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()));

    // Add files to list represented as an identifier.
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    group = findGroup(product, "Existing Group 2");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "newfile3.txt");
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()));

    // Add files to list represented as a block of code (not yet implemented).
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    group = findGroup(product, "Existing Group 3");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "newfile4.txt");
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("complex"), qPrintable(errorInfo.toString()));

    // Add file to group with directory prefix.
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    group = findGroup(product, "Existing Group 4");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "file.txt");
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()));

    // Error handling: Add file to group with non-directory prefix.
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    group = findGroup(product, "Existing Group 5");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "newfile1.txt");
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("prefix"), qPrintable(errorInfo.toString()));

    // Remove group.
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    group = findGroup(product, "Existing Group 5");
    QVERIFY(group.isValid());
    errorInfo = project.removeGroup(product, group);
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()));
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    QCOMPARE(projectData.products().first().groups().count(), 7);

    // Error handling: Try to remove the same group again.
    errorInfo = project.removeGroup(product, group);
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("does not exist"), qPrintable(errorInfo.toString()));

    // Check whether building will take the added and removed cpp files into account.
    // This must not be moved below the re-resolving test!!!
    qbs::BuildOptions buildOptions;
    buildOptions.setDryRun(true);
    BuildDescriptionReceiver rcvr;
    QScopedPointer<qbs::BuildJob> buildJob(project.buildAllProducts(buildOptions, this));
    connect(buildJob.data(), SIGNAL(reportCommandDescription(QString,QString)), &rcvr,
            SLOT(handleDescription(QString,QString)));
    waitForFinished(buildJob.data());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    QVERIFY(rcvr.descriptions.contains("compiling file.cpp"));
    QVERIFY(!rcvr.descriptions.contains("compiling main.cpp"));

    // Now check whether the data updates were done correctly.
    projectData = project.projectData();
    job.reset(qbs::Project::setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    project = job->project();
    const qbs::ProjectData newProjectData = project.projectData();

    // Can't use Project::operator== here, as the target artifacts will differ due to the build
    // not having run yet.
    const bool projectDataMatches = newProjectData.products().count() == 1
            && projectData.products().count() == 1
            && newProjectData.products().first().groups() == projectData.products().first().groups();
    if (!projectDataMatches) {
        qDebug("This is the assumed project:");
        printProjectData(projectData);
        qDebug("This is the actual project:");
        printProjectData(newProjectData);
    }
    QVERIFY(projectDataMatches); // Will fail if e.g. code locations don't match.

    // Now try building again and check if the newly resolved product behaves the same way.
    buildJob.reset(project.buildAllProducts(buildOptions, this));
    connect(buildJob.data(), SIGNAL(reportCommandDescription(QString,QString)), &rcvr,
            SLOT(handleDescription(QString,QString)));
    waitForFinished(buildJob.data());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    QVERIFY(rcvr.descriptions.contains("compiling file.cpp"));
    QVERIFY(!rcvr.descriptions.contains("compiling main.cpp"));

    // Now, after the build, the project data must be entirely identical.
    QVERIFY(projectData == project.projectData());

    // Error handling: Try to change the project during a build.
    buildJob.reset(project.buildAllProducts(buildOptions, this));
    errorInfo = project.addGroup(newProjectData.products().first(), "blubb");
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("in process"), qPrintable(errorInfo.toString()));
    waitForFinished(buildJob.data());
    errorInfo = project.addGroup(newProjectData.products().first(), "blubb");
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()));
}

static qbs::ErrorInfo forceRuleEvaluation(const qbs::Project project)
{
    qbs::BuildOptions buildOptions;
    buildOptions.setDryRun(true);
    QScopedPointer<qbs::BuildJob> buildJob(project.buildAllProducts(buildOptions));
    waitForFinished(buildJob.data());
    return buildJob->error();
}

void TestApi::disabledInstallGroup()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters();
    setupParams.setProjectFilePath(QDir::cleanPath(m_workingDataDir +
        "/disabled_install_group/project.qbs"));
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project::setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    const qbs::Project project = job->project();

    const qbs::ErrorInfo errorInfo = forceRuleEvaluation(project);
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()));

    qbs::ProjectData projectData = project.projectData();
    QCOMPARE(projectData.allProducts().count(), 1);
    qbs::ProductData product = projectData.allProducts().first();
    const QList<qbs::TargetArtifact> targets = product.targetArtifacts();
    QCOMPARE(targets.count(), 1);
    QVERIFY(targets.first().isExecutable());
    QList<qbs::InstallableFile> installableFiles
            = project.installableFilesForProduct(product, qbs::InstallOptions());
    QCOMPARE(installableFiles.count(), 0);
    QCOMPARE(project.targetExecutable(product, qbs::InstallOptions()), targets.first().filePath());
}

void TestApi::fileTagsFilterOverride()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters();
    setupParams.setProjectFilePath(QDir::cleanPath(m_workingDataDir +
        "/filetagsfilter_override/project.qbs"));
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project::setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    qbs::Project project = job->project();

    const qbs::ErrorInfo errorInfo = forceRuleEvaluation(project);
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()));

    qbs::ProjectData projectData = project.projectData();
    QCOMPARE(projectData.allProducts().count(), 1);
    const qbs::ProductData product = projectData.allProducts().first();
    QList<qbs::InstallableFile> installableFiles
            = project.installableFilesForProduct(product, qbs::InstallOptions());
    QCOMPARE(installableFiles.count(), 1);
    QEXPECT_FAIL(0, "QBS-424", Continue);
    QVERIFY(installableFiles.first().targetDirectory().contains("habicht"));
}

void TestApi::infiniteLoopBuilding()
{
    QFETCH(QString, projectDirName);
    qbs::SetupProjectParameters setupParams = defaultSetupParameters();
    const QString projectDir = QDir::cleanPath(m_workingDataDir + '/' + projectDirName);
    setupParams.setProjectFilePath(projectDir + "/infinite-loop.qbs");
    setupParams.setBuildRoot(projectDir);
    QScopedPointer<qbs::SetupProjectJob> setupJob(qbs::Project::setupProject(setupParams,
                                                                             m_logSink, 0));
    waitForFinished(setupJob.data());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    const QScopedPointer<qbs::BuildJob> buildJob(project.buildAllProducts(qbs::BuildOptions()));
    QTimer::singleShot(1000, buildJob.data(), SLOT(cancel()));
    QVERIFY(waitForFinished(buildJob.data(), 30000));
}

void TestApi::infiniteLoopBuilding_data()
{
    QTest::addColumn<QString>("projectDirName");
    QTest::newRow("JS Command") << QString("infinite-loop-js");
    QTest::newRow("Process Command") << QString("infinite-loop-process");
}

void TestApi::infiniteLoopResolving()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters();
    const QString projectDir = QDir::cleanPath(m_workingDataDir + "/infinite-loop-resolving");
    setupParams.setProjectFilePath(projectDir + "/project.qbs");
    setupParams.setBuildRoot(projectDir);
    QScopedPointer<qbs::SetupProjectJob> setupJob(qbs::Project::setupProject(setupParams,
                                                                             m_logSink, 0));
    QTimer::singleShot(1000, setupJob.data(), SLOT(cancel()));
    QVERIFY(waitForFinished(setupJob.data(), 30000));
    QVERIFY2(setupJob->error().toString().toLower().contains("cancel"),
             qPrintable(setupJob->error().toString()));
}

void TestApi::installableFiles()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters();
    setupParams.setProjectFilePath(QDir::cleanPath(QLatin1String(SRCDIR "/../blackbox/testdata"
        "/installed_artifact/installed_artifact.qbs")));
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project::setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    qbs::Project project = job->project();

    const qbs::ErrorInfo errorInfo = forceRuleEvaluation(project);
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()));

    qbs::ProjectData projectData = project.projectData();
    QCOMPARE(projectData.allProducts().count(), 1);
    qbs::ProductData product = projectData.allProducts().first();
    qbs::InstallOptions installOptions;
    installOptions.setInstallRoot(QLatin1String("/tmp"));
    QList<qbs::InstallableFile> installableFiles
            = project.installableFilesForProduct(product, installOptions);
    QCOMPARE(installableFiles.count(), 2);
    foreach (const qbs::InstallableFile &f,installableFiles) {
        if (!f.sourceFilePath().endsWith("main.cpp")) {
            QVERIFY(f.isExecutable());
            QString expectedTargetFilePath = qbs::Internal::HostOsInfo
                    ::appendExecutableSuffix(QLatin1String("/tmp/usr/bin/installedApp"));
            QCOMPARE(f.targetFilePath(), expectedTargetFilePath);
            QCOMPARE(project.targetExecutable(product, installOptions), expectedTargetFilePath);
            break;
        }
    }

    setupParams.setProjectFilePath(QDir::cleanPath(QLatin1String(SRCDIR "/../blackbox/testdata"
        "/recursive_wildcards/recursive_wildcards.qbs")));
    job.reset(qbs::Project::setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    project = job->project();
    projectData = project.projectData();
    QCOMPARE(projectData.allProducts().count(), 1);
    product = projectData.allProducts().first();
    installableFiles = project.installableFilesForProduct(product, installOptions);
    QCOMPARE(installableFiles.count(), 2);
    foreach (const qbs::InstallableFile &f, installableFiles)
        QVERIFY(!f.isExecutable());
    QCOMPARE(installableFiles.first().targetFilePath(), QLatin1String("/tmp/dir/file1.txt"));
    QCOMPARE(installableFiles.last().targetFilePath(), QLatin1String("/tmp/dir/file2.txt"));
}

void TestApi::listBuildSystemFiles()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters();
    const QString projectDir
            = QDir::cleanPath(QLatin1String(SRCDIR "/../blackbox/testdata/subprojects"));
    const QString topLevelProjectFile = projectDir + QLatin1String("/toplevelproject.qbs");
    setupParams.setProjectFilePath(topLevelProjectFile);
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project::setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    const QSet<QString> buildSystemFiles = job->project().buildSystemFiles();
    QVERIFY(buildSystemFiles.contains(topLevelProjectFile));
    QVERIFY(buildSystemFiles.contains(projectDir + QLatin1String("/subproject2/subproject2.qbs")));
    QVERIFY(buildSystemFiles.contains(projectDir
                                      + QLatin1String("/subproject2/subproject3/subproject3.qbs")));
}

void TestApi::nonexistingProjectPropertyFromProduct()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters();
    const QString projectDir
            = QDir::cleanPath(m_workingDataDir + "/nonexistingprojectproperties");
    const QString topLevelProjectFile = projectDir + QLatin1String("/invalidaccessfromproduct.qbs");
    setupParams.setProjectFilePath(topLevelProjectFile);
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project::setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QEXPECT_FAIL("", "QBS-432", Abort);
    QVERIFY(job->error().hasError());
    QVERIFY2(job->error().toString().contains(QLatin1String("blubb")),
             qPrintable(job->error().toString()));
}

void TestApi::nonexistingProjectPropertyFromCommandLine()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters();
    const QString projectDir = QDir::cleanPath(m_workingDataDir + "/nonexistingprojectproperties");
    const QString topLevelProjectFile = projectDir + QLatin1String("/project.qbs");
    setupParams.setProjectFilePath(topLevelProjectFile);
    QVariantMap projectProperties;
    projectProperties.insert(QLatin1String("project.blubb"), QLatin1String("true"));
    setupParams.setOverriddenValues(projectProperties);
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project::setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY(job->error().hasError());
    QVERIFY2(job->error().toString().contains(QLatin1String("blubb")),
             qPrintable(job->error().toString()));
}

qbs::SetupProjectParameters TestApi::defaultSetupParameters() const
{
    qbs::SetupProjectParameters setupParams;
    setupParams.setDryRun(true); // So no build graph gets created.
    setupParams.setBuildRoot(QLatin1String("/blubb")); // Must be set and be absolute.
    setupParams.setRestoreBehavior(qbs::SetupProjectParameters::ResolveOnly); // No restoring.

    const QString qbsRootPath = QDir::cleanPath(QCoreApplication::applicationDirPath()
                                                + QLatin1String("/../"));
    SettingsPtr settings = qbsSettings(QString());
    const QString profileName = QLatin1String("qbs_autotests");
    const qbs::Preferences prefs(settings.data(), profileName);
    setupParams.setSearchPaths(prefs.searchPaths(qbsRootPath));
    setupParams.setPluginPaths(prefs.pluginPaths(qbsRootPath + QLatin1String("/lib")));
    QVariantMap buildConfig;
    buildConfig.insert(QLatin1String("qbs.profile"), profileName);
    buildConfig.insert(QLatin1String("qbs.buildVariant"), QLatin1String("debug"));
    setupParams.setBuildConfiguration(buildConfig);
    setupParams.expandBuildConfiguration(settings.data());
    return setupParams;
}

void TestApi::references()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters();
    const QString projectDir = QDir::cleanPath(m_workingDataDir + "/references");
    setupParams.setProjectFilePath(projectDir + QLatin1String("/invalid1.qbs"));
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project::setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY(job->error().hasError());
    QString errorString = job->error().toString();
    QVERIFY2(errorString.contains("does not contain"), qPrintable(errorString));

    setupParams.setProjectFilePath(projectDir + QLatin1String("/invalid2.qbs"));
    job.reset(qbs::Project::setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY(job->error().hasError());
    errorString = job->error().toString();
    QVERIFY2(errorString.contains("contains more than one"), qPrintable(errorString));

    setupParams.setProjectFilePath(projectDir + QLatin1String("/valid.qbs"));
    job.reset(qbs::Project::setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    const qbs::ProjectData topLevelProject = job->project().projectData();
    QCOMPARE(topLevelProject.subProjects().count(), 1);
    const QString subProjectFileName
            = QFileInfo(topLevelProject.subProjects().first().location().fileName()).fileName();
    QCOMPARE(subProjectFileName, QString("p.qbs"));
}

void TestApi::sourceFileInBuildDir()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters();
    const QString projectDir = QDir::cleanPath(m_workingDataDir + "/source-file-in-build-dir");
    setupParams.setProjectFilePath(projectDir + QLatin1String("/project.qbs"));
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project::setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    const qbs::ProjectData projectData = job->project().projectData();
    QCOMPARE(projectData.allProducts().count(), 1);
    const qbs::ProductData product = projectData.allProducts().first();
    QCOMPARE(product.groups().count(), 1);
    const qbs::GroupData group = product.groups().first();
    QCOMPARE(group.allFilePaths().count(), 1);
}

QTEST_MAIN(TestApi)

#include "tst_api.moc"
