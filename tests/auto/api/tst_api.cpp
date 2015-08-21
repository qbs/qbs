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

#include "tst_api.h"

#include "../shared.h"

#include <qbs.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QScopedPointer>
#include <QStringList>
#include <QTest>
#include <QTimer>

#include <algorithm>
#include <functional>

#define VERIFY_NO_ERROR(errorInfo) \
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()))

#define WAIT_FOR_NEW_TIMESTAMP() waitForNewTimestamp(m_workingDataDir)

class LogSink: public qbs::ILogSink
{
public:
    QString output;

    void doPrintWarning(const qbs::ErrorInfo &error) {
        qDebug("%s", qPrintable(error.toString()));
    }
    void doPrintMessage(qbs::LoggerLevel, const QString &message, const QString &) {
        output += message;
    }
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

class ProcessResultReceiver : public QObject
{
    Q_OBJECT
public:
    QString output;

private slots:
    void handleProcessResult(const qbs::ProcessResult &result) {
        output += result.stdErr().join(QLatin1Char('\n'));
        output += result.stdOut().join(QLatin1Char('\n'));
    }
};

class TaskReceiver : public QObject
{
    Q_OBJECT
public:
    QString taskDescriptions;

private slots:
    void handleTaskStart(const QString &task) { taskDescriptions += task; }
};


static void removeBuildDir(const qbs::SetupProjectParameters &params)
{
    QString message;
    const QString dir = params.buildRoot() + '/' + params.topLevelProfile()
            + '-' + params.buildVariant();
    if (!qbs::Internal::removeDirectoryWithContents(dir, &message))
        qFatal("Could not remove build dir: %s", qPrintable(message));
}

static bool waitForFinished(qbs::AbstractJob *job, int timeout = 0)
{
    if (job->state() == qbs::AbstractJob::StateFinished)
        return true;
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


TestApi::TestApi()
    : m_logSink(new LogSink)
    , m_sourceDataDir(QDir::cleanPath(SRCDIR "/testdata"))
    , m_workingDataDir(testWorkDir(QStringLiteral("api")))
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

void TestApi::addQObjectMacroToCppFile()
{
    BuildDescriptionReceiver receiver;
    qbs::ErrorInfo errorInfo = doBuildProject("add-qobject-macro-to-cpp-file/project.qbs", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY2(!receiver.descriptions.contains("moc"), qPrintable(receiver.descriptions));
    receiver.descriptions.clear();

    WAIT_FOR_NEW_TIMESTAMP();
    QFile cppFile("object.cpp");
    QVERIFY2(cppFile.open(QIODevice::ReadWrite), qPrintable(cppFile.errorString()));
    QByteArray contents = cppFile.readAll();
    contents.replace("// ", "");
    cppFile.resize(0);
    cppFile.write(contents);
    cppFile.close();
    errorInfo = doBuildProject("add-qobject-macro-to-cpp-file/project.qbs", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY2(receiver.descriptions.contains("moc"), qPrintable(receiver.descriptions));
}

static bool isAboutUndefinedSymbols(const QString &_message)
{
    const QString message = _message.toLower();
    return message.contains("undefined") || message.contains("unresolved");
}

void TestApi::addedFilePersistent()
{
    // On the initial run, linking will fail.
    const QString relProjectFilePath = "added-file-persistent/project.qbs";
    ProcessResultReceiver receiver;
    qbs::ErrorInfo errorInfo = doBuildProject(relProjectFilePath, 0, &receiver);
    QVERIFY(errorInfo.hasError());
    QVERIFY2(isAboutUndefinedSymbols(receiver.output), qPrintable((receiver.output)));
    receiver.output.clear();

    // Add a file. qbs must schedule it for rule application on the next build.
    WAIT_FOR_NEW_TIMESTAMP();
    const qbs::SetupProjectParameters params = defaultSetupParameters(relProjectFilePath);
    QFile projectFile(params.projectFilePath());
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    const QByteArray originalContent = projectFile.readAll();
    QByteArray addedFileContent = originalContent;
    addedFileContent.replace("/* 'file.cpp' */", "'file.cpp'");
    projectFile.resize(0);
    projectFile.write(addedFileContent);
    projectFile.flush();
    QScopedPointer<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(params, m_logSink,
                                                                              0));
    waitForFinished(setupJob.data());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    setupJob.reset(0);

    // Remove the file again. qbs must unschedule the rule application again.
    // Consequently, the linking step must fail as in the initial run.
    WAIT_FOR_NEW_TIMESTAMP();
    projectFile.resize(0);
    projectFile.write(originalContent);
    projectFile.flush();
    errorInfo = doBuildProject(relProjectFilePath, 0, &receiver);
    QVERIFY(errorInfo.hasError());
    QVERIFY2(isAboutUndefinedSymbols(receiver.output), qPrintable((receiver.output)));

    // Add the file again. qbs must schedule it for rule application on the next build.
    WAIT_FOR_NEW_TIMESTAMP();
    projectFile.resize(0);
    projectFile.write(addedFileContent);
    projectFile.close();
    setupJob.reset(qbs::Project().setupProject(params, m_logSink, 0));
    waitForFinished(setupJob.data());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    setupJob.reset(0);

    // qbs must remember that a file was scheduled for rule application. The build must then
    // succeed, as now all necessary symbols are linked in.
    errorInfo = doBuildProject(relProjectFilePath);
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::baseProperties()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("base-properties/prj.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::buildGraphLocking()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("buildgraph-locking/project.qbs");
    QScopedPointer<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(setupJob.data());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    const qbs::Project project = setupJob->project();
    Q_UNUSED(project);
    setupJob.reset(qbs::Project().setupProject(setupParams, m_logSink, 0));
    waitForFinished(setupJob.data());
    QVERIFY(setupJob->error().hasError());
    QVERIFY2(setupJob->error().toString().contains("lock"),
             qPrintable(setupJob->error().toString()));
}

void TestApi::buildProject()
{
    QFETCH(QString, projectSubDir);
    QFETCH(QString, productFileName);
    qbs::SetupProjectParameters params = defaultSetupParameters(projectSubDir + "/project.qbs");
    removeBuildDir(params);
    qbs::ErrorInfo errorInfo = doBuildProject(projectSubDir + "/project.qbs");
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(regularFileExists(relativeBuildGraphFilePath()));
    if (!productFileName.isEmpty()) {
        QVERIFY2(regularFileExists(productFileName), qPrintable(productFileName));
        QVERIFY2(QFile::remove(productFileName), qPrintable(productFileName));
    }

    WAIT_FOR_NEW_TIMESTAMP();
    qbs::BuildOptions options;
    options.setForceTimestampCheck(true);
    errorInfo = doBuildProject(projectSubDir + "/project.qbs", 0, 0, 0, options);
    VERIFY_NO_ERROR(errorInfo);
    if (!productFileName.isEmpty())
        QVERIFY2(regularFileExists(productFileName), qPrintable(productFileName));
    QVERIFY(regularFileExists(relativeBuildGraphFilePath()));
}

void TestApi::buildProject_data()
{
    QTest::addColumn<QString>("projectSubDir");
    QTest::addColumn<QString>("productFileName");
    QTest::newRow("BPs in Sources")
            << QString("build-properties-source")
            << relativeExecutableFilePath("HelloWorld");
    QTest::newRow("code generator")
            << QString("codegen")
            << relativeExecutableFilePath("codegen");
    QTest::newRow("link static libs")
            << QString("link-static-lib")
            << relativeExecutableFilePath("HelloWorld");
    QTest::newRow("precompiled header")
            << QString("precompiled-header")
            << relativeExecutableFilePath("MyApp");
    QTest::newRow("lots of dots")
            << QString("lots-of-dots")
            << relativeExecutableFilePath("lots.of.dots");
    QTest::newRow("Qt5 plugin")
            << QString("qt5-plugin")
            << relativeProductBuildDir("echoplugin") + '/'
               + qbs::Internal::HostOsInfo::dynamicLibraryName("echoplugin");
    QTest::newRow("Q_OBJECT in source")
            << QString("moc-cpp")
            << relativeExecutableFilePath("moc_cpp");
    QTest::newRow("Q_OBJECT in header")
            << QString("moc-hpp")
            << relativeExecutableFilePath("moc_hpp");
    QTest::newRow("Q_OBJECT in header, moc_XXX.cpp included")
            << QString("moc-hpp-included")
            << relativeExecutableFilePath("moc_hpp_included");
    QTest::newRow("app and lib with same source file")
            << QString("lib-same-source")
            << relativeExecutableFilePath("HelloWorldApp");
    QTest::newRow("source files with the same base name but different extensions")
            << QString("same-base-name")
            << relativeExecutableFilePath("basename");
    QTest::newRow("static library dependencies")
            << QString("static-lib-deps")
            << relativeExecutableFilePath("staticLibDeps");
    QTest::newRow("simple probes")
            << QString("simple-probe")
            << relativeExecutableFilePath("MyApp");
    QTest::newRow("application without sources")
            << QString("app-without-sources")
            << relativeExecutableFilePath("appWithoutSources");
    QTest::newRow("productNameWithDots")
            << QString("productNameWithDots")
            << relativeExecutableFilePath("myapp");
    QTest::newRow("only default properties")
            << QString("two-default-property-values")
            << relativeProductBuildDir("two-default-property-values") + "/set";
    QTest::newRow("Export item with Group")
            << QString("export-item-with-group")
            << relativeExecutableFilePath("app");
    QTest::newRow("QBS-728")
            << QString("QBS-728")
            << QString();
}

void TestApi::buildProjectDryRun()
{
    QFETCH(QString, projectSubDir);
    QFETCH(QString, productFileName);
    qbs::SetupProjectParameters params = defaultSetupParameters(projectSubDir + "/project.qbs");
    removeBuildDir(params);
    qbs::BuildOptions options;
    options.setDryRun(true);
    const qbs::ErrorInfo errorInfo
            = doBuildProject(projectSubDir + "/project.qbs", 0, 0, 0, options);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY2(!QFileInfo::exists(relativeBuildDir()), qPrintable(QDir(relativeBuildDir())
            .entryList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::System).join(", ")));
}

void TestApi::buildProjectDryRun_data()
{
    return buildProject_data();
}

void TestApi::buildSingleFile()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("build-single-file/project.qbs");
    QScopedPointer<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                              m_logSink, 0));
    waitForFinished(setupJob.data());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    qbs::BuildOptions options;
    options.setDryRun(true);
    options.setFilesToConsider(QStringList(setupParams.buildRoot() + "/compiled.cpp"));
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

#ifdef QBS_ENABLE_PROJECT_FILE_UPDATES

static qbs::Project::ProductSelection defaultProducts()
{
    return qbs::Project::ProductSelectionDefaultOnly;
}

static void printProjectData(const qbs::ProjectData &project)
{
    foreach (const qbs::ProductData &p, project.products()) {
        qDebug("    Product '%s' at %s", qPrintable(p.name()), qPrintable(p.location().toString()));
        foreach (const qbs::GroupData &g, p.groups()) {
            qDebug("        Group '%s' at %s", qPrintable(g.name()), qPrintable(g.location().toString()));
            qDebug("            Files: %s", qPrintable(g.allFilePaths().join(QLatin1String(", "))));
        }
    }
}

void TestApi::changeContent()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("project-editing/project.qbs");
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    qbs::Project project = job->project();
    qbs::ProjectData projectData = project.projectData();
    QCOMPARE(projectData.allProducts().count(), 1);
    qbs::ProductData product = projectData.allProducts().first();
    QCOMPARE(product.groups().count(), 8);

    // Error handling: Invalid product.
    qbs::ErrorInfo errorInfo = project.addGroup(qbs::ProductData(), "blubb");
    QVERIFY(errorInfo.hasError());
    QVERIFY(errorInfo.toString().contains("invalid"));

    // Error handling: Empty group name.
    errorInfo = project.addGroup(product, QString());
    QVERIFY(errorInfo.hasError());
    QVERIFY(errorInfo.toString().contains("empty"));

    errorInfo = project.addGroup(product, "New Group 1");
    VERIFY_NO_ERROR(errorInfo);

    errorInfo = project.addGroup(product, "New Group 2");
    VERIFY_NO_ERROR(errorInfo);

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
    QCOMPARE(product.groups().count(), 10);
    qbs::GroupData group = findGroup(product, "New Group 1");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "file.h" << "file.cpp");
    VERIFY_NO_ERROR(errorInfo);

    // Error handling: Add the same file again.
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    QCOMPARE(product.groups().count(), 10);
    group = findGroup(product, "New Group 1");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "file.cpp");
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("already"), qPrintable(errorInfo.toString()));

    // Remove one of the newly added files again.
    errorInfo = project.removeFiles(product, group, QStringList("file.h"));
    VERIFY_NO_ERROR(errorInfo);

    // Error handling: Try to remove the same file again.
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    QCOMPARE(product.groups().count(), 10);
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
    VERIFY_NO_ERROR(errorInfo);

    // Add file to non-empty array literal.
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    group = findGroup(product, "Existing Group 1");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "newfile1.txt");
    VERIFY_NO_ERROR(errorInfo);

    // Add files to list represented as a single string.
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    errorInfo = project.addFiles(product, qbs::GroupData(), QStringList() << "newfile2.txt");
    VERIFY_NO_ERROR(errorInfo);

    // Add files to list represented as an identifier.
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    group = findGroup(product, "Existing Group 2");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "newfile3.txt");
    VERIFY_NO_ERROR(errorInfo);

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
    VERIFY_NO_ERROR(errorInfo);

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
    VERIFY_NO_ERROR(errorInfo);
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    QCOMPARE(projectData.products().first().groups().count(), 9);

    // Error handling: Try to remove the same group again.
    errorInfo = project.removeGroup(product, group);
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("does not exist"), qPrintable(errorInfo.toString()));

    // Add a file to a group where the file name is already matched by a wildcard.
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    group = findGroup(product, "Group with wildcards");
    QVERIFY(group.isValid());
    QFile newFile("koerper.klaus");
    QVERIFY2(newFile.open(QIODevice::WriteOnly), qPrintable(newFile.errorString()));
    newFile.close();
    errorInfo = project.addFiles(product, group, QStringList() << newFile.fileName());
    VERIFY_NO_ERROR(errorInfo);
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    group = findGroup(product, "Group with wildcards");
    QVERIFY(group.isValid());
    QCOMPARE(group.sourceArtifactsFromWildcards().count(), 1);
    QCOMPARE(group.sourceArtifactsFromWildcards().first().filePath(),
             QFileInfo(newFile).absoluteFilePath());

    // Error checking: Try to remove a file that originates from a wildcard pattern.
    projectData = project.projectData();
    QVERIFY(projectData.products().count() == 1);
    product = projectData.products().first();
    group = findGroup(product, "Other group with wildcards");
    QVERIFY(group.isValid());
    errorInfo = project.removeFiles(product, group, QStringList() << "test.wildcard");
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("pattern"), qPrintable(errorInfo.toString()));

    // Check whether building will take the added and removed cpp files into account.
    // This must not be moved below the re-resolving test!!!
    qbs::BuildOptions buildOptions;
    buildOptions.setDryRun(true);
    BuildDescriptionReceiver rcvr;
    QScopedPointer<qbs::BuildJob> buildJob(project.buildAllProducts(buildOptions, defaultProducts(),
                                                                    this));
    connect(buildJob.data(), SIGNAL(reportCommandDescription(QString,QString)), &rcvr,
            SLOT(handleDescription(QString,QString)));
    waitForFinished(buildJob.data());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    QVERIFY(rcvr.descriptions.contains("compiling file.cpp"));
    QVERIFY(!rcvr.descriptions.contains("compiling main.cpp"));

    // Now check whether the data updates were done correctly.
    projectData = project.projectData();
    job.reset(project.setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    project = job->project();
    qbs::ProjectData newProjectData = project.projectData();

    // Can't use Project::operator== here, as the target artifacts will differ due to the build
    // not having run yet.
    bool projectDataMatches = newProjectData.products().count() == 1
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
    buildJob.reset(project.buildAllProducts(buildOptions, defaultProducts(), this));
    connect(buildJob.data(), SIGNAL(reportCommandDescription(QString,QString)), &rcvr,
            SLOT(handleDescription(QString,QString)));
    waitForFinished(buildJob.data());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    QVERIFY(rcvr.descriptions.contains("compiling file.cpp"));
    QVERIFY(!rcvr.descriptions.contains("compiling main.cpp"));

    // Now, after the build, the project data must be entirely identical.
    QVERIFY(projectData == project.projectData());

    // Error handling: Try to change the project during a build.
    buildJob.reset(project.buildAllProducts(buildOptions, defaultProducts(), this));
    errorInfo = project.addGroup(newProjectData.products().first(), "blubb");
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("in process"), qPrintable(errorInfo.toString()));
    waitForFinished(buildJob.data());
    errorInfo = project.addGroup(newProjectData.products().first(), "blubb");
    VERIFY_NO_ERROR(errorInfo);

    project = qbs::Project();
    job.reset(0);
    buildJob.reset(0);
    removeBuildDir(setupParams);
    // Add a file to the top level of a product that does not have a "files" binding yet.
    setupParams.setProjectFilePath(QDir::cleanPath(m_workingDataDir +
        "/project-editing/project-with-no-files.qbs"));

    job.reset(project.setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    project = job->project();
    projectData = project.projectData();
    QCOMPARE(projectData.allProducts().count(), 1);
    product = projectData.allProducts().first();
    errorInfo = project.addFiles(product, qbs::GroupData(), QStringList("main.cpp"));
    VERIFY_NO_ERROR(errorInfo);
    projectData = project.projectData();
    rcvr.descriptions.clear();
    buildJob.reset(project.buildAllProducts(buildOptions, defaultProducts(), this));
    connect(buildJob.data(), SIGNAL(reportCommandDescription(QString,QString)), &rcvr,
            SLOT(handleDescription(QString,QString)));
    waitForFinished(buildJob.data());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    QVERIFY(rcvr.descriptions.contains("compiling main.cpp"));
    job.reset(project.setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    // Can't use Project::operator== here, as the target artifacts will differ due to the build
    // not having run yet.
    newProjectData = job->project().projectData();
    projectDataMatches = newProjectData.products().count() == 1
            && projectData.products().count() == 1
            && newProjectData.products().first().groups() == projectData.products().first().groups();
    if (!projectDataMatches) {
        printProjectData(projectData);
        qDebug("\n====\n");
        printProjectData(newProjectData);
    }
    QVERIFY(projectDataMatches);
}

#endif // QBS_ENABLE_PROJECT_FILE_UPDATES

void TestApi::commandExtraction()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("/command-extraction/project.qbs");
    QScopedPointer<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                              m_logSink, 0));
    waitForFinished(setupJob.data());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    qbs::ProjectData projectData = project.projectData();
    QCOMPARE(projectData.allProducts().count(), 1);
    qbs::ProductData productData = projectData.allProducts().first();
    qbs::ErrorInfo errorInfo;
    const QString projectDirPath = QDir::cleanPath(QFileInfo(setupParams.projectFilePath()).path());
    const QString sourceFilePath = projectDirPath + "/main.cpp";

    // Before the first build, no rules exist.
    qbs::RuleCommandList commands
            = project.ruleCommands(productData, sourceFilePath, "obj", &errorInfo);
    QCOMPARE(commands.count(), 0);
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("No rule"), qPrintable(errorInfo.toString()));

    qbs::BuildOptions options;
    options.setDryRun(true);
    QScopedPointer<qbs::BuildJob> buildJob(project.buildAllProducts(options));
    waitForFinished(buildJob.data());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    projectData = project.projectData();
    QCOMPARE(projectData.allProducts().count(), 1);
    productData = projectData.allProducts().first();
    errorInfo = qbs::ErrorInfo();

    // After the build, the compile command must be found.
    commands = project.ruleCommands(productData, sourceFilePath, "obj", &errorInfo);
    QCOMPARE(commands.count(), 1);
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()));
    const qbs::RuleCommand command = commands.first();
    QCOMPARE(command.type(), qbs::RuleCommand::ProcessCommandType);
    QVERIFY(!command.executable().isEmpty());
    QVERIFY(!command.arguments().isEmpty());
}

void TestApi::changeDependentLib()
{
    qbs::ErrorInfo errorInfo = doBuildProject("change-dependent-lib/change-dependent-lib.qbs");
    VERIFY_NO_ERROR(errorInfo);

    WAIT_FOR_NEW_TIMESTAMP();
    const QString qbsFileName("change-dependent-lib.qbs");
    QFile qbsFile(qbsFileName);
    QVERIFY(qbsFile.open(QIODevice::ReadWrite));
    const QByteArray content1 = qbsFile.readAll();
    QByteArray content2 = content1;
    content2.replace("cpp.defines: [\"XXXX\"]", "cpp.defines: [\"ABCD\"]");
    QVERIFY(content1 != content2);
    qbsFile.seek(0);
    qbsFile.write(content2);
    qbsFile.close();
    errorInfo = doBuildProject("change-dependent-lib/change-dependent-lib.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::enableAndDisableProduct()
{
    BuildDescriptionReceiver bdr;
    qbs::ErrorInfo errorInfo = doBuildProject("enable-and-disable-product/project.qbs", &bdr);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(!bdr.descriptions.contains("compiling"));

    WAIT_FOR_NEW_TIMESTAMP();
    QFile projectFile("project.qbs");
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    QByteArray content = projectFile.readAll();
    content.replace("undefined", "'hidden'");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    bdr.descriptions.clear();
    errorInfo = doBuildProject("enable-and-disable-product/project.qbs", &bdr);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(bdr.descriptions.contains("linking"));

    WAIT_FOR_NEW_TIMESTAMP();
    touch("main.cpp");
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    content = projectFile.readAll();
    content.replace("'hidden'", "undefined");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    bdr.descriptions.clear();
    errorInfo = doBuildProject("enable-and-disable-product/project.qbs", &bdr);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(!bdr.descriptions.contains("compiling"));
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
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("disabled_install_group/project.qbs");
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    const qbs::Project project = job->project();

    const qbs::ErrorInfo errorInfo = forceRuleEvaluation(project);
    VERIFY_NO_ERROR(errorInfo);

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

void TestApi::disabledProduct()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("disabled-product/disabledProduct.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::disabledProject()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("disabled-project/disabled_project.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::duplicateProductNames()
{
    QFETCH(QString, projectFileName);
    const qbs::ErrorInfo errorInfo = doBuildProject("duplicate-product-names/" + projectFileName);
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("Duplicate product name"),
             qPrintable(errorInfo.toString()));
}

void TestApi::duplicateProductNames_data()
{
    QTest::addColumn<QString>("projectFileName");
    QTest::newRow("Names explicitly set") << QString("explicit.qbs");
    QTest::newRow("Unnamed products in same file") << QString("implicit.qbs");
    QTest::newRow("Unnamed products in files of the same name") << QString("implicit-indirect.qbs");

}

void TestApi::dynamicLibs()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("dynamic-libs/link_dynamiclib.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::emptyFileTagList()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("empty-filetag-list/project.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::emptySubmodulesList()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("empty-submodules-list/project.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::explicitlyDependsOn()
{
    BuildDescriptionReceiver receiver;
    qbs::ErrorInfo errorInfo = doBuildProject("explicitly-depends-on/project.qbs", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(receiver.descriptions.contains("Creating output artifact"));
    receiver.descriptions.clear();

    errorInfo = doBuildProject("explicitly-depends-on/project.qbs", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(!receiver.descriptions.contains("Creating output artifact"));

    WAIT_FOR_NEW_TIMESTAMP();
    touch("dependency.txt");
    errorInfo = doBuildProject("explicitly-depends-on/project.qbs", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(receiver.descriptions.contains("Creating output artifact"));
}

void TestApi::exportSimple()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("export-simple/project.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::exportWithRecursiveDepends()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("export-with-recursive-depends/project.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::fileTagger()
{
    BuildDescriptionReceiver receiver;
    const qbs::ErrorInfo errorInfo = doBuildProject("file-tagger/moc_cpp.qbs", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY2(receiver.descriptions.contains("moc bla.cpp"), qPrintable(receiver.descriptions));
}

void TestApi::fileTagsFilterOverride()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("filetagsfilter_override/project.qbs");
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                         m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    qbs::Project project = job->project();

    const qbs::ErrorInfo errorInfo = forceRuleEvaluation(project);
    VERIFY_NO_ERROR(errorInfo);

    qbs::ProjectData projectData = project.projectData();
    QCOMPARE(projectData.allProducts().count(), 1);
    const qbs::ProductData product = projectData.allProducts().first();
    QList<qbs::InstallableFile> installableFiles
            = project.installableFilesForProduct(product, qbs::InstallOptions());
    QCOMPARE(installableFiles.count(), 1);
    QVERIFY(installableFiles.first().targetFilePath().contains("habicht"));
}

void TestApi::infiniteLoopBuilding()
{
    QFETCH(QString, projectDirName);
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters(projectDirName + "/infinite-loop.qbs");
    QScopedPointer<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                              m_logSink, 0));
    waitForFinished(setupJob.data());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    const QScopedPointer<qbs::BuildJob> buildJob(project.buildAllProducts(qbs::BuildOptions()));
    QTimer::singleShot(1000, buildJob.data(), SLOT(cancel()));
    QVERIFY(waitForFinished(buildJob.data(), 600000));
}

void TestApi::infiniteLoopBuilding_data()
{
    QTest::addColumn<QString>("projectDirName");
    QTest::newRow("JS Command") << QString("infinite-loop-js");
    QTest::newRow("Process Command") << QString("infinite-loop-process");
}

void TestApi::infiniteLoopResolving()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("infinite-loop-resolving/project.qbs");
    QScopedPointer<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                              m_logSink, 0));
    QTimer::singleShot(1000, setupJob.data(), SLOT(cancel()));
    QVERIFY(waitForFinished(setupJob.data(), 600000));
    QVERIFY2(setupJob->error().toString().toLower().contains("cancel"),
             qPrintable(setupJob->error().toString()));
}

void TestApi::inheritQbsSearchPaths()
{
    const QString projectFilePath = "inherit-qbs-search-paths/prj.qbs";
    qbs::ErrorInfo errorInfo = doBuildProject(projectFilePath);
    VERIFY_NO_ERROR(errorInfo);

    WAIT_FOR_NEW_TIMESTAMP();
    QFile projectFile(m_workingDataDir + '/' + projectFilePath);
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    QByteArray content = projectFile.readAll();
    content.replace("qbsSearchPaths: \"subdir\"", "//qbsSearchPaths: \"subdir\"");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    errorInfo = doBuildProject(projectFilePath);
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("Product dependency 'bli' not found"),
             qPrintable(errorInfo.toString()));

    QVariantMap overriddenValues;
    overriddenValues.insert("project.qbsSearchPaths", QStringList() << "subdir");
    errorInfo = doBuildProject(projectFilePath, 0, 0, 0, qbs::BuildOptions(), overriddenValues);
    VERIFY_NO_ERROR(errorInfo);
}

template <typename T, class Pred> T findElem(const QList<T> &list, Pred p)
{
    const auto it = std::find_if(list.constBegin(), list.constEnd(), p);
    return it == list.constEnd() ? T() : *it;
}

void TestApi::installableFiles()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("installed-artifact/installed_artifact.qbs");
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                         m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    qbs::Project project = job->project();

    const qbs::ErrorInfo errorInfo = forceRuleEvaluation(project);
    VERIFY_NO_ERROR(errorInfo);

    qbs::ProjectData projectData = project.projectData();
    QCOMPARE(projectData.allProducts().count(), 2);
    qbs::ProductData product = findElem(projectData.allProducts(), [](const qbs::ProductData &p) {
        return p.name() == QLatin1String("installedApp");
    });
    QVERIFY(product.isValid());
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

    setupParams  = defaultSetupParameters("recursive-wildcards/recursive_wildcards.qbs");
    job.reset(project.setupProject(setupParams, m_logSink, 0));
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

void TestApi::isRunnable()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("is-runnable/project.qbs");
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    qbs::Project project = job->project();
    const QList<qbs::ProductData> products = project.projectData().products();
    QCOMPARE(products.count(), 2);
    foreach (const qbs::ProductData &p, products) {
        QVERIFY2(p.name() == "app" || p.name() == "lib", qPrintable(p.name()));
        if (p.name() == "app")
            QVERIFY(p.isRunnable());
        else
            QVERIFY(!p.isRunnable());
    }
}

void TestApi::listBuildSystemFiles()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("subprojects/toplevelproject.qbs");
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    const QSet<QString> buildSystemFiles = job->project().buildSystemFiles();
    QVERIFY(buildSystemFiles.contains(setupParams.projectFilePath()));
    QVERIFY(buildSystemFiles.contains(setupParams.buildRoot() + "/subproject2/subproject2.qbs"));
    QVERIFY(buildSystemFiles.contains(setupParams.buildRoot()
                                      + "/subproject2/subproject3/subproject3.qbs"));
}

void TestApi::mocCppIncluded()
{
    // Initial build.
    qbs::ErrorInfo errorInfo = doBuildProject("moc-hpp-included/project.qbs");
    VERIFY_NO_ERROR(errorInfo);

    // Touch header and try again.
    WAIT_FOR_NEW_TIMESTAMP();
    QFile headerFile("object.h");
    QVERIFY2(headerFile.open(QIODevice::WriteOnly | QIODevice::Append),
             qPrintable(headerFile.errorString()));
    headerFile.write("\n");
    headerFile.close();
    errorInfo = doBuildProject("moc-hpp-included/project.qbs");
    VERIFY_NO_ERROR(errorInfo);

    // Touch cpp file and try again.
    WAIT_FOR_NEW_TIMESTAMP();
    QFile cppFile("object.cpp");
    QVERIFY2(cppFile.open(QIODevice::WriteOnly | QIODevice::Append),
             qPrintable(cppFile.errorString()));
    cppFile.write("\n");
    cppFile.close();
    errorInfo = doBuildProject("moc-hpp-included/project.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::multiArch()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("multi-arch/project.qbs");
    qbs::Settings settings((QString()));
    qbs::Internal::TemporaryProfile tph("host", &settings);
    qbs::Profile hostProfile = tph.p;
    hostProfile.setValue("qbs.architecture", "host-arch");
    qbs::Internal::TemporaryProfile tpt("target", &settings);
    qbs::Profile targetProfile = tpt.p;
    targetProfile.setValue("qbs.architecture", "target-arch");
    QVariantMap overriddenValues;
    overriddenValues.insert("project.hostProfile", hostProfile.name());
    overriddenValues.insert("project.targetProfile", targetProfile.name());
    setupParams.setOverriddenValues(overriddenValues);
    QScopedPointer<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(setupJob.data());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    QCOMPARE(project.profile(), profileName());
    const QList<qbs::ProductData> &products = project.projectData().products();
    QCOMPARE(products.count(), 3);
    QList<qbs::ProductData> hostProducts;
    QList<qbs::ProductData> targetProducts;
    foreach (const qbs::ProductData &p, products) {
        QVERIFY2(p.profile() == hostProfile.name() || p.profile() == targetProfile.name(),
                 qPrintable(p.profile()));
        if (p.profile() == hostProfile.name())
            hostProducts << p;
        else
            targetProducts << p;
    }
    QCOMPARE(hostProducts.count(), 2);
    QCOMPARE(targetProducts.count(), 1);
    QCOMPARE(targetProducts.first().name(), QLatin1String("p1"));
    QStringList hostProductNames
            = QStringList() << hostProducts.first().name() << hostProducts.last().name();
    QCOMPARE(hostProductNames.count("p1"), 1);
    QCOMPARE(hostProductNames.count("p2"), 1);

    QScopedPointer<qbs::BuildJob> buildJob(project.buildAllProducts(qbs::BuildOptions()));
    waitForFinished(buildJob.data());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    const QString outputBaseDir = setupParams.buildRoot() + '/';
    QFile p1HostArtifact(outputBaseDir
                         + relativeProductBuildDir("p1", "host") + "/host+target.output");
    QVERIFY2(p1HostArtifact.exists(), qPrintable(p1HostArtifact.fileName()));
    QVERIFY2(p1HostArtifact.open(QIODevice::ReadOnly), qPrintable(p1HostArtifact.errorString()));
    QCOMPARE(p1HostArtifact.readAll().constData(), "host-arch");
    QFile p1TargetArtifact(outputBaseDir + relativeProductBuildDir("p1", "target")
                           + "/host+target.output");
    QVERIFY2(p1TargetArtifact.exists(), qPrintable(p1TargetArtifact.fileName()));
    QVERIFY2(p1TargetArtifact.open(QIODevice::ReadOnly), qPrintable(p1TargetArtifact.errorString()));
    QCOMPARE(p1TargetArtifact.readAll().constData(), "target-arch");
    QFile p2Artifact(outputBaseDir + relativeProductBuildDir("p2", "host") + "/host-tool.output");
    QVERIFY2(p2Artifact.exists(), qPrintable(p2Artifact.fileName()));
    QVERIFY2(p2Artifact.open(QIODevice::ReadOnly), qPrintable(p2Artifact.errorString()));
    QCOMPARE(p2Artifact.readAll().constData(), "host-arch");

    const QString installRoot = outputBaseDir + relativeBuildDir() + '/'
            + qbs::InstallOptions::defaultInstallRoot();
    QScopedPointer<qbs::InstallJob> installJob(project.installAllProducts(qbs::InstallOptions()));
    waitForFinished(installJob.data());
    QVERIFY2(!installJob->error().hasError(), qPrintable(installJob->error().toString()));
    QFile p1HostArtifactInstalled(installRoot + "/host/host+target.output");
    QVERIFY2(p1HostArtifactInstalled.exists(), qPrintable(p1HostArtifactInstalled.fileName()));
    QFile p1TargetArtifactInstalled(installRoot + "/target/host+target.output");
    QVERIFY2(p1TargetArtifactInstalled.exists(), qPrintable(p1TargetArtifactInstalled.fileName()));
    QFile p2ArtifactInstalled(installRoot + "/host/host-tool.output");
    QVERIFY2(p2ArtifactInstalled.exists(), qPrintable(p2ArtifactInstalled.fileName()));

    // Error check: Try to build for the same profile twice.
    overriddenValues.insert("project.targetProfile", hostProfile.name());
    setupParams.setOverriddenValues(overriddenValues);
    setupJob.reset(project.setupProject(setupParams, m_logSink, 0));
    waitForFinished(setupJob.data());
    QVERIFY(setupJob->error().hasError());
    QVERIFY2(setupJob->error().toString().contains(hostProfile.name())
             && setupJob->error().toString().contains("not allowed"),
             qPrintable(setupJob->error().toString()));

    // Error check: Try to build for the same profile twice, this time attaching
    // the properties via the product name.
    overriddenValues.clear();
    overriddenValues.insert("p1.profiles", targetProfile.name() + ',' + targetProfile.name());
    setupParams.setOverriddenValues(overriddenValues);
    setupJob.reset(project.setupProject(setupParams, m_logSink, 0));
    waitForFinished(setupJob.data());
    QVERIFY(setupJob->error().hasError());
    QVERIFY2(setupJob->error().toString().contains(targetProfile.name())
             && setupJob->error().toString().contains("not allowed"),
             qPrintable(setupJob->error().toString()));
}

void TestApi::newOutputArtifactInDependency()
{
    BuildDescriptionReceiver receiver;
    qbs::ErrorInfo errorInfo
            = doBuildProject("new-output-artifact-in-dependency/project.qbs", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(receiver.descriptions.contains("linking app"));
    const QByteArray linkingLibString = QByteArray("linking ")
            + qbs::Internal::HostOsInfo::dynamicLibraryName("lib").toLatin1();
    QVERIFY(!receiver.descriptions.contains(linkingLibString));
    receiver.descriptions.clear();

    WAIT_FOR_NEW_TIMESTAMP();
    QFile projectFile("project.qbs");
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    QByteArray contents = projectFile.readAll();
    contents.replace("//Depends", "Depends");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    errorInfo = doBuildProject("new-output-artifact-in-dependency/project.qbs", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(receiver.descriptions.contains("linking app"));
    QVERIFY(receiver.descriptions.contains(linkingLibString));
}

void TestApi::newPatternMatch()
{
    TaskReceiver receiver;
    qbs::ErrorInfo errorInfo = doBuildProject("new-pattern-match/project.qbs", 0, 0, &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY2(receiver.taskDescriptions.contains("Resolving"), qPrintable(m_logSink->output));
    receiver.taskDescriptions.clear();

    errorInfo = doBuildProject("new-pattern-match/project.qbs", 0, 0, &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(!receiver.taskDescriptions.contains("Resolving"));

    QFile f("test.txt");
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(f.errorString()));
    f.close();
    errorInfo = doBuildProject("new-pattern-match/project.qbs", 0, 0, &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(receiver.taskDescriptions.contains("Resolving"));
    receiver.taskDescriptions.clear();

    errorInfo = doBuildProject("new-pattern-match/project.qbs", 0, 0, &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(!receiver.taskDescriptions.contains("Resolving"));

    f.remove();
    errorInfo = doBuildProject("new-pattern-match/project.qbs", 0, 0, &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(receiver.taskDescriptions.contains("Resolving"));
}

void TestApi::nonexistingProjectPropertyFromProduct()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("nonexistingprojectproperties/invalidaccessfromproduct.qbs");
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QEXPECT_FAIL("", "QBS-432", Abort);
    QVERIFY(job->error().hasError());
    QVERIFY2(job->error().toString().contains(QLatin1String("blubb")),
             qPrintable(job->error().toString()));
}

void TestApi::nonexistingProjectPropertyFromCommandLine()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("nonexistingprojectproperties/project.qbs");
    removeBuildDir(setupParams);
    QVariantMap projectProperties;
    projectProperties.insert(QLatin1String("project.blubb"), QLatin1String("true"));
    setupParams.setOverriddenValues(projectProperties);
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY(job->error().hasError());
    QVERIFY2(job->error().toString().contains(QLatin1String("blubb")),
             qPrintable(job->error().toString()));
}

void TestApi::objC()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("objc/objc.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::projectInvalidation()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("project-invalidation/project.qbs");
    QVERIFY(QFile::copy("project.no-error.qbs", "project.qbs"));
    QScopedPointer<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(setupJob.data());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    QVERIFY(project.isValid());
    WAIT_FOR_NEW_TIMESTAMP();
    copyFileAndUpdateTimestamp("project.early-error.qbs", "project.qbs");
    setupJob.reset(project.setupProject(setupParams, m_logSink, 0));
    waitForFinished(setupJob.data());
    QVERIFY(setupJob->error().hasError());
    QVERIFY(project.isValid()); // Error in Loader, old project still valid.
    WAIT_FOR_NEW_TIMESTAMP();
    copyFileAndUpdateTimestamp("project.late-error.qbs", "project.qbs");
    setupJob.reset(project.setupProject(setupParams, m_logSink, 0));
    waitForFinished(setupJob.data());
    QVERIFY(setupJob->error().hasError());
    QVERIFY(!project.isValid()); // Error in build data re-resolving, old project not valid anymore.
}

void TestApi::projectLocking()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("project-locking/project.qbs");
    QScopedPointer<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(setupJob.data());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    setupJob.reset(project.setupProject(setupParams, m_logSink, 0));
    QScopedPointer<qbs::SetupProjectJob> setupJob2(project.setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(setupJob2.data());
    QVERIFY(setupJob2->error().hasError());
    QVERIFY2(setupJob2->error().toString()
             .contains("Cannot start a job while another one is in progress."),
             qPrintable(setupJob2->error().toString()));
    waitForFinished(setupJob.data());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
}

void TestApi::projectPropertiesByName()
{
    const QString projectFile = "project-properties-by-name/project.qbs";
    qbs::ErrorInfo errorInfo = doBuildProject(projectFile);
    QVERIFY(errorInfo.hasError());
    QVariantMap overridden;
    overridden.insert("project.theDefines", QStringList() << "SUB1" << "SUB2");
    errorInfo = doBuildProject(projectFile, 0, 0, 0, qbs::BuildOptions(), overridden);
    QVERIFY(errorInfo.hasError());
    overridden.clear();
    overridden.insert("subproject1.theDefines", QStringList() << "SUB1");
    errorInfo = doBuildProject(projectFile, 0, 0, 0, qbs::BuildOptions(), overridden);
    QVERIFY(errorInfo.hasError());
    overridden.insert("subproject2.theDefines", QStringList() << "SUB2");
    errorInfo = doBuildProject(projectFile, 0, 0, 0, qbs::BuildOptions(), overridden);
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::projectWithPropertiesItem()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("project-with-properties-item/project.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::propertiesBlocks()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("properties-blocks/propertiesblocks.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::rc()
{
    BuildDescriptionReceiver receiver;
    const qbs::ErrorInfo errorInfo = doBuildProject("rc/rc.qbs", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    const bool rcFileWasCompiled = receiver.descriptions.contains("compiling test.rc");
    QCOMPARE(rcFileWasCompiled, qbs::Internal::HostOsInfo::isWindowsHost());
}

qbs::SetupProjectParameters TestApi::defaultSetupParameters(const QString &projectFilePath) const
{
    qbs::SetupProjectParameters setupParams;
    const QString projectDirPath = QDir::cleanPath(m_workingDataDir + QLatin1Char('/')
                                                   + QFileInfo(projectFilePath).path());
    setupParams.setProjectFilePath(projectDirPath + QLatin1Char('/')
                                   + QFileInfo(projectFilePath).fileName());
    setupParams.setPropertyCheckingMode(qbs::SetupProjectParameters::PropertyCheckingStrict);
    QDir::setCurrent(projectDirPath);
    setupParams.setBuildRoot(projectDirPath);
    qbs::Settings settings((QString()));
    const qbs::Preferences prefs(&settings, profileName());
    setupParams.setSearchPaths(prefs.searchPaths(QDir::cleanPath(QCoreApplication::applicationDirPath()
            + QLatin1String("/" QBS_RELATIVE_SEARCH_PATH))));
    setupParams.setPluginPaths(prefs.pluginPaths(QDir::cleanPath(QCoreApplication::applicationDirPath()
            + QLatin1String("/" QBS_RELATIVE_PLUGINS_PATH))));
    setupParams.setLibexecPath(QDir::cleanPath(QCoreApplication::applicationDirPath()
            + QLatin1String("/" QBS_RELATIVE_LIBEXEC_PATH)));
    setupParams.setTopLevelProfile(profileName());
    setupParams.setBuildVariant(QLatin1String("debug"));
    return setupParams;
}

void TestApi::references()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("references/invalid1.qbs");
    const QString projectDir = QDir::cleanPath(m_workingDataDir + "/references");
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY(job->error().hasError());
    QString errorString = job->error().toString();
    QVERIFY2(errorString.contains("does not contain"), qPrintable(errorString));

    setupParams.setProjectFilePath(projectDir + QLatin1String("/invalid2.qbs"));
    job.reset(qbs::Project().setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY(job->error().hasError());
    errorString = job->error().toString();
    QVERIFY2(errorString.contains("contains more than one"), qPrintable(errorString));

    setupParams.setProjectFilePath(projectDir + QLatin1String("/valid.qbs"));
    job.reset(qbs::Project().setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    const qbs::ProjectData topLevelProject = job->project().projectData();
    QCOMPARE(topLevelProject.subProjects().count(), 1);
    const QString subProjectFileName
            = QFileInfo(topLevelProject.subProjects().first().location().filePath()).fileName();
    QCOMPARE(subProjectFileName, QString("p.qbs"));
}

void TestApi::renameProduct()
{
    // Initial run.
    qbs::ErrorInfo errorInfo = doBuildProject("rename-product/rename.qbs");
    VERIFY_NO_ERROR(errorInfo);

    // Rename lib and adapt Depends item.
    WAIT_FOR_NEW_TIMESTAMP();
    QFile f("rename.qbs");
    QVERIFY(f.open(QIODevice::ReadWrite));
    QByteArray contents = f.readAll();
    contents.replace("TheLib", "thelib");
    f.resize(0);
    f.write(contents);
    f.close();
    errorInfo = doBuildProject("rename-product/rename.qbs");
    VERIFY_NO_ERROR(errorInfo);

    // Rename lib and don't adapt Depends item.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(f.open(QIODevice::ReadWrite));
    contents = f.readAll();
    const int libNameIndex = contents.lastIndexOf("thelib");
    QVERIFY(libNameIndex != -1);
    contents.replace(libNameIndex, 6, "TheLib");
    f.resize(0);
    f.write(contents);
    f.close();
    errorInfo = doBuildProject("rename-product/rename.qbs");
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("Product dependency 'thelib' not found"),
             qPrintable(errorInfo.toString()));
}

void TestApi::renameTargetArtifact()
{
    // Initial run.
    BuildDescriptionReceiver receiver;
    qbs::ErrorInfo errorInfo = doBuildProject("rename-target-artifact/rename.qbs", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY2(receiver.descriptions.contains("compiling"), qPrintable(receiver.descriptions));
    QCOMPARE(receiver.descriptions.count("linking"), 2);
    receiver.descriptions.clear();

    // Rename library file name.
    WAIT_FOR_NEW_TIMESTAMP();
    QFile f("rename.qbs");
    QVERIFY(f.open(QIODevice::ReadWrite));
    QByteArray contents = f.readAll();
    contents.replace("the_lib", "TheLib");
    f.resize(0);
    f.write(contents);
    f.close();
    errorInfo = doBuildProject("rename-target-artifact/rename.qbs", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY2(!receiver.descriptions.contains("compiling"), qPrintable(receiver.descriptions));
    QCOMPARE(receiver.descriptions.count("linking"), 2);
}

void TestApi::removeFileDependency()
{
    qbs::ErrorInfo errorInfo = doBuildProject("remove-file-dependency/removeFileDependency.qbs");
    VERIFY_NO_ERROR(errorInfo);

    QFile::remove("someheader.h");
    ProcessResultReceiver receiver;
    errorInfo = doBuildProject("remove-file-dependency/removeFileDependency.qbs", 0, &receiver);
    QVERIFY(errorInfo.hasError());
    QVERIFY2(receiver.output.contains("someheader.h"), qPrintable(receiver.output));
}

void TestApi::resolveProject()
{
    QFETCH(QString, projectSubDir);
    QFETCH(QString, productFileName);

    const qbs::SetupProjectParameters params
            = defaultSetupParameters(projectSubDir + "/project.qbs");
    removeBuildDir(params);
    const QScopedPointer<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(params,
                                                                                    m_logSink, 0));
    waitForFinished(setupJob.data());
    VERIFY_NO_ERROR(setupJob->error());
    QVERIFY2(!QFile::exists(productFileName), qPrintable(productFileName));
    QVERIFY(regularFileExists(relativeBuildGraphFilePath()));
}

void TestApi::resolveProject_data()
{
    return buildProject_data();
}

void TestApi::resolveProjectDryRun()
{
    QFETCH(QString, projectSubDir);
    QFETCH(QString, productFileName);

    qbs::SetupProjectParameters params = defaultSetupParameters(projectSubDir + "/project.qbs");
    params.setDryRun(true);
    removeBuildDir(params);
    const QScopedPointer<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(params,
                                                                                    m_logSink, 0));
    waitForFinished(setupJob.data());
    VERIFY_NO_ERROR(setupJob->error());
    QVERIFY2(!QFile::exists(productFileName), qPrintable(productFileName));
    QVERIFY(!regularFileExists(relativeBuildGraphFilePath()));
}

void TestApi::resolveProjectDryRun_data()
{
    return resolveProject_data();
}

void TestApi::softDependency()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("soft-dependency/project.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::sourceFileInBuildDir()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("source-file-in-build-dir/project.qbs");
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.data());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    const qbs::ProjectData projectData = job->project().projectData();
    QCOMPARE(projectData.allProducts().count(), 1);
    const qbs::ProductData product = projectData.allProducts().first();
    QCOMPARE(product.profile(), profileName());
    QCOMPARE(product.groups().count(), 1);
    const qbs::GroupData group = product.groups().first();
    QCOMPARE(group.allFilePaths().count(), 1);
}

void TestApi::subProjects()
{
    const qbs::SetupProjectParameters params
            = defaultSetupParameters("subprojects/toplevelproject.qbs");
    removeBuildDir(params);

    // Check all three types of subproject creation, plus property overrides.
    qbs::ErrorInfo errorInfo = doBuildProject("subprojects/toplevelproject.qbs");
    VERIFY_NO_ERROR(errorInfo);

    // Disabling both the project with the dependency and the one with the dependent
    // should not cause an error.
    WAIT_FOR_NEW_TIMESTAMP();
    QFile f(params.projectFilePath());
    QVERIFY(f.open(QIODevice::ReadWrite));
    QByteArray contents = f.readAll();
    contents.replace("condition: true", "condition: false");
    f.resize(0);
    f.write(contents);
    f.close();
    f.setFileName(params.buildRoot() + "/subproject2/subproject2.qbs");
    QVERIFY(f.open(QIODevice::ReadWrite));
    contents = f.readAll();
    contents.replace("condition: true", "condition: false");
    f.resize(0);
    f.write(contents);
    f.close();
    errorInfo = doBuildProject("subprojects/toplevelproject.qbs");
    VERIFY_NO_ERROR(errorInfo);

    // Disabling the project with the dependency only is an error.
    // This tests also whether changes in sub-projects are detected.
    WAIT_FOR_NEW_TIMESTAMP();
    f.setFileName(params.projectFilePath());
    QVERIFY(f.open(QIODevice::ReadWrite));
    contents = f.readAll();
    contents.replace("condition: false", "condition: true");
    f.resize(0);
    f.write(contents);
    f.close();
    errorInfo = doBuildProject("subprojects/toplevelproject.qbs");
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("Product dependency 'testLib' not found"),
             qPrintable(errorInfo.toString()));
}

void TestApi::trackAddQObjectHeader()
{
    const qbs::SetupProjectParameters params
            = defaultSetupParameters("missing-qobject-header/missingheader.qbs");
    QFile qbsFile(params.projectFilePath());
    QVERIFY(qbsFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    qbsFile.write("import qbs.base 1.0\nCppApplication {\n    Depends { name: 'Qt.core' }\n"
                  "    files: ['main.cpp', 'myobject.cpp']\n}");
    qbsFile.close();
    ProcessResultReceiver receiver;
    qbs::ErrorInfo errorInfo
            = doBuildProject("missing-qobject-header/missingheader.qbs", 0, &receiver);
    QVERIFY(errorInfo.hasError());
    QVERIFY2(isAboutUndefinedSymbols(receiver.output), qPrintable(receiver.output));

    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(qbsFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    qbsFile.write("import qbs.base 1.0\nCppApplication {\n    Depends { name: 'Qt.core' }\n"
                  "    files: ['main.cpp', 'myobject.cpp','myobject.h']\n}");
    qbsFile.close();
    errorInfo = doBuildProject("missing-qobject-header/missingheader.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::trackRemoveQObjectHeader()
{
    const qbs::SetupProjectParameters params
            = defaultSetupParameters("missing-qobject-header/missingheader.qbs");
    removeBuildDir(params);
    QFile qbsFile(params.projectFilePath());
    QVERIFY(qbsFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    qbsFile.write("import qbs.base 1.0\nCppApplication {\n    Depends { name: 'Qt.core' }\n"
                  "    files: ['main.cpp', 'myobject.cpp','myobject.h']\n}");
    qbsFile.close();
    qbs::ErrorInfo errorInfo = doBuildProject("missing-qobject-header/missingheader.qbs");
    VERIFY_NO_ERROR(errorInfo);

    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(qbsFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    qbsFile.write("import qbs.base 1.0\nCppApplication {\n    Depends { name: 'Qt.core' }\n"
                  "    files: ['main.cpp', 'myobject.cpp']\n}");
    qbsFile.close();
    ProcessResultReceiver receiver;
    errorInfo = doBuildProject("missing-qobject-header/missingheader.qbs", 0, &receiver);
    QVERIFY(errorInfo.hasError());
    QVERIFY2(isAboutUndefinedSymbols(receiver.output), qPrintable(receiver.output));
}

void TestApi::transformers()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("transformers/transformers.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::typeChange()
{
    BuildDescriptionReceiver receiver;
    qbs::ErrorInfo errorInfo = doBuildProject("type-change/project.qbs", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY2(!receiver.descriptions.contains("compiling"), qPrintable(receiver.descriptions));

    WAIT_FOR_NEW_TIMESTAMP();
    QFile projectFile("project.qbs");
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    QByteArray content = projectFile.readAll();
    content.replace("//", "");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    errorInfo = doBuildProject("type-change/project.qbs", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY2(receiver.descriptions.contains("compiling"), qPrintable(receiver.descriptions));
}

void TestApi::uic()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("uic/uic.qbs");
    VERIFY_NO_ERROR(errorInfo);
}


qbs::ErrorInfo TestApi::doBuildProject(const QString &projectFilePath,
        QObject *buildDescriptionReceiver, QObject *procResultReceiver, QObject *taskReceiver,
        const qbs::BuildOptions &options, const QVariantMap overriddenValues)
{
    qbs::SetupProjectParameters params = defaultSetupParameters(projectFilePath);
    params.setOverriddenValues(overriddenValues);
    params.setDryRun(options.dryRun());
    const QScopedPointer<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(params,
                                                                                    m_logSink, 0));
    if (taskReceiver) {
        connect(setupJob.data(), SIGNAL(taskStarted(QString,int,qbs::AbstractJob*)), taskReceiver,
                SLOT(handleTaskStart(QString)));
    }
    waitForFinished(setupJob.data());
    if (setupJob->error().hasError())
        return setupJob->error();
    const QScopedPointer<qbs::BuildJob> buildJob(setupJob->project().buildAllProducts(options));
    if (buildDescriptionReceiver) {
        connect(buildJob.data(), SIGNAL(reportCommandDescription(QString,QString)),
                buildDescriptionReceiver, SLOT(handleDescription(QString,QString)));
    }
    if (procResultReceiver) {
        connect(buildJob.data(), SIGNAL(reportProcessResult(qbs::ProcessResult)),
                procResultReceiver, SLOT(handleProcessResult(qbs::ProcessResult)));
    }
    waitForFinished(buildJob.data());
    return buildJob->error();
}

QTEST_MAIN(TestApi)

#include "tst_api.moc"
