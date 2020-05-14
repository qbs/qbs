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

#include "tst_api.h"

#include "../shared.h"

#include <api/runenvironment.h>
#include <qbs.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/qttools.h>
#include <tools/set.h>
#include <tools/toolchains.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qeventloop.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qthread.h>
#include <QtCore/qtimer.h>

#include <QtTest/qtest.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <regex>
#include <utility>
#include <vector>

#define VERIFY_NO_ERROR(errorInfo) \
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()))

#define WAIT_FOR_NEW_TIMESTAMP() waitForNewTimestamp(m_workingDataDir)

class LogSink: public qbs::ILogSink
{
public:
    QString output;

    void doPrintWarning(const qbs::ErrorInfo &error) override {
        qDebug("%s", qPrintable(error.toString()));
        warnings.push_back(error);
    }
    void doPrintMessage(qbs::LoggerLevel, const QString &message, const QString &) override {
        output += message;
    }

    QList<qbs::ErrorInfo> warnings;
};

class BuildDescriptionReceiver : public QObject
{
    Q_OBJECT
public:
    QString descriptions;
    QStringList descriptionLines;

    void handleDescription(const QString &, const QString &description) {
        descriptions += description;
        descriptionLines << description;
    }
};

class ProcessResultReceiver : public QObject
{
    Q_OBJECT
public:
    QString output;
    std::vector<qbs::ProcessResult> results;

    void handleProcessResult(const qbs::ProcessResult &result) {
        results.push_back(result);
        output += result.stdErr().join(QLatin1Char('\n'));
        output += result.stdOut().join(QLatin1Char('\n'));
    }
};

class TaskReceiver : public QObject
{
    Q_OBJECT
public:
    QString taskDescriptions;

    void handleTaskStart(const QString &task) { taskDescriptions += task; }
};


static void removeBuildDir(const qbs::SetupProjectParameters &params)
{
    QString message;
    const QString dir = params.buildRoot() + '/' + params.configurationName();
    if (!qbs::Internal::removeDirectoryWithContents(dir, &message))
        qFatal("Could not remove build dir: %s", qPrintable(message));
}

static bool waitForFinished(qbs::AbstractJob *job, int timeout = 0)
{
    if (job->state() == qbs::AbstractJob::StateFinished)
        return true;
    QEventLoop loop;
    QObject::connect(job, &qbs::AbstractJob::finished, &loop, &QEventLoop::quit);
    if (timeout > 0) {
        QTimer timer;
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
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
    , m_sourceDataDir(testDataSourceDir(SRCDIR "/testdata"))
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
                                              m_workingDataDir, false, true, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(copyDllExportHeader(m_sourceDataDir, m_workingDataDir));
}

void TestApi::init()
{
    m_logSink->warnings.clear();
    m_logSink->setLogLevel(qbs::LoggerInfo);
}

void TestApi::addQObjectMacroToCppFile()
{
    BuildDescriptionReceiver receiver;
    qbs::ErrorInfo errorInfo = doBuildProject("add-qobject-macro-to-cpp-file", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY2(!receiver.descriptions.contains("moc"), qPrintable(receiver.descriptions));
    receiver.descriptions.clear();

    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("object.cpp", "// ", "");
    errorInfo = doBuildProject("add-qobject-macro-to-cpp-file", &receiver);
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
    const QString relProjectFilePath = "added-file-persistent";
    ProcessResultReceiver receiver;
    qbs::ErrorInfo errorInfo = doBuildProject(relProjectFilePath, 0, &receiver);
    QVERIFY(errorInfo.hasError());
    QVERIFY2(isAboutUndefinedSymbols(receiver.output), qPrintable((receiver.output)));
    receiver.output.clear();

    // Add a file. qbs must schedule it for rule application on the next build.
    WAIT_FOR_NEW_TIMESTAMP();
    const qbs::SetupProjectParameters params = defaultSetupParameters(relProjectFilePath);
    REPLACE_IN_FILE(params.projectFilePath(), "/* 'file.cpp' */", "'file.cpp'");
    std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(params, m_logSink,
                                                                              0));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    setupJob.reset(nullptr);

    // Remove the file again. qbs must unschedule the rule application again.
    // Consequently, the linking step must fail as in the initial run.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(params.projectFilePath(), "'file.cpp'", "/* 'file.cpp' */");
    errorInfo = doBuildProject(relProjectFilePath, 0, &receiver);
    QVERIFY(errorInfo.hasError());
    QVERIFY2(isAboutUndefinedSymbols(receiver.output), qPrintable((receiver.output)));

    // Add the file again. qbs must schedule it for rule application on the next build.
    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE(params.projectFilePath(), "/* 'file.cpp' */", "'file.cpp'");
    setupJob.reset(qbs::Project().setupProject(params, m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    setupJob.reset(nullptr);

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

void TestApi::buildGraphInfo()
{
    SettingsPtr s = settings();
    qbs::Internal::TemporaryProfile p("bgInfoProfile", s.get());
    p.p.setValue("qbs.targetPlatform", "xenix");
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("buildgraph-info");
    setupParams.setTopLevelProfile(p.p.name());
    setupParams.setOverriddenValues({std::make_pair("qbs.architecture", "arm")});
    std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    const QString bgFilePath = setupParams.buildRoot() + QLatin1Char('/')
            + relativeBuildGraphFilePath();
    QVERIFY2(QFileInfo::exists(bgFilePath), qPrintable(bgFilePath));
    qbs::Project::BuildGraphInfo bgInfo
            = qbs::Project::getBuildGraphInfo(bgFilePath, QStringList());
    QVERIFY(bgInfo.error.hasError()); // Build graph is still locked.
    setupJob.reset(nullptr);
    const QStringList requestedProperties({"qbs.architecture", "qbs.shellPath",
                                           "qbs.targetPlatform"});
    bgInfo = qbs::Project::getBuildGraphInfo(bgFilePath, requestedProperties);
    QVERIFY2(!bgInfo.error.hasError(), qPrintable(bgInfo.error.toString()));
    QCOMPARE(bgFilePath, bgInfo.bgFilePath);
    QCOMPARE(bgInfo.profileData.size(), 1);
    QCOMPARE(bgInfo.profileData.value(p.p.name()).toMap().size(), 1);
    QCOMPARE(bgInfo.profileData.value(p.p.name()).toMap().value("qbs").toMap().value(
                 "targetPlatform"),
             p.p.value("qbs.targetPlatform"));
    QCOMPARE(bgInfo.overriddenProperties, setupParams.overriddenValues());
    QCOMPARE(bgInfo.requestedProperties.size(), requestedProperties.size());
    QCOMPARE(bgInfo.requestedProperties.value("qbs.architecture").toString(), QString("arm"));
    QCOMPARE(bgInfo.requestedProperties.value("qbs.shellPath").toString(), QString("/bin/bash"));
    QCOMPARE(bgInfo.requestedProperties.value("qbs.targetPlatform").toString(), QString("xenix"));
}

void TestApi::buildErrorCodeLocation()
{
    const qbs::ErrorInfo errorInfo
            = doBuildProject("build-error-code-location/build-error-code-location.qbs");
    QVERIFY(errorInfo.hasError());
    const qbs::ErrorItem errorItem = errorInfo.items().front();
    QCOMPARE(errorItem.description(),
             QString("Rule.outputArtifacts must return an array of objects."));
    const qbs::CodeLocation errorLoc = errorItem.codeLocation();
    QCOMPARE(QFileInfo(errorLoc.filePath()).fileName(), QString("build-error-code-location.qbs"));
    QCOMPARE(errorLoc.line(), 7);
    QCOMPARE(errorLoc.column(), 26);
}

void TestApi::buildGraphLocking()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("buildgraph-locking");
    std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    const qbs::Project project = setupJob->project();
    Q_UNUSED(project);

    // Case 1: Setting up a competing project from scratch.
    setupJob.reset(qbs::Project().setupProject(setupParams, m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY(setupJob->error().hasError());
    QVERIFY2(setupJob->error().toString().contains("lock"),
             qPrintable(setupJob->error().toString()));

    // Case 2: Setting up a non-competing project and then making it competing.
    qbs::SetupProjectParameters setupParams2 = setupParams;
    setupParams2.setBuildRoot(setupParams.buildRoot() + "/2");
    setupJob.reset(qbs::Project().setupProject(setupParams2, m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    const QString buildDirName = relativeBuildDir(setupParams2.configurationName());
    const QString lockFile = setupParams2.buildRoot() + '/' + buildDirName + '/' + buildDirName
            + ".bg.lock";
    QVERIFY2(QFileInfo(lockFile).isFile(), qPrintable(lockFile));
    qbs::Project project2 = setupJob->project();
    QVERIFY(project2.isValid());
    setupJob.reset(project2.setupProject(setupParams, m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY(setupJob->error().hasError());
    QVERIFY2(setupJob->error().toString().contains("lock"),
             qPrintable(setupJob->error().toString()));
    QVERIFY2(QFileInfo(lockFile).isFile(), qPrintable(lockFile));

    // Case 3: Changing the build directory of an existing project to something non-competing.
    qbs::SetupProjectParameters setupParams3 = setupParams2;
    setupParams3.setBuildRoot(setupParams.buildRoot() + "/3");
    setupJob.reset(qbs::Project().setupProject(setupParams3, m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    project2 = qbs::Project();
    QVERIFY2(!QFileInfo(lockFile).exists(), qPrintable(lockFile));
    const QString newLockFile = setupParams3.buildRoot() + '/' + buildDirName + '/'
            + buildDirName + ".bg.lock";
    QVERIFY2(QFileInfo(newLockFile).isFile(), qPrintable(newLockFile));
    qbs::Project project3 = setupJob->project();
    QVERIFY(project3.isValid());

    // Case 4: Changing the build directory again, but cancelling the job.
    setupJob.reset(project3.setupProject(setupParams2, m_logSink, 0));
    QThread::sleep(1);
    setupJob->cancel();
    waitForFinished(setupJob.get());
    QVERIFY(setupJob->error().hasError());
    QVERIFY2(!QFileInfo(lockFile).exists(), qPrintable(lockFile));
    QVERIFY2(QFileInfo(newLockFile).isFile(), qPrintable(newLockFile));
    setupJob.reset(nullptr);
    project3 = qbs::Project();
    QVERIFY2(!QFileInfo(newLockFile).exists(), qPrintable(newLockFile));
}

void TestApi::buildProject()
{
    QFETCH(QString, projectSubDir);
    QFETCH(QString, productFileName);
    const QString projectFilePath = projectSubDir + QLatin1Char('/') + projectSubDir
            + QLatin1String(".qbs");
    qbs::SetupProjectParameters params = defaultSetupParameters(projectFilePath);
    removeBuildDir(params);
    qbs::ErrorInfo errorInfo = doBuildProject(projectFilePath);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(regularFileExists(relativeBuildGraphFilePath()));
    if (!productFileName.isEmpty()) {
        QVERIFY2(regularFileExists(productFileName), qPrintable(productFileName));
        QVERIFY2(QFile::remove(productFileName), qPrintable(productFileName));
    }

    WAIT_FOR_NEW_TIMESTAMP();
    qbs::BuildOptions options;
    options.setForceTimestampCheck(true);
    errorInfo = doBuildProject(projectFilePath, 0, 0, 0, options);
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
    QTest::newRow("link staticlib dynamiclib")
            << QString("link-staticlib-dynamiclib")
            << relativeExecutableFilePath("app");
    QTest::newRow("precompiled header new")
            << QString("precompiled-header-new")
            << relativeExecutableFilePath("MyApp");
    QTest::newRow("precompiled header dynamic")
            << QString("precompiled-header-dynamic")
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
    const QString projectFilePath = projectSubDir + QLatin1Char('/') + projectSubDir
            + QLatin1String(".qbs");
    qbs::SetupProjectParameters params = defaultSetupParameters(projectFilePath);
    removeBuildDir(params);
    qbs::BuildOptions options;
    options.setDryRun(true);
    const qbs::ErrorInfo errorInfo = doBuildProject(projectFilePath, 0, 0, 0, options);
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
            = defaultSetupParameters("build-single-file");
    std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                              m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    qbs::BuildOptions options;
    options.setFilesToConsider(QStringList(setupParams.buildRoot() + "/compiled.cpp"));
    options.setActiveFileTags(QStringList("obj"));
    m_logSink->setLogLevel(qbs::LoggerMaxLevel);
    std::unique_ptr<qbs::BuildJob> buildJob(project.buildAllProducts(options));
    BuildDescriptionReceiver receiver;
    connect(buildJob.get(), &qbs::BuildJob::reportCommandDescription, &receiver,
            &BuildDescriptionReceiver::handleDescription);
    waitForFinished(buildJob.get());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    QCOMPARE(receiver.descriptions.count("compiling"), 2);
    QCOMPARE(receiver.descriptions.count("precompiling"), 1);
    QVERIFY2(receiver.descriptions.contains("generating generated.h"),
             qPrintable(receiver.descriptions));
    QVERIFY2(receiver.descriptions.contains("compiling compiled.cpp"),
             qPrintable(receiver.descriptions));
}

void TestApi::canonicalToolchainList()
{
    // All the known toolchain lists should be equal
    QCOMPARE(qbs::canonicalToolchain(QStringList({"xcode", "clang", "llvm", "gcc"})),
             QStringList({"xcode", "clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"clang", "llvm", "gcc"})),
             QStringList({"clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"clang-cl", "msvc"})),
             QStringList({"clang-cl", "msvc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"llvm", "gcc"})),
             QStringList({"llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"mingw", "gcc"})),
             QStringList({"mingw", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"gcc"})),
             QStringList({"gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"msvc"})),
             QStringList({"msvc"}));

    // Single names should canonicalize to the known lists
    QCOMPARE(qbs::canonicalToolchain(QStringList({"xcode"})),
             QStringList({"xcode", "clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"clang"})),
             QStringList({"clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"clang-cl"})),
             QStringList({"clang-cl", "msvc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"llvm"})),
             QStringList({"llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"mingw"})),
             QStringList({"mingw", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"gcc"})),
             QStringList({"gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"msvc"})),
             QStringList({"msvc"}));

    // Missing some in the middle
    QCOMPARE(qbs::canonicalToolchain(QStringList({"xcode", "llvm", "gcc"})),
             QStringList({"xcode", "clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"xcode", "clang", "gcc"})),
             QStringList({"xcode", "clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"xcode", "gcc"})),
             QStringList({"xcode", "clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"clang", "llvm"})),
             QStringList({"clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"clang", "gcc"})),
             QStringList({"clang", "llvm", "gcc"}));

    // Sorted wrong, missing some in the middle
    QCOMPARE(qbs::canonicalToolchain(QStringList({"gcc", "llvm", "clang", "xcode"})),
             QStringList({"xcode", "clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"clang", "gcc", "llvm", "xcode"})),
             QStringList({"xcode", "clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"llvm", "clang", "xcode", "gcc"})),
             QStringList({"xcode", "clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"gcc", "llvm", "clang"})),
             QStringList({"clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"gcc", "clang", "xcode"})),
             QStringList({"xcode", "clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"gcc", "llvm"})),
             QStringList({"llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"gcc", "mingw"})),
             QStringList({"mingw", "gcc"}));

    // Duplicates
    QCOMPARE(qbs::canonicalToolchain(QStringList({"gcc", "llvm", "clang", "xcode", "xcode",
                                                  "xcode"})),
             QStringList({"xcode", "clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"clang", "gcc", "llvm", "clang", "xcode"})),
             QStringList({"xcode", "clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"llvm", "clang", "clang", "xcode", "xcode",
                                                  "gcc"})),
             QStringList({"xcode", "clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"llvm", "clang", "gcc", "llvm", "clang"})),
             QStringList({"clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"xcode", "gcc", "clang", "gcc", "clang",
                                                  "xcode"})),
             QStringList({"xcode", "clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"llvm", "gcc", "llvm", "llvm"})),
             QStringList({"llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(QStringList({"gcc", "gcc", "gcc", "mingw"})),
             QStringList({"mingw", "gcc"}));

    // Custom insanity
    QCOMPARE(qbs::canonicalToolchain(
                 QStringList({"crazy", "gcc", "llvm", "clang", "xcode", "insane"})),
             QStringList({"crazy", "insane", "xcode", "clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(
                 QStringList({"crazy", "gcc", "llvm", "clang", "xcode", "insane", "crazy"})),
             QStringList({"crazy", "insane", "xcode", "clang", "llvm", "gcc"}));
    QCOMPARE(qbs::canonicalToolchain(
                 QStringList({"crazy", "insane", "gcc", "trade", "llvm", "clang", "xcode",
                              "insane", "mark", "crazy"})),
             QStringList({"crazy", "insane", "trade", "mark", "xcode", "clang", "llvm", "gcc"}));
}

void TestApi::checkOutputs()
{
    QFETCH(bool, check);
    qbs::SetupProjectParameters params = defaultSetupParameters("/check-outputs");
    qbs::BuildOptions options;
    options.setForceOutputCheck(check);
    removeBuildDir(params);
    qbs::ErrorInfo errorInfo = doBuildProject("/check-outputs", 0, 0, 0, options);
    if (check)
        QVERIFY(errorInfo.hasError());
    else
        VERIFY_NO_ERROR(errorInfo);
}

void TestApi::checkOutputs_data()
{
    QTest::addColumn<bool>("check");
    QTest::newRow("checked outputs") << true;
    QTest::newRow("unchecked outputs") << false;
}

qbs::GroupData findGroup(const qbs::ProductData &product, const QString &name)
{
    const auto groups = product.groups();
    for (const qbs::GroupData &g : groups) {
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

void TestApi::changeContent()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("project-editing");
    std::unique_ptr<qbs::SetupProjectJob> job;
    qbs::Project project;
    qbs::ProjectData projectData;
    qbs::ProductData product;

    const auto resolve = [&] {
        job.reset(project.setupProject(setupParams, m_logSink, 0));
        waitForFinished(job.get());
        QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
        project = job->project();
        projectData = project.projectData();
        QCOMPARE(projectData.allProducts().size(), 1);
        product = projectData.allProducts().front();
    };
    resolve();
    QVERIFY(product.groups().size() >= 8);

    // Error handling: Invalid product.
    qbs::ErrorInfo errorInfo = project.addGroup(qbs::ProductData(), "blubb");
    QVERIFY(errorInfo.hasError());
    QVERIFY(errorInfo.toString().contains("invalid"));

    // Error handling: Empty group name.
    errorInfo = project.addGroup(product, QString());
    QVERIFY(errorInfo.hasError());
    QVERIFY(errorInfo.toString().contains("empty"));

    WAIT_FOR_NEW_TIMESTAMP();
    errorInfo = project.addGroup(product, "New Group 1");
    VERIFY_NO_ERROR(errorInfo);

    errorInfo = project.addGroup(product, "New Group 2");
    VERIFY_NO_ERROR(errorInfo);

    resolve();
    QVERIFY(product.groups().size() >= 10);

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
    WAIT_FOR_NEW_TIMESTAMP();
    qbs::GroupData group = findGroup(product, "New Group 1");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "file.h" << "file.cpp");
    VERIFY_NO_ERROR(errorInfo);

    // Error handling: Add the same file again.
    resolve();
    group = findGroup(product, "New Group 1");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "file.cpp");
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("already"), qPrintable(errorInfo.toString()));

    // Remove one of the newly added files again.
    WAIT_FOR_NEW_TIMESTAMP();
    errorInfo = project.removeFiles(product, group, QStringList("file.h"));
    VERIFY_NO_ERROR(errorInfo);

    // Error handling: Try to remove the same file again.
    resolve();
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
    WAIT_FOR_NEW_TIMESTAMP();
    errorInfo = project.removeFiles(product, qbs::GroupData(), QStringList("main.cpp"));
    VERIFY_NO_ERROR(errorInfo);
    resolve();

    // Add file to non-empty array literal.
    WAIT_FOR_NEW_TIMESTAMP();
    group = findGroup(product, "Existing Group 1");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "newfile1.txt");
    VERIFY_NO_ERROR(errorInfo);
    resolve();

    // Add files to list represented as a single string.
    WAIT_FOR_NEW_TIMESTAMP();
    errorInfo = project.addFiles(product, qbs::GroupData(), QStringList() << "newfile2.txt");
    VERIFY_NO_ERROR(errorInfo);
    resolve();

    // Add files to list represented as an identifier.
    WAIT_FOR_NEW_TIMESTAMP();
    group = findGroup(product, "Existing Group 2");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "newfile3.txt");
    VERIFY_NO_ERROR(errorInfo);
    resolve();

    // Add files to list represented as a block of code (not yet implemented).
    group = findGroup(product, "Existing Group 3");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "newfile4.txt");
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("complex"), qPrintable(errorInfo.toString()));

    // Add file to group with directory prefix.
    WAIT_FOR_NEW_TIMESTAMP();
    group = findGroup(product, "Existing Group 4");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "file.txt");
    VERIFY_NO_ERROR(errorInfo);
    resolve();

    // Error handling: Add file to group with non-directory prefix.
    group = findGroup(product, "Existing Group 5");
    QVERIFY(group.isValid());
    errorInfo = project.addFiles(product, group, QStringList() << "newfile1.txt");
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("prefix"), qPrintable(errorInfo.toString()));

    // Remove group.
    WAIT_FOR_NEW_TIMESTAMP();
    group = findGroup(product, "Existing Group 5");
    QVERIFY(group.isValid());
    errorInfo = project.removeGroup(product, group);
    VERIFY_NO_ERROR(errorInfo);
    resolve();

    // Error handling: Try to remove the same group again.
    errorInfo = project.removeGroup(product, group);
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("does not exist"), qPrintable(errorInfo.toString()));

    // Add a file to a group where the file name is already matched by a wildcard.
    projectData = project.projectData();
    QVERIFY(projectData.products().size() == 1);
    product = projectData.products().front();
    group = findGroup(product, "Group with wildcards");
    QVERIFY(group.isValid());
    QFile newFile("koerper.klaus");
    QVERIFY2(newFile.open(QIODevice::WriteOnly), qPrintable(newFile.errorString()));
    newFile.close();
    errorInfo = project.addFiles(product, group, QStringList() << newFile.fileName());
    VERIFY_NO_ERROR(errorInfo);
    resolve();
    group = findGroup(product, "Group with wildcards");
    QVERIFY(group.isValid());
    QCOMPARE(group.sourceArtifactsFromWildcards().size(), 1);
    QCOMPARE(group.sourceArtifactsFromWildcards().front().filePath(),
             QFileInfo(newFile).absoluteFilePath());

    // Error checking: Try to remove a file that originates from a wildcard pattern.
    projectData = project.projectData();
    QVERIFY(projectData.products().size() == 1);
    product = projectData.products().front();
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
    std::unique_ptr<qbs::BuildJob> buildJob(project.buildAllProducts(buildOptions, defaultProducts(),
                                                                    this));
    connect(buildJob.get(), &qbs::BuildJob::reportCommandDescription,
            &rcvr, &BuildDescriptionReceiver::handleDescription);
    waitForFinished(buildJob.get());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    QVERIFY(rcvr.descriptions.contains("compiling file.cpp"));
    QVERIFY(!rcvr.descriptions.contains("compiling main.cpp"));

    // Error handling: Try to change the project during a build.
    buildJob.reset(project.buildAllProducts(buildOptions, defaultProducts(), this));
    errorInfo = project.addGroup(projectData.products().front(), "blubb");
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("in progress"), qPrintable(errorInfo.toString()));
    waitForFinished(buildJob.get());
    errorInfo = project.addGroup(projectData.products().front(), "blubb");
    VERIFY_NO_ERROR(errorInfo);

    project = qbs::Project();
    job.reset(nullptr);
    buildJob.reset(nullptr);
    removeBuildDir(setupParams);
    // Add a file to the top level of a product that does not have a "files" binding yet.
    setupParams.setProjectFilePath(QDir::cleanPath(m_workingDataDir +
        "/project-editing/project-with-no-files.qbs"));

    resolve();
    WAIT_FOR_NEW_TIMESTAMP();
    errorInfo = project.addFiles(product, qbs::GroupData(), QStringList("main.cpp"));
    VERIFY_NO_ERROR(errorInfo);
    resolve();
    rcvr.descriptions.clear();
    buildJob.reset(project.buildAllProducts(buildOptions, defaultProducts(), this));
    connect(buildJob.get(), &qbs::BuildJob::reportCommandDescription,
            &rcvr, &BuildDescriptionReceiver::handleDescription);
    waitForFinished(buildJob.get());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    QVERIFY(rcvr.descriptions.contains("compiling main.cpp"));
    job.reset(project.setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
}

#endif // QBS_ENABLE_PROJECT_FILE_UPDATES

void TestApi::commandExtraction()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("/command-extraction");
    std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                              m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    qbs::ProjectData projectData = project.projectData();
    QCOMPARE(projectData.allProducts().size(), 1);
    qbs::ProductData productData = projectData.allProducts().front();
    qbs::ErrorInfo errorInfo;
    const QString projectDirPath = QDir::cleanPath(QFileInfo(setupParams.projectFilePath()).path());
    const QString sourceFilePath = projectDirPath + "/main.cpp";

    // Before the first build, no rules exist.
    qbs::RuleCommandList commands
            = project.ruleCommands(productData, sourceFilePath, "obj", &errorInfo);
    QCOMPARE(commands.size(), 0);
    QVERIFY(errorInfo.hasError());
    QVERIFY2(errorInfo.toString().contains("No rule"), qPrintable(errorInfo.toString()));

    qbs::BuildOptions options;
    options.setDryRun(true);
    std::unique_ptr<qbs::BuildJob> buildJob(project.buildAllProducts(options));
    waitForFinished(buildJob.get());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    projectData = project.projectData();
    QCOMPARE(projectData.allProducts().size(), 1);
    productData = projectData.allProducts().front();
    errorInfo = qbs::ErrorInfo();

    // After the build, the compile command must be found.
    commands = project.ruleCommands(productData, sourceFilePath, "obj", &errorInfo);
    QCOMPARE(commands.size(), 1);
    QVERIFY2(!errorInfo.hasError(), qPrintable(errorInfo.toString()));
    const qbs::RuleCommand command = commands.front();
    QCOMPARE(command.type(), qbs::RuleCommand::ProcessCommandType);
    QVERIFY(!command.executable().isEmpty());
    QVERIFY(!command.arguments().empty());
}

void TestApi::dependencyOnMultiplexedType()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("/dependency-on-multiplexed-type");
    std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                              m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    qbs::ProjectData projectData = project.projectData();
    const QList<qbs::ProductData> allProducts = projectData.allProducts();
    QCOMPARE(allProducts.size(), 5);
    int depCount = 0;
    int p1Count = 0;
    int p2Count = 0;
    for (const qbs::ProductData &p : allProducts) {
        if (p.name() == "dep") {
            ++depCount;
            QCOMPARE(p.dependencies().size(), 0);
        } else if (p.name() == "p1") {
            ++p1Count;
            if (p.multiplexConfigurationId().isEmpty()) // aggregate
                QCOMPARE(p.dependencies().size(), 3);
            else
                QCOMPARE(p.dependencies().size(), 1);
        } else {
            QVERIFY(p.name() == "p2");
            ++p2Count;

            // FIXME: This is an odd effect of our current algorithm: We collect the products
            // matching the requested type and add Depends items with their names ("p1" in
            // this case). Later, the algorithm checking for compatibility regarding the
            // multiplexing axes picks the aggregate. However, the aggregate does not have
            // a matching type... It's not entirely clear what the real expected
            // result should be here.
            QCOMPARE(p.dependencies().size(), 2);
        }
    }
    std::unique_ptr<qbs::BuildJob> buildJob(project.buildAllProducts(qbs::BuildOptions()));
    waitForFinished(buildJob.get());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
}

void TestApi::changeDependentLib()
{
    qbs::ErrorInfo errorInfo = doBuildProject("change-dependent-lib");
    VERIFY_NO_ERROR(errorInfo);

    WAIT_FOR_NEW_TIMESTAMP();
    REPLACE_IN_FILE("change-dependent-lib.qbs", "cpp.defines: [\"XXXX\"]",
                    "cpp.defines: [\"ABCD\"]");
    errorInfo = doBuildProject("change-dependent-lib");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::enableAndDisableProduct()
{
    BuildDescriptionReceiver bdr;
    qbs::ErrorInfo errorInfo = doBuildProject("enable-and-disable-product", &bdr);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(!bdr.descriptions.contains("compiling"));

    WAIT_FOR_NEW_TIMESTAMP();
    QFile projectFile("enable-and-disable-product.qbs");
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    QByteArray content = projectFile.readAll();
    content.replace("undefined", "'hidden'");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    bdr.descriptions.clear();
    errorInfo = doBuildProject("enable-and-disable-product", &bdr);
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
    errorInfo = doBuildProject("enable-and-disable-product", &bdr);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(!bdr.descriptions.contains("compiling"));
}

void TestApi::errorInSetupRunEnvironment()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("error-in-setup-run-environment");
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    const qbs::Project project = job->project();
    QVERIFY(project.isValid());
    QCOMPARE(project.projectData().products().size(), 1);
    const qbs::ProductData product = project.projectData().products().front();

    bool exceptionCaught = false;
    try {
        const SettingsPtr s = settings();
        qbs::RunEnvironment runEnv = project.getRunEnvironment(product, qbs::InstallOptions(),
                QProcessEnvironment(), QStringList(), s.get());
        qbs::ErrorInfo error;
        const QProcessEnvironment env = runEnv.runEnvironment(&error);
        QVERIFY(error.hasError());
        QVERIFY(error.toString().contains("trallala"));
    } catch (const qbs::ErrorInfo &) {
        exceptionCaught = true;
    }
    QVERIFY(!exceptionCaught);
}

void TestApi::excludedInputs()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("excluded-inputs");
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    const qbs::Project project = job->project();
    std::unique_ptr<qbs::BuildJob> buildJob(project.buildAllProducts(qbs::BuildOptions()));
    waitForFinished(buildJob.get());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(job->error().toString()));
    QVERIFY(project.isValid());
    QCOMPARE(project.projectData().products().size(), 2);
    qbs::ProductData depProduct;
    qbs::ProductData pProduct;
    for (const qbs::ProductData &p : project.projectData().products()) {
        if (p.name() == "dep")
            depProduct = p;
        else if (p.name() == "p")
            pProduct = p;
    }
    QVERIFY(depProduct.isValid());
    QVERIFY(pProduct.isValid());
    int theTagCount = 0;
    for (const qbs::ArtifactData &artifact : depProduct.targetArtifacts()) {
        if (!artifact.fileTags().contains("the_tag"))
            continue;
        ++theTagCount;
        QFile f(artifact.filePath());
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.errorString()));
        const QByteArray content = f.readAll();
        QVERIFY2(content.contains("the_content"), content.constData());
        QCOMPARE(artifact.fileTags().contains("the_other_tag"),
                 content.contains("the_other_content"));
    }
    QCOMPARE(theTagCount, 2);
    int dummyCount = 0;
    for (const qbs::ArtifactData &artifact : pProduct.targetArtifacts()) {
        QFileInfo fi(artifact.filePath());
        QVERIFY2(fi.exists(), qPrintable(fi.filePath()));
        if (fi.fileName().startsWith("dummy"))
            ++dummyCount;
    }
    QCOMPARE(dummyCount, 3);
}

static qbs::ErrorInfo forceRuleEvaluation(const qbs::Project &project)
{
    qbs::BuildOptions buildOptions;
    buildOptions.setDryRun(true);
    std::unique_ptr<qbs::BuildJob> buildJob(project.buildAllProducts(buildOptions));
    waitForFinished(buildJob.get());
    return buildJob->error();
}

void TestApi::disabledInstallGroup()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("disabled_install_group");
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    const qbs::Project project = job->project();

    const qbs::ErrorInfo errorInfo = forceRuleEvaluation(project);
    VERIFY_NO_ERROR(errorInfo);

    qbs::ProjectData projectData = project.projectData();
    QCOMPARE(projectData.allProducts().size(), 1);
    qbs::ProductData product = projectData.allProducts().front();
    const QList<qbs::ArtifactData> targets = product.targetArtifacts();
    QCOMPARE(targets.size(), 1);
    QVERIFY(targets.front().isGenerated());
    QVERIFY(targets.front().isExecutable());
    QVERIFY(targets.front().isTargetArtifact());
    QCOMPARE(projectData.installableArtifacts().size(), 0);
    QCOMPARE(product.targetExecutable(), targets.front().filePath());
}

void TestApi::disabledProduct()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("disabled-product");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::disabledProject()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("disabled-project");
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

void TestApi::emptyFileTagList()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("empty-filetag-list");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::emptySubmodulesList()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("empty-submodules-list");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::explicitlyDependsOn()
{
    BuildDescriptionReceiver receiver;
    qbs::ErrorInfo errorInfo = doBuildProject("explicitly-depends-on", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    if (m_logSink->output.contains("targetPlatform differs from hostPlatform"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QVERIFY2(receiver.descriptions.contains("compiling compiler.cpp"),
             qPrintable(receiver.descriptions));
    QVERIFY2(receiver.descriptions.contains("compiling a.in"), qPrintable(receiver.descriptions));
    QVERIFY2(receiver.descriptions.contains("compiling b.in"), qPrintable(receiver.descriptions));
    QVERIFY2(receiver.descriptions.contains("compiling c.in"), qPrintable(receiver.descriptions));
    QFile txtFile(relativeProductBuildDir("p") + "/compiler-name.txt");
    QVERIFY2(txtFile.open(QIODevice::ReadOnly), qPrintable(txtFile.errorString()));
    const QByteArray content = txtFile.readAll();
    QCOMPARE(content, QByteArray("compiler file name: compiler"));
    receiver.descriptions.clear();

    errorInfo = doBuildProject("explicitly-depends-on", &receiver);
    QVERIFY2(!receiver.descriptions.contains("compiling compiler.cpp"),
             qPrintable(receiver.descriptions));
    QVERIFY2(!receiver.descriptions.contains("compiling a.in"), qPrintable(receiver.descriptions));
    QVERIFY2(!receiver.descriptions.contains("compiling b.in"), qPrintable(receiver.descriptions));
    QVERIFY2(!receiver.descriptions.contains("compiling c.in"), qPrintable(receiver.descriptions));
    VERIFY_NO_ERROR(errorInfo);

    WAIT_FOR_NEW_TIMESTAMP();
    touch("compiler.cpp");
    waitForFileUnlock();
    errorInfo = doBuildProject("explicitly-depends-on", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY2(receiver.descriptions.contains("compiling compiler.cpp"),
             qPrintable(receiver.descriptions));
    QVERIFY2(receiver.descriptions.contains("compiling a.in"), qPrintable(receiver.descriptions));
    QVERIFY2(receiver.descriptions.contains("compiling b.in"), qPrintable(receiver.descriptions));
    QVERIFY2(receiver.descriptions.contains("compiling c.in"), qPrintable(receiver.descriptions));
}

void TestApi::exportSimple()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("export-simple");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::exportWithRecursiveDepends()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("export-with-recursive-depends");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::fallbackGcc()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("fallback-gcc/fallback-gcc.qbs");
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                          m_logSink, nullptr));
    waitForFinished(job.get());
    VERIFY_NO_ERROR(job->error());

    qbs::ProjectData project = job->project().projectData();
    QVERIFY(project.isValid());
    QList<qbs::ProductData> products = project.allProducts();
    QCOMPARE(products.size(), 2);
    for (const qbs::ProductData &p : qAsConst(products)) {
        if (p.profile() == "unixProfile") {
            qbs::PropertyMap moduleProps = p.moduleProperties();
            QCOMPARE(moduleProps.getModuleProperty("qbs", "targetOS").toStringList(),
                     QStringList({"unix"}));
            QCOMPARE(moduleProps.getModuleProperty("qbs", "toolchain").toStringList(),
                     QStringList({"gcc"}));
            QCOMPARE(QFileInfo(moduleProps.getModuleProperty("cpp", "cxxCompilerName").toString())
                     .completeBaseName(), QString("g++"));
            QCOMPARE(moduleProps.getModuleProperty("cpp", "dynamicLibrarySuffix").toString(),
                     QString(".so"));
        } else {
            QCOMPARE(p.profile(), QString("gccProfile"));
            qbs::PropertyMap moduleProps = p.moduleProperties();
            QCOMPARE(moduleProps.getModuleProperty("qbs", "targetOS").toStringList(),
                     QStringList());
            QCOMPARE(moduleProps.getModuleProperty("qbs", "toolchain").toStringList(),
                     QStringList({"gcc"}));
            QCOMPARE(QFileInfo(moduleProps.getModuleProperty("cpp", "cxxCompilerName").toString())
                     .completeBaseName(), QString("g++"));
            QCOMPARE(moduleProps.getModuleProperty("cpp", "dynamicLibrarySuffix").toString(),
                     QString());
        }
    }
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
            = defaultSetupParameters("filetagsfilter_override");
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                         m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    qbs::Project project = job->project();

    const qbs::ErrorInfo errorInfo = forceRuleEvaluation(project);
    VERIFY_NO_ERROR(errorInfo);

    qbs::ProjectData projectData = project.projectData();
    QCOMPARE(projectData.allProducts().size(), 1);
    const qbs::ProductData product = projectData.allProducts().front();
    QList<qbs::ArtifactData> installableFiles = product.installableArtifacts();
    QCOMPARE(installableFiles.size(), 1);
    QVERIFY(installableFiles.front().installData().installFilePath().contains("habicht"));
}

void TestApi::generatedFilesList()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("generated-files-list");
    std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                              m_logSink, 0));
    QVERIFY(waitForFinished(setupJob.get()));
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    qbs::BuildOptions options;
    options.setExecuteRulesOnly(true);
    const std::unique_ptr<qbs::BuildJob> buildJob(project.buildAllProducts(options));
    QVERIFY(waitForFinished(buildJob.get()));
    VERIFY_NO_ERROR(buildJob->error());
    const qbs::ProjectData projectData = project.projectData();
    QCOMPARE(projectData.products().size(), 1);
    const qbs::ProductData product = projectData.products().front();
    QString uiFilePath;
    QVERIFY(product.generatedArtifacts().size() >= 6);
    const auto artifacts = product.generatedArtifacts();
    for (const qbs::ArtifactData &a : artifacts) {
        QVERIFY(a.isGenerated());
        QFileInfo fi(a.filePath());
        using qbs::Internal::HostOsInfo;
        const QStringList possibleFileNames = QStringList()
                << "main.cpp.o" << "main.cpp.obj"
                << "mainwindow.cpp.o" << "mainwindow.cpp.obj"
                << "moc_mainwindow.cpp" << "moc_mainwindow.cpp.o" << "moc_mainwindow.cpp.obj"
                << "ui_mainwindow.h"
                << HostOsInfo::appendExecutableSuffix("generated-files-list");
        QVERIFY2(possibleFileNames.contains(fi.fileName()) || fi.fileName().endsWith(".plist")
                 || fi.fileName().contains("qt_plugin_import"),
                 qPrintable(fi.fileName()));
    }
    const auto groups = product.groups();
    for (const qbs::GroupData &group : groups) {
        const auto artifacts = group.sourceArtifacts();
        for (const qbs::ArtifactData &a : artifacts) {
            QVERIFY(!a.isGenerated());
            QVERIFY(!a.isTargetArtifact());
            if (a.fileTags().contains(QLatin1String("ui"))) {
                uiFilePath = a.filePath();
                break;
            }
        }
        if (!uiFilePath.isEmpty())
            break;
    }
    QVERIFY(!uiFilePath.isEmpty());
    const QStringList directParents = project.generatedFiles(product, uiFilePath, false);
    QCOMPARE(directParents.size(), 1);
    const QFileInfo uiHeaderFileInfo(directParents.front());
    QCOMPARE(uiHeaderFileInfo.fileName(), QStringLiteral("ui_mainwindow.h"));
    QVERIFY(!uiHeaderFileInfo.exists());
    const QStringList allParents = project.generatedFiles(product, uiFilePath, true);
    QCOMPARE(allParents.size(), 3);
}

void TestApi::infiniteLoopBuilding()
{
    QFETCH(QString, projectDirName);
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters(projectDirName + "/infinite-loop.qbs");
    std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                              m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    const std::unique_ptr<qbs::BuildJob> buildJob(project.buildAllProducts(qbs::BuildOptions()));
    QTimer::singleShot(1000, buildJob.get(), &qbs::AbstractJob::cancel);
    QVERIFY(waitForFinished(buildJob.get(), testTimeoutInMsecs()));
    QVERIFY(buildJob->error().hasError());
}

void TestApi::infiniteLoopBuilding_data()
{
    QTest::addColumn<QString>("projectDirName");
    QTest::newRow("JS Command") << QString("infinite-loop-js");
    QTest::newRow("Process Command") << QString("infinite-loop-process");
}

void TestApi::infiniteLoopResolving()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("infinite-loop-resolving");
    std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                              m_logSink, 0));
    QTimer::singleShot(1000, setupJob.get(), &qbs::AbstractJob::cancel);
    QVERIFY(waitForFinished(setupJob.get(), testTimeoutInMsecs()));
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
    QVERIFY2(errorInfo.toString().contains("Dependency 'bli' not found"),
             qPrintable(errorInfo.toString()));

    QVariantMap overriddenValues;
    overriddenValues.insert("project.qbsSearchPaths",
                            QStringList() << m_workingDataDir + "/inherit-qbs-search-paths/subdir");
    errorInfo = doBuildProject(projectFilePath, 0, 0, 0, qbs::BuildOptions(), overriddenValues);
    VERIFY_NO_ERROR(errorInfo);
}

template <typename T, class Pred> typename T::value_type findElem(const T &list, Pred p)
{
    const auto it = std::find_if(list.cbegin(), list.cend(), p);
    return it == list.cend() ? typename T::value_type() : *it;
}

void TestApi::installableFiles()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("installed-artifact");
    QVariantMap overriddenValues;
    overriddenValues.insert(QStringLiteral("qbs.installRoot"), QStringLiteral("/tmp"));
    setupParams.setOverriddenValues(overriddenValues);
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                         m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    qbs::Project project = job->project();

    const qbs::ErrorInfo errorInfo = forceRuleEvaluation(project);
    VERIFY_NO_ERROR(errorInfo);

    qbs::ProjectData projectData = project.projectData();
    QCOMPARE(projectData.allProducts().size(), 2);
    qbs::ProductData product = findElem(projectData.allProducts(), [](const qbs::ProductData &p) {
        return p.name() == QLatin1String("installedApp");
    });
    QVERIFY(product.isValid());
    const QList<qbs::ArtifactData> beforeInstallableFiles = product.installableArtifacts();
    QCOMPARE(beforeInstallableFiles.size(), 3);
    for (const qbs::ArtifactData &f : beforeInstallableFiles) {
        if (!QFileInfo(f.filePath()).fileName().startsWith("main")) {
            QVERIFY(f.isExecutable());
            QString expectedTargetFilePath = qbs::Internal::HostOsInfo
                    ::appendExecutableSuffix(QStringLiteral("/tmp/usr/bin/installedApp"));
            QCOMPARE(f.installData().localInstallFilePath(), expectedTargetFilePath);
            QCOMPARE(product.targetExecutable(), expectedTargetFilePath);
            break;
        }
    }

    setupParams  = defaultSetupParameters("recursive-wildcards");
    setupParams.setOverriddenValues(overriddenValues);
    job.reset(project.setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    project = job->project();
    projectData = project.projectData();
    QCOMPARE(projectData.allProducts().size(), 1);
    product = projectData.allProducts().front();
    const QList<qbs::ArtifactData>  afterInstallableFiles = product.installableArtifacts();
    QCOMPARE(afterInstallableFiles.size(), 2);
    for (const qbs::ArtifactData &f : afterInstallableFiles)
        QVERIFY(!f.isExecutable());
    QCOMPARE(afterInstallableFiles.front().installData().localInstallFilePath(),
             QLatin1String("/tmp/dir/file1.txt"));
    QCOMPARE(afterInstallableFiles.last().installData().localInstallFilePath(),
             QLatin1String("/tmp/dir/file2.txt"));
}

void TestApi::isRunnable()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("is-runnable");
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    qbs::Project project = job->project();
    const QList<qbs::ProductData> products = project.projectData().products();
    QCOMPARE(products.size(), 2);
    for (const qbs::ProductData &p : products) {
        QVERIFY2(p.name() == "app" || p.name() == "lib", qPrintable(p.name()));
        if (p.name() == "app")
            QVERIFY(p.isRunnable());
        else
            QVERIFY(!p.isRunnable());
    }
}

void TestApi::linkDynamicLibs()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("link-dynamiclibs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::linkDynamicAndStaticLibs()
{
    BuildDescriptionReceiver bdr;
    qbs::BuildOptions options;
    options.setEchoMode(qbs::CommandEchoModeCommandLine);
    const qbs::ErrorInfo errorInfo = doBuildProject("link-dynamiclibs-staticlibs", &bdr, nullptr,
                                                    nullptr, options);
    VERIFY_NO_ERROR(errorInfo);

    // The dependent static libs should not appear in the link command for the executable.
    const SettingsPtr s = settings();
    const qbs::Profile buildProfile(profileName(), s.get());
    if (profileToolchain(buildProfile).contains("gcc")) {
        static const std::regex appLinkCmdRex(" -o [^ ]*/HelloWorld" QBS_HOST_EXE_SUFFIX " ");
        QString appLinkCmd;
        for (const QString &line : qAsConst(bdr.descriptionLines)) {
            const auto ln = line.toStdString();
            if (std::regex_search(ln, appLinkCmdRex)) {
                appLinkCmd = line;
                break;
            }
        }
        QVERIFY(!appLinkCmd.isEmpty());
        QVERIFY(!appLinkCmd.contains("static1"));
        QVERIFY(!appLinkCmd.contains("static2"));
    }
}

void TestApi::linkStaticAndDynamicLibs()
{
    BuildDescriptionReceiver bdr;
    qbs::BuildOptions options;
    options.setEchoMode(qbs::CommandEchoModeCommandLine);
    m_logSink->output.clear();
    const qbs::ErrorInfo errorInfo = doBuildProject("link-staticlibs-dynamiclibs", &bdr, nullptr,
                                                    nullptr, options);
    VERIFY_NO_ERROR(errorInfo);
    const bool isNormalUnix = m_logSink->output.contains("is normal unix: yes");
    const bool isNotNormalUnix = m_logSink->output.contains("is normal unix: no");
    QVERIFY2(isNormalUnix != isNotNormalUnix, qPrintable(m_logSink->output));

    // The dependencies libdynamic1.so and libstatic2.a must not appear in the link command for the
    // executable. The -rpath-link line for libdynamic1.so must be there.
    const SettingsPtr s = settings();
    const qbs::Profile buildProfile(profileName(), s.get());
    if (profileToolchain(buildProfile).contains("gcc")) {
        static const std::regex appLinkCmdRex(" -o [^ ]*/HelloWorld" QBS_HOST_EXE_SUFFIX " ");
        QString appLinkCmd;
        for (const QString &line : qAsConst(bdr.descriptionLines)) {
            const auto ln = line.toStdString();
            if (std::regex_search(ln, appLinkCmdRex)) {
                appLinkCmd = line;
                break;
            }
        }
        QVERIFY(!appLinkCmd.isEmpty());
        if (isNormalUnix) {
            const std::regex rpathLinkRex("-rpath-link=\\S*/"
                                          + relativeProductBuildDir("dynamic2").toStdString());
            const auto ln = appLinkCmd.toStdString();
            QVERIFY(std::regex_search(ln, rpathLinkRex));
        }
        QVERIFY(!appLinkCmd.contains("libstatic2.a"));
        QVERIFY(!appLinkCmd.contains("libdynamic2.so"));
    }
}

void TestApi::listBuildSystemFiles()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("subprojects/toplevelproject.qbs");
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    const auto buildSystemFiles = qbs::Internal::Set<QString>::fromStdSet(
                job->project().buildSystemFiles());
    QVERIFY(buildSystemFiles.contains(setupParams.projectFilePath()));
    QVERIFY(buildSystemFiles.contains(setupParams.buildRoot() + "/subproject2/subproject2.qbs"));
    QVERIFY(buildSystemFiles.contains(setupParams.buildRoot()
                                      + "/subproject2/subproject3/subproject3.qbs"));
}

void TestApi::localProfiles()
{
    QFETCH(bool, enableProfiles);
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("local-profiles/local-profiles.qbs");
    setupParams.setOverriddenValues(
        {std::make_pair(QString("project.enableProfiles"), enableProfiles)});
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    QString taskDescriptions;
    const auto taskDescHandler = [&taskDescriptions](const QString &desc, int, qbs::AbstractJob *) {
        taskDescriptions += '\n' + desc;
    };
    connect(job.get(), &qbs::AbstractJob::taskStarted, taskDescHandler);
    waitForFinished(job.get());
    const QString error = job->error().toString();
    QVERIFY2(job->error().hasError() == !enableProfiles, qPrintable(error));
    if (!enableProfiles) {
        QVERIFY2(error.contains("does not exist"), qPrintable(error));
        return;
    }
    QVERIFY2(taskDescriptions.contains("Resolving"), qPrintable(taskDescriptions));

    qbs::ProjectData project = job->project().projectData();
    QList<qbs::ProductData> products = project.allProducts();
    QCOMPARE(products.size(), 4);
    qbs::ProductData libMingw;
    qbs::ProductData libClang;
    qbs::ProductData appDebug;
    qbs::ProductData appRelease;
    for (const qbs::ProductData &p : qAsConst(products)) {
        if (p.name() == "lib") {
            if (p.profile() == "mingwProfile")
                libMingw = p;
            else if (p.profile() == "clangProfile")
                libClang = p;
        } else if (p.name() == "app") {
            const QString buildVariant
                    = p.moduleProperties().getModuleProperty("qbs", "buildVariant").toString();
            if (buildVariant == "debug")
                appDebug = p;
            else if (buildVariant == "release")
                appRelease = p;

        }
    }
    QVERIFY(libMingw.isValid());
    QVERIFY((libClang.isValid()));
    QVERIFY(appDebug.isValid());
    QVERIFY(appRelease.isValid());
    QCOMPARE(appDebug.profile(), QLatin1String("mingwProfile"));
    QCOMPARE(appRelease.profile(), QLatin1String("mingwProfile"));

    qbs::PropertyMap moduleProps = libMingw.moduleProperties();
    QCOMPARE(moduleProps.getModuleProperty("qbs", "targetOS").toStringList(),
             QStringList({"windows"}));
    QCOMPARE(moduleProps.getModuleProperty("qbs", "toolchain").toStringList(),
             QStringList({"mingw", "gcc"}));
    if (moduleProps.getModuleProperty("cpp", "present").toBool()) {
        QCOMPARE(moduleProps.getModuleProperty("cpp", "cxxCompilerName").toString(),
                 qbs::Internal::HostOsInfo::appendExecutableSuffix(QString("g++")));
    }
    moduleProps = libClang.moduleProperties();
    QCOMPARE(moduleProps.getModuleProperty("qbs", "targetOS").toStringList(),
             QStringList({"linux", "unix"}));
    QCOMPARE(moduleProps.getModuleProperty("qbs", "toolchain").toStringList(),
             QStringList({"clang", "llvm", "gcc"}));
    if (moduleProps.getModuleProperty("cpp", "present").toBool()) {
        QCOMPARE(moduleProps.getModuleProperty("cpp", "cxxCompilerName").toString(),
                 qbs::Internal::HostOsInfo::appendExecutableSuffix(QString("clang++")));
    }
    moduleProps = appDebug.moduleProperties();
    if (moduleProps.getModuleProperty("cpp", "present").toBool())
        QCOMPARE(moduleProps.getModuleProperty("cpp", "optimization").toString(), QString("none"));
    moduleProps = appRelease.moduleProperties();
    if (moduleProps.getModuleProperty("cpp", "present").toBool())
        QCOMPARE(moduleProps.getModuleProperty("cpp", "optimization").toString(), QString("fast"));

    taskDescriptions.clear();
    job.reset(qbs::Project().setupProject(setupParams, m_logSink, 0));
    connect(job.get(), &qbs::AbstractJob::taskStarted, taskDescHandler);
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    QVERIFY2(!taskDescriptions.contains("Resolving"), qPrintable(taskDescriptions));

    WAIT_FOR_NEW_TIMESTAMP();
    QFile projectFile(setupParams.projectFilePath());
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    QByteArray content = projectFile.readAll();
    content.replace("\"clang\"", "\"gcc\"");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    job.reset(qbs::Project().setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    project = job->project().projectData();
    products = project.allProducts();
    QCOMPARE(products.size(), 4);
    int clangProfiles = 0;
    for (const qbs::ProductData &p : qAsConst(products)) {
        if (p.profile() == "clangProfile") {
            ++clangProfiles;
            moduleProps = p.moduleProperties();
            if (moduleProps.getModuleProperty("cpp", "present").toBool()) {
                QCOMPARE(moduleProps.getModuleProperty("cpp", "cxxCompilerName").toString(),
                         qbs::Internal::HostOsInfo::appendExecutableSuffix(QString("g++")));
            }
        }
    }
    QCOMPARE(clangProfiles, 1);
}

void TestApi::localProfiles_data()
{
    QTest::addColumn<bool>("enableProfiles");
    QTest::newRow("profiles enabled") << true;
    QTest::newRow("profiles disabled") << false;
}

void TestApi::missingSourceFile()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("missing-source-file/missing-source-file.qbs");
    setupParams.setProductErrorMode(qbs::ErrorHandlingMode::Relaxed);
    m_logSink->setLogLevel(qbs::LoggerMinLevel);
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    qbs::ProjectData project = job->project().projectData();
    QCOMPARE(project.allProducts().size(), 1);
    qbs::ProductData product = project.allProducts().front();
    QCOMPARE(product.groups().size(), 1);
    qbs::GroupData group = product.groups().front();
    QCOMPARE(group.allSourceArtifacts().size(), 2);

    QFile::rename("file2.txt.missing", "file2.txt");
    job.reset(qbs::Project().setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    project = job->project().projectData();
    QCOMPARE(project.allProducts().size(), 1);
    product = project.allProducts().front();
    QCOMPARE(product.groups().size(), 1);
    group = product.groups().front();
    QCOMPARE(group.allSourceArtifacts().size(), 3);
}

void TestApi::mocCppIncluded()
{
    // Initial build.
    qbs::ErrorInfo errorInfo = doBuildProject("moc-hpp-included");
    VERIFY_NO_ERROR(errorInfo);

    // Touch header and try again.
    WAIT_FOR_NEW_TIMESTAMP();
    QFile headerFile("object.h");
    QVERIFY2(headerFile.open(QIODevice::WriteOnly | QIODevice::Append),
             qPrintable(headerFile.errorString()));
    headerFile.write("\n");
    headerFile.close();
    errorInfo = doBuildProject("moc-hpp-included");
    VERIFY_NO_ERROR(errorInfo);

    // Touch cpp file and try again.
    WAIT_FOR_NEW_TIMESTAMP();
    QFile cppFile("object.cpp");
    QVERIFY2(cppFile.open(QIODevice::WriteOnly | QIODevice::Append),
             qPrintable(cppFile.errorString()));
    cppFile.write("\n");
    cppFile.close();
    errorInfo = doBuildProject("moc-hpp-included");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::multiArch()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("multi-arch");
    const SettingsPtr s = settings();
    qbs::Internal::TemporaryProfile tph("host", s.get());
    qbs::Profile hostProfile = tph.p;
    hostProfile.setValue("qbs.architecture", "host-arch");
    qbs::Internal::TemporaryProfile tpt("target", s.get());
    qbs::Profile targetProfile = tpt.p;
    targetProfile.setValue("qbs.architecture", "target-arch");
    QVariantMap overriddenValues;
    overriddenValues.insert("project.hostProfile", hostProfile.name());
    overriddenValues.insert("project.targetProfile", targetProfile.name());
    setupParams.setOverriddenValues(overriddenValues);
    std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    QCOMPARE(project.profile(), profileName());
    const QList<qbs::ProductData> &products = project.projectData().products();
    QCOMPARE(products.size(), 3);
    QList<qbs::ProductData> hostProducts;
    QList<qbs::ProductData> targetProducts;
    for (const qbs::ProductData &p : products) {
        QVERIFY2(p.profile() == hostProfile.name() || p.profile() == targetProfile.name(),
                 qPrintable(p.profile()));
        if (p.profile() == hostProfile.name())
            hostProducts.push_back(p);
        else
            targetProducts.push_back(p);
    }
    QCOMPARE(hostProducts.size(), 2);
    QCOMPARE(targetProducts.size(), 1);
    QCOMPARE(targetProducts.front().name(), QLatin1String("p1"));
    QStringList hostProductNames
            = QStringList() << hostProducts.front().name() << hostProducts.last().name();
    QCOMPARE(hostProductNames.count("p1"), 1);
    QCOMPARE(hostProductNames.count("p2"), 1);

    const QString p1HostMultiplexCfgId = hostProducts.at(0).multiplexConfigurationId();
    const QString p2HostMultiplexCfgId = hostProducts.at(1).multiplexConfigurationId();
    const QString p1TargetMultiplexCfgId = targetProducts.at(0).multiplexConfigurationId();

    std::unique_ptr<qbs::BuildJob> buildJob(project.buildAllProducts(qbs::BuildOptions()));
    waitForFinished(buildJob.get());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    const QString outputBaseDir = setupParams.buildRoot() + '/';
    QFile p1HostArtifact(outputBaseDir
                         + relativeProductBuildDir("p1", QString(), p1HostMultiplexCfgId)
                         + "/host+target.output");
    QVERIFY2(p1HostArtifact.exists(), qPrintable(p1HostArtifact.fileName()));
    QVERIFY2(p1HostArtifact.open(QIODevice::ReadOnly), qPrintable(p1HostArtifact.errorString()));
    QCOMPARE(p1HostArtifact.readAll().constData(), "host-arch");
    QFile p1TargetArtifact(outputBaseDir
                           + relativeProductBuildDir("p1", QString(), p1TargetMultiplexCfgId)
                           + "/host+target.output");
    QVERIFY2(p1TargetArtifact.exists(), qPrintable(p1TargetArtifact.fileName()));
    QVERIFY2(p1TargetArtifact.open(QIODevice::ReadOnly), qPrintable(p1TargetArtifact.errorString()));
    QCOMPARE(p1TargetArtifact.readAll().constData(), "target-arch");
    QFile p2Artifact(outputBaseDir
                     + relativeProductBuildDir("p2", QString(), p2HostMultiplexCfgId)
                     + "/host-tool.output");
    QVERIFY2(p2Artifact.exists(), qPrintable(p2Artifact.fileName()));
    QVERIFY2(p2Artifact.open(QIODevice::ReadOnly), qPrintable(p2Artifact.errorString()));
    QCOMPARE(p2Artifact.readAll().constData(), "host-arch");

    const QString installRoot = outputBaseDir + relativeBuildDir() + '/'
            + qbs::InstallOptions::defaultInstallRoot();
    std::unique_ptr<qbs::InstallJob> installJob(project.installAllProducts(qbs::InstallOptions()));
    waitForFinished(installJob.get());
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
    waitForFinished(setupJob.get());
    QVERIFY(setupJob->error().hasError());
    QVERIFY2(setupJob->error().toString().contains("Duplicate entry 'host' in qbs.profiles."),
             qPrintable(setupJob->error().toString()));

    // Error check: Try to build for the same profile twice, this time attaching
    // the properties via the product name.
    overriddenValues.remove(QStringLiteral("project.targetProfile"));
    overriddenValues.insert("products.p1.myProfiles",
                            targetProfile.name() + ',' + targetProfile.name());
    setupParams.setOverriddenValues(overriddenValues);
    setupJob.reset(project.setupProject(setupParams, m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY(setupJob->error().hasError());
    QVERIFY2(setupJob->error().toString().contains("Duplicate entry 'target' in qbs.profiles."),
             qPrintable(setupJob->error().toString()));
}

struct ProductDataSelector
{
    void clear()
    {
        name.clear();
        qbsProperties.clear();
    }

    bool matches(const qbs::ProductData &p) const
    {
        return name == p.name() && qbsPropertiesMatch(p);
    }

    bool qbsPropertiesMatch(const qbs::ProductData &p) const
    {
        for (auto it = qbsProperties.begin(); it != qbsProperties.end(); ++it) {
            if (it.value() != p.moduleProperties().getModuleProperty("qbs", it.key()))
                return false;
        }
        return true;
    }

    QString name;
    QVariantMap qbsProperties;
};

static qbs::ProductData takeMatchingProduct(QList<qbs::ProductData> &products,
                                            const ProductDataSelector &s)
{
    qbs::ProductData result;
    auto it = std::find_if(products.begin(), products.end(),
                           [&s] (const qbs::ProductData &pd) { return s.matches(pd); });
    if (it != products.end()) {
        result = *it;
        products.erase(it);
    }
    return result;
}

void TestApi::multiplexing()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("multiplexing");
    std::unique_ptr<qbs::SetupProjectJob> setupJob(
                qbs::Project().setupProject(setupParams, m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    QList<qbs::ProductData> products = project.projectData().allProducts();
    qbs::ProductData product;
    ProductDataSelector selector;
    selector.name = "no-multiplexing";
    product = takeMatchingProduct(products,  selector);
    QVERIFY(product.isValid());
    QVERIFY(!product.isMultiplexed());
    QVERIFY(product.dependencies().empty());

    selector.clear();
    selector.name = "multiplex-without-aggregator-2";
    selector.qbsProperties["architecture"] = "TRS-80";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QVERIFY(product.dependencies().empty());

    selector.qbsProperties["architecture"] = "C64";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QVERIFY(product.dependencies().empty());

    selector.clear();
    selector.name = "multiplex-with-export";
    selector.qbsProperties["architecture"] = "TRS-80";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QVERIFY(product.dependencies().empty());

    selector.qbsProperties["architecture"] = "C64";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QVERIFY(product.dependencies().empty());

    selector.clear();
    selector.name = "nonmultiplex-with-export";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(!product.isMultiplexed());
    QVERIFY(product.dependencies().empty());

    selector.clear();
    selector.name = "nonmultiplex-exporting-aggregation";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(!product.isMultiplexed());
    QVERIFY(product.dependencies().empty());

    selector.clear();
    selector.name = "multiplex-using-export";
    selector.qbsProperties["architecture"] = "TRS-80";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 2);

    selector.qbsProperties["architecture"] = "C64";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 2);

    selector.clear();
    selector.name = "multiplex-without-aggregator-2-depend-on-non-multiplexed";
    selector.qbsProperties["architecture"] = "TRS-80";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 1);

    selector.qbsProperties["architecture"] = "C64";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 1);

    selector.clear();
    selector.name = "multiplex-without-aggregator-4";
    selector.qbsProperties["architecture"] = "C64";
    selector.qbsProperties["buildVariant"] = "debug";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QVERIFY(product.dependencies().empty());
    selector.qbsProperties["buildVariant"] = "release";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QVERIFY(product.dependencies().empty());
    selector.qbsProperties["architecture"] = "TRS-80";
    selector.qbsProperties["buildVariant"] = "debug";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QVERIFY(product.dependencies().empty());
    selector.qbsProperties["buildVariant"] = "release";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QVERIFY(product.dependencies().empty());

    selector.clear();
    selector.name = "multiplex-without-aggregator-4-depends-2";
    selector.qbsProperties["architecture"] = "C64";
    selector.qbsProperties["buildVariant"] = "debug";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 1);
    selector.qbsProperties["buildVariant"] = "release";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 1);
    selector.qbsProperties["architecture"] = "TRS-80";
    selector.qbsProperties["buildVariant"] = "debug";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 1);
    selector.qbsProperties["buildVariant"] = "release";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 1);

    selector.clear();
    selector.name = "multiplex-with-aggregator-2";
    selector.qbsProperties["architecture"] = "C64";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 0);
    selector.qbsProperties["architecture"] = "TRS-80";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 0);
    selector.qbsProperties["architecture"] = "Atari ST";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 2);

    selector.clear();
    selector.name = "multiplex-with-aggregator-2-dependent";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(!product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 1);

    selector.clear();
    selector.name = "non-multiplexed-with-dependencies-on-multiplexed";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(!product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 2);

    selector.clear();
    selector.name = "non-multiplexed-with-dependencies-on-multiplexed-via-export1";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(!product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 4);

    selector.clear();
    selector.name = "non-multiplexed-with-dependencies-on-multiplexed-via-export2";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(!product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 3);

    selector.clear();
    selector.name = "non-multiplexed-with-dependencies-on-aggregation-via-export";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(!product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 2);

    selector.clear();
    selector.name = "aggregate-with-dependencies-on-aggregation-via-export";
    selector.qbsProperties["architecture"] = "C64";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 2);
    selector.qbsProperties["architecture"] = "TRS-80";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 2);
    selector.qbsProperties["architecture"] = "Atari ST";
    product = takeMatchingProduct(products, selector);
    QVERIFY(product.isValid());
    QVERIFY(product.isMultiplexed());
    QCOMPARE(product.dependencies().size(), 4);

    QVERIFY(products.empty());
}

void TestApi::newOutputArtifactInDependency()
{
    BuildDescriptionReceiver receiver;
    qbs::ErrorInfo errorInfo
            = doBuildProject("new-output-artifact-in-dependency", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(receiver.descriptions.contains("linking app"));
    const QByteArray linkingLibString = QByteArray("linking ")
            + qbs::Internal::HostOsInfo::dynamicLibraryName("lib").toLatin1();
    QVERIFY(!receiver.descriptions.contains(linkingLibString));
    receiver.descriptions.clear();

    WAIT_FOR_NEW_TIMESTAMP();
    QFile projectFile("new-output-artifact-in-dependency.qbs");
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    QByteArray contents = projectFile.readAll();
    contents.replace("//Depends", "Depends");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    errorInfo = doBuildProject("new-output-artifact-in-dependency", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(receiver.descriptions.contains("linking app"));
    QVERIFY(receiver.descriptions.contains(linkingLibString));
}

void TestApi::newPatternMatch()
{
    TaskReceiver receiver;
    qbs::ErrorInfo errorInfo = doBuildProject("new-pattern-match", 0, 0, &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY2(receiver.taskDescriptions.contains("Resolving"), qPrintable(m_logSink->output));
    receiver.taskDescriptions.clear();

    errorInfo = doBuildProject("new-pattern-match", 0, 0, &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(!receiver.taskDescriptions.contains("Resolving"));

    WAIT_FOR_NEW_TIMESTAMP();
    QFile f("test.txt");
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(f.errorString()));
    f.close();
    errorInfo = doBuildProject("new-pattern-match", 0, 0, &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(receiver.taskDescriptions.contains("Resolving"));
    receiver.taskDescriptions.clear();

    errorInfo = doBuildProject("new-pattern-match", 0, 0, &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(!receiver.taskDescriptions.contains("Resolving"));

    WAIT_FOR_NEW_TIMESTAMP();
    f.remove();
    errorInfo = doBuildProject("new-pattern-match", 0, 0, &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY(receiver.taskDescriptions.contains("Resolving"));
}

void TestApi::nonexistingProjectPropertyFromProduct()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("nonexistingprojectproperties");
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.get());
    QEXPECT_FAIL("", "QBS-432", Abort);
    QVERIFY(job->error().hasError());
    QVERIFY2(job->error().toString().contains(QLatin1String("blubb")),
             qPrintable(job->error().toString()));
}

void TestApi::nonexistingProjectPropertyFromCommandLine()
{
    qbs::SetupProjectParameters setupParams
            = defaultSetupParameters("nonexistingprojectproperties");
    removeBuildDir(setupParams);
    QVariantMap projectProperties;
    projectProperties.insert(QStringLiteral("project.blubb"), QStringLiteral("true"));
    setupParams.setOverriddenValues(projectProperties);
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY(job->error().hasError());
    QVERIFY2(job->error().toString().contains(QLatin1String("blubb")),
             qPrintable(job->error().toString()));
}

void TestApi::objC()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("objc");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::projectDataAfterProductInvalidation()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("project-data-after-"
            "product-invalidation/project-data-after-product-invalidation.qbs");
    std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                              m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    QVERIFY(project.isValid());
    QCOMPARE(project.projectData().products().size(), 1);
    QVERIFY(project.projectData().products().front().generatedArtifacts().empty());
    std::unique_ptr<qbs::BuildJob> buildJob(project.buildAllProducts(qbs::BuildOptions()));
    waitForFinished(buildJob.get());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    QCOMPARE(project.projectData().products().size(), 1);
    const qbs::ProductData productAfterBulding = project.projectData().products().front();
    QVERIFY(!productAfterBulding.generatedArtifacts().empty());
    QFile projectFile(setupParams.projectFilePath());
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    QByteArray content = projectFile.readAll();
    QVERIFY(!content.isEmpty());
    content.replace("\"file.cpp", "// \"file.cpp");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.flush();
    setupJob.reset(project.setupProject(setupParams, m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    QVERIFY(!project.isValid());
    project = setupJob->project();
    QVERIFY(project.isValid());
    QCOMPARE(project.projectData().products().size(), 1);
    QVERIFY(project.projectData().products().front().generatedArtifacts()
            == productAfterBulding.generatedArtifacts());
    buildJob.reset(project.buildAllProducts(qbs::BuildOptions()));
    waitForFinished(buildJob.get());
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    QCOMPARE(project.projectData().products().size(), 1);
    QVERIFY(project.projectData().products().front().generatedArtifacts()
            != productAfterBulding.generatedArtifacts());
}

void TestApi::processResult()
{
    waitForFileUnlock();
    removeBuildDir(defaultSetupParameters("process-result"));

    QFETCH(int, expectedExitCode);
    QFETCH(bool, redirectStdout);
    QFETCH(bool, redirectStderr);
    QVariantMap overridden;
    overridden.insert("products.app-caller.argument", expectedExitCode);
    overridden.insert("products.app-caller.redirectStdout", redirectStdout);
    overridden.insert("products.app-caller.redirectStderr", redirectStderr);
    ProcessResultReceiver resultReceiver;
    const qbs::ErrorInfo errorInfo = doBuildProject("process-result",
            nullptr, &resultReceiver, nullptr, qbs::BuildOptions(), overridden);
    if (m_logSink->output.contains("targetPlatform differs from hostPlatform"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QCOMPARE(expectedExitCode != 0, errorInfo.hasError());
    QVERIFY(resultReceiver.results.size() > 1);
    const qbs::ProcessResult &result = resultReceiver.results.back();
    QVERIFY2(result.executableFilePath().contains("app"), qPrintable(result.executableFilePath()));
    QCOMPARE(expectedExitCode, result.exitCode());
    QCOMPARE(expectedExitCode == 0, result.success());
    QCOMPARE(result.error(), QProcess::UnknownError);
    struct CheckParams {
        CheckParams(bool r, QString f, QByteArray c, QStringList co)
            : redirect(r)
            , fileName(std::move(f))
            , expectedContent(std::move(c))
            , consoleOutput(std::move(co))
        {}
        bool redirect;
        QString fileName;
        QByteArray expectedContent;
        const QStringList consoleOutput;
    };
    const std::vector<CheckParams> checkParams({
        CheckParams(redirectStdout, "stdout.txt", "stdout", result.stdOut()),
        CheckParams(redirectStderr, "stderr.txt", "stderr", result.stdErr())
    });
    for (const CheckParams &p : checkParams) {
        QFile f(relativeProductBuildDir("app-caller") + '/' + p.fileName);
        QCOMPARE(f.exists(), p.redirect);
        if (p.redirect) {
            QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.errorString()));
            QCOMPARE(f.readAll(), p.expectedContent);
            QCOMPARE(p.consoleOutput, QStringList());
        } else {
            QCOMPARE(p.consoleOutput.join("").toLocal8Bit(), p.expectedContent);
        }
    }
}

void TestApi::processResult_data()
{
    QTest::addColumn<int>("expectedExitCode");
    QTest::addColumn<bool>("redirectStdout");
    QTest::addColumn<bool>("redirectStderr");
    QTest::newRow("success, no redirection") << 0 << false << false;
    QTest::newRow("success, stdout redirection") << 0 << true << false;
    QTest::newRow("failure, stderr redirection") << 1 << false << true;
}

void TestApi::projectInvalidation()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("project-invalidation");
    QVERIFY(QFile::copy("project.no-error.qbs", "project-invalidation.qbs"));
    std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    QVERIFY(project.isValid());
    WAIT_FOR_NEW_TIMESTAMP();
    copyFileAndUpdateTimestamp("project.early-error.qbs", "project-invalidation.qbs");
    setupJob.reset(project.setupProject(setupParams, m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY(setupJob->error().hasError());
    QVERIFY(project.isValid()); // Error in Loader, old project still valid.
    WAIT_FOR_NEW_TIMESTAMP();
    copyFileAndUpdateTimestamp("project.late-error.qbs", "project-invalidation.qbs");
    setupJob.reset(project.setupProject(setupParams, m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY(setupJob->error().hasError());
    QVERIFY(!project.isValid()); // Error in build data re-resolving, old project not valid anymore.
}

void TestApi::projectLocking()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("project-locking");
    std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    qbs::Project project = setupJob->project();
    setupJob.reset(project.setupProject(setupParams, m_logSink, 0));
    std::unique_ptr<qbs::SetupProjectJob> setupJob2(project.setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(setupJob2.get());
    QVERIFY(setupJob2->error().hasError());
    QVERIFY2(setupJob2->error().toString()
             .contains("Cannot start a job while another one is in progress."),
             qPrintable(setupJob2->error().toString()));
    waitForFinished(setupJob.get());
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
}

void TestApi::projectPropertiesByName()
{
    const QString projectFile = "project-properties-by-name/project-properties-by-name.qbs";
    qbs::ErrorInfo errorInfo = doBuildProject(projectFile);
    QVERIFY(errorInfo.hasError());
    QVariantMap overridden;
    overridden.insert("project.theDefines", QStringList() << "SUB1" << "SUB2");
    errorInfo = doBuildProject(projectFile, 0, 0, 0, qbs::BuildOptions(), overridden);
    QVERIFY(errorInfo.hasError());
    overridden.clear();
    overridden.insert("projects.subproject1.theDefines", QStringList() << "SUB1");
    errorInfo = doBuildProject(projectFile, 0, 0, 0, qbs::BuildOptions(), overridden);
    QVERIFY(errorInfo.hasError());
    overridden.insert("projects.subproject2.theDefines", QStringList() << "SUB2");
    errorInfo = doBuildProject(projectFile, 0, 0, 0, qbs::BuildOptions(), overridden);
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::projectWithPropertiesItem()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("project-with-properties-item");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::projectWithProbeAndProfileItem()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("project-with-probe-and-profile-item");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::propertiesBlocks()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("properties-blocks");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::rc()
{
    BuildDescriptionReceiver bdr;
    ProcessResultReceiver prr;
    const auto buildRc = [this, &bdr, &prr]() {
        bdr.descriptions.clear();
        bdr.descriptionLines.clear();
        prr.output.clear();
        prr.results.clear();
        const qbs::ErrorInfo errorInfo = doBuildProject("rc", &bdr, &prr);
        if (errorInfo.hasError())
            qDebug() << prr.output;
        return errorInfo;
    };
    const auto rcFileWasCompiled = [&bdr]() {
        return bdr.descriptions.contains("compiling test.rc");
    };
    qbs::ErrorInfo error = buildRc();
    VERIFY_NO_ERROR(error);
    QCOMPARE(rcFileWasCompiled(), qbs::Internal::HostOsInfo::isWindowsHost());
    WAIT_FOR_NEW_TIMESTAMP();
    error = buildRc();
    VERIFY_NO_ERROR(error);
    QVERIFY(!rcFileWasCompiled());
    touch("subdir/rc-include.h");
    error = buildRc();
    VERIFY_NO_ERROR(error);
    QCOMPARE(rcFileWasCompiled(), qbs::Internal::HostOsInfo::isWindowsHost());
}

void TestApi::referencedFileErrors()
{
    QFETCH(bool, relaxedMode);
    qbs::SetupProjectParameters params = defaultSetupParameters("referenced-file-errors");
    params.setDryRun(true);
    params.setProductErrorMode(relaxedMode ? qbs::ErrorHandlingMode::Relaxed
                                           : qbs::ErrorHandlingMode::Strict);
    m_logSink->setLogLevel(qbs::LoggerMinLevel);
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(params, m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(job->error().hasError() != relaxedMode, qPrintable(job->error().toString()));
    const qbs::Project project = job->project();
    QCOMPARE(project.isValid(), relaxedMode);
    if (!relaxedMode)
        return;
    const QList<qbs::ProductData> products = project.projectData().allProducts();
    QCOMPARE(products.size(), 5);
    for (const qbs::ProductData &p : products)
        QCOMPARE(p.isEnabled(), p.name() != "p5");
}

void TestApi::referencedFileErrors_data()
{
    QTest::addColumn<bool>("relaxedMode");
    QTest::newRow("strict mode") << false;
    QTest::newRow("relaxed mode") << true;
}

qbs::SetupProjectParameters TestApi::defaultSetupParameters(const QString &projectFileOrDir) const
{
    QFileInfo fi(m_workingDataDir + QLatin1Char('/') + projectFileOrDir);
    QString projectDirPath;
    QString projectFilePath;
    if (fi.isDir()) {
        projectDirPath = fi.absoluteFilePath();
        projectFilePath = projectDirPath + QLatin1Char('/') + projectFileOrDir
                + QStringLiteral(".qbs");
    } else {
        projectDirPath = fi.absolutePath();
        projectFilePath = fi.absoluteFilePath();
    }

    qbs::SetupProjectParameters setupParams;
    setupParams.setEnvironment(QProcessEnvironment::systemEnvironment());
    setupParams.setProjectFilePath(projectFilePath);
    setupParams.setPropertyCheckingMode(qbs::ErrorHandlingMode::Strict);
    setupParams.setOverrideBuildGraphData(true);
    QDir::setCurrent(projectDirPath);
    setupParams.setBuildRoot(projectDirPath);
    const SettingsPtr s = settings();
    const qbs::Preferences prefs(s.get(), profileName());
    setupParams.setSearchPaths(prefs.searchPaths(QDir::cleanPath(QCoreApplication::applicationDirPath()
            + QLatin1String("/" QBS_RELATIVE_SEARCH_PATH))));
    setupParams.setPluginPaths(prefs.pluginPaths(QDir::cleanPath(QCoreApplication::applicationDirPath()
            + QLatin1String("/" QBS_RELATIVE_PLUGINS_PATH))));
    setupParams.setLibexecPath(QDir::cleanPath(QCoreApplication::applicationDirPath()
            + QLatin1String("/" QBS_RELATIVE_LIBEXEC_PATH)));
    setupParams.setTopLevelProfile(profileName());
    setupParams.setConfigurationName(QStringLiteral("default"));
    setupParams.setSettingsDirectory(settings()->baseDirectory());
    return setupParams;
}

void TestApi::references()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("references/invalid1.qbs");
    const QString projectDir = QDir::cleanPath(m_workingDataDir + "/references");
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY(job->error().hasError());
    QString errorString = job->error().toString();
    QVERIFY2(errorString.contains("does not contain"), qPrintable(errorString));

    setupParams.setProjectFilePath(projectDir + QLatin1String("/invalid2.qbs"));
    job.reset(qbs::Project().setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY(job->error().hasError());
    errorString = job->error().toString();
    QVERIFY2(errorString.contains("contains more than one"), qPrintable(errorString));

    setupParams.setProjectFilePath(projectDir + QLatin1String("/valid.qbs"));
    job.reset(qbs::Project().setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    const qbs::ProjectData topLevelProject = job->project().projectData();
    QCOMPARE(topLevelProject.subProjects().size(), 1);
    const QString subProjectFileName
            = QFileInfo(topLevelProject.subProjects().front().location().filePath()).fileName();
    QCOMPARE(subProjectFileName, QString("p.qbs"));
}

void TestApi::relaxedModeRecovery()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("relaxed-mode-recovery");
    setupParams.setProductErrorMode(qbs::ErrorHandlingMode::Relaxed);
    setupParams.setPropertyCheckingMode(qbs::ErrorHandlingMode::Relaxed);
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    if (m_logSink->warnings.size() != 4) {
        const auto errors = m_logSink->warnings;
        for (const qbs::ErrorInfo &error : errors)
            qDebug() << error.toString();
    }
    QCOMPARE(m_logSink->warnings.size(), 4);

    const auto errors = m_logSink->warnings;
    for (const qbs::ErrorInfo &error : errors) {
        QVERIFY2(!error.toString().contains("ASSERT")
                 && (error.toString().contains("Dependency 'blubb' not found")
                     || error.toString().contains("Product 'p1' had errors and was disabled")
                     || error.toString().contains("Product 'p2' had errors and was disabled")),
                 qPrintable(error.toString()));
    }
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
    QVERIFY2(errorInfo.toString().contains("Dependency 'thelib' not found"),
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

    const qbs::SetupProjectParameters params = defaultSetupParameters(projectSubDir);
    removeBuildDir(params);
    const std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(params,
                                                                                    m_logSink, 0));
    waitForFinished(setupJob.get());
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

    qbs::SetupProjectParameters params = defaultSetupParameters(projectSubDir);
    params.setDryRun(true);
    removeBuildDir(params);
    const std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(params,
                                                                                    m_logSink, 0));
    waitForFinished(setupJob.get());
    VERIFY_NO_ERROR(setupJob->error());
    QVERIFY2(!QFile::exists(productFileName), qPrintable(productFileName));
    QVERIFY(!regularFileExists(relativeBuildGraphFilePath()));
}

void TestApi::resolveProjectDryRun_data()
{
    return resolveProject_data();
}

void TestApi::restoredWarnings()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("restored-warnings");
    setupParams.setPropertyCheckingMode(qbs::ErrorHandlingMode::Relaxed);
    setupParams.setProductErrorMode(qbs::ErrorHandlingMode::Relaxed);

    // Initial resolving: Errors are new.
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    job.reset(nullptr);
    QCOMPARE(toSet(m_logSink->warnings).size(), 2);
    const auto beforeErrors = m_logSink->warnings;
    for (const qbs::ErrorInfo &e : beforeErrors) {
        const QString msg = e.toString();
        QVERIFY2(msg.contains("Superfluous version")
                 || msg.contains("Property 'blubb' is not declared"),
                 qPrintable(msg));
    }
    m_logSink->warnings.clear();

    // Re-resolving with no changes: Errors come from the stored build graph.
    job.reset(qbs::Project().setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    job.reset(nullptr);
    QCOMPARE(toSet(m_logSink->warnings).size(), 2);
    m_logSink->warnings.clear();

    // Re-resolving with changes: Errors come from the re-resolving, stored ones must be suppressed.
    QVariantMap overridenValues;
    overridenValues.insert("products.theProduct.moreFiles", true);
    setupParams.setOverriddenValues(overridenValues);
    job.reset(qbs::Project().setupProject(setupParams, m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    job.reset(nullptr);
    QCOMPARE(toSet(m_logSink->warnings).size(), 3); // One more for the additional group
    const auto afterErrors = m_logSink->warnings;
    for (const qbs::ErrorInfo &e : afterErrors) {
        const QString msg = e.toString();
        QVERIFY2(msg.contains("Superfluous version")
                 || msg.contains("Property 'blubb' is not declared")
                 || msg.contains("blubb.cpp' does not exist"),
                 qPrintable(msg));
    }
    m_logSink->warnings.clear();
}

void TestApi::ruleConflict()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("rule-conflict");
    QVERIFY(errorInfo.hasError());
    const QString errorString = errorInfo.toString();
    QVERIFY2(errorString.contains("conflict") && errorString.contains("pch1.h")
             && errorString.contains("pch2.h"), qPrintable(errorString));
}

void TestApi::runEnvForDisabledProduct()
{
    const qbs::SetupProjectParameters params
            = defaultSetupParameters("run-disabled-product/run-disabled-product.qbs");
    const std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(params,
                                                                                    m_logSink, 0));
    QVERIFY(waitForFinished(setupJob.get()));
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    const qbs::Project project = setupJob->project();
    const std::unique_ptr<qbs::BuildJob> buildJob(project.buildAllProducts(qbs::BuildOptions()));
    QVERIFY(waitForFinished(buildJob.get()));
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    const qbs::ProjectData projectData = project.projectData();
    const QList<qbs::ProductData> products = projectData.products();
    QCOMPARE(products.size(), 1);
    const qbs::ProductData product = products.front();
    qbs::RunEnvironment runEnv = project.getRunEnvironment(
                product, qbs::InstallOptions(), QProcessEnvironment(), QStringList(),
                settings().get());
    qbs::ErrorInfo runError;
    const QProcessEnvironment env = runEnv.runEnvironment(&runError);
    QVERIFY2(runError.toString().contains("Cannot run disabled product 'app'"),
             qPrintable(runError.toString()));
    runError.clear();
    runEnv.runTarget(QString(), QStringList(), true, &runError);
    QVERIFY2(runError.toString().contains("Cannot run disabled product 'app'"),
             qPrintable(runError.toString()));
}

void TestApi::softDependency()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("soft-dependency");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::sourceFileInBuildDir()
{
    VERIFY_NO_ERROR(doBuildProject("source-file-in-build-dir"));
    qbs::SetupProjectParameters setupParams = defaultSetupParameters("source-file-in-build-dir");
    const QString generatedFile = relativeProductBuildDir("theProduct") + "/generated.cpp";
    QVERIFY2(regularFileExists(generatedFile), qPrintable(generatedFile));
    std::unique_ptr<qbs::SetupProjectJob> job(qbs::Project().setupProject(setupParams,
                                                                        m_logSink, 0));
    waitForFinished(job.get());
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    const qbs::ProjectData projectData = job->project().projectData();
    QCOMPARE(projectData.allProducts().size(), 1);
    const qbs::ProductData product = projectData.allProducts().front();
    QCOMPARE(product.profile(), profileName());
    const qbs::GroupData group = findGroup(product, "the group");
    QVERIFY(group.isValid());
    QCOMPARE(group.allFilePaths().size(), 1);
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
    contents.replace("condition: qbs.targetOS.length > 0", "condition: false");
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
    QVERIFY2(errorInfo.toString().contains("Dependency 'testLib' not found"),
             qPrintable(errorInfo.toString()));
}

void TestApi::targetArtifactStatus_data()
{
    QTest::addColumn<bool>("enableTagging");
    QTest::newRow("tagging off") << false;
    QTest::newRow("tagging on") << true;
    QTest::newRow("tagging off again") << false;
}

void TestApi::targetArtifactStatus()
{
    QFETCH(bool, enableTagging);
    qbs::SetupProjectParameters params
            = defaultSetupParameters("target-artifact-status/target-artifact-status.qbs");
    params.setOverriddenValues({std::make_pair("products.p.enableTagging", enableTagging)});
    const std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(params,
                                                                                     m_logSink, 0));
    waitForFinished(setupJob.get());
    VERIFY_NO_ERROR(setupJob->error());
    const qbs::Project project = setupJob->project();
    QVERIFY(project.isValid());
    const std::unique_ptr<qbs::BuildJob> buildJob(project.buildAllProducts(qbs::BuildOptions()));
    QVERIFY(waitForFinished(buildJob.get()));
    VERIFY_NO_ERROR(buildJob->error());
    const qbs::ProjectData projectData = project.projectData();
    const QList<qbs::ProductData> products = projectData.products();
    QCOMPARE(products.size(), 1);
    const qbs::ProductData product = products.front();
    QCOMPARE(product.targetArtifacts().size(), enableTagging ? 2 : 1);
}

void TestApi::timeout()
{
    QFETCH(QString, projectDirName);
    const auto setupParams = defaultSetupParameters(projectDirName + "/timeout.qbs");
    std::unique_ptr<qbs::SetupProjectJob> setupJob{
            qbs::Project().setupProject(setupParams, m_logSink, nullptr)};
    waitForFinished(setupJob.get());
    if (m_logSink->output.contains("targetPlatform differs from hostPlatform"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    auto project = setupJob->project();
    const auto products = project.projectData().products();
    QList<qbs::ProductData> helperProducts;
    qbs::ProductData productUnderTest;
    for (const auto &product : products) {
        if (!product.type().contains(QLatin1String("product-under-test")))
            helperProducts.append(product);
        else
            productUnderTest = product;
    }
    const std::unique_ptr<qbs::BuildJob> buildHelpersJob{
            project.buildSomeProducts(helperProducts, qbs::BuildOptions())};
    QVERIFY(waitForFinished(buildHelpersJob.get(), testTimeoutInMsecs()));
    if (buildHelpersJob->error().hasError()) {
        qDebug().noquote() << buildHelpersJob->error().toString();
        QFAIL("Could not build helper products");
    }

    const std::unique_ptr<qbs::BuildJob> buildJob(project.buildOneProduct(productUnderTest,
                                                                          qbs::BuildOptions()));
    QVERIFY(waitForFinished(buildJob.get(), testTimeoutInMsecs()));
    QVERIFY(buildJob->error().hasError());
    const auto errorString = buildJob->error().toString();
    QVERIFY2(errorString.contains("cancel"), qPrintable(errorString));
    QVERIFY(errorString.contains("timeout"));
}

void TestApi::timeout_data()
{
    QTest::addColumn<QString>("projectDirName");
    QTest::newRow("JS Command") << QString("timeout-js");
    QTest::newRow("Process Command") << QString("timeout-process");
}

void TestApi::toolInModule()
{
    QVariantMap overrides({std::make_pair("qbs.installRoot", m_workingDataDir
                           + "/tool-in-module/use-outside-project")});

    qbs::SetupProjectParameters params
            = defaultSetupParameters("tool-in-module/use-within-project/use-within-project.qbs");
    params.setOverriddenValues(overrides);
    std::unique_ptr<qbs::SetupProjectJob> setupJob(
                qbs::Project().setupProject(params, m_logSink, 0));
    QVERIFY(waitForFinished(setupJob.get()));
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    if (m_logSink->output.contains("Skip this test"))
        QSKIP("Skip this test");

    std::unique_ptr<qbs::BuildJob> buildJob(setupJob->project()
                                            .buildAllProducts(qbs::BuildOptions()));

    QVERIFY(waitForFinished(buildJob.get()));
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));

    const QString toolOutput = relativeProductBuildDir("user-in-project") + "/tool-output.txt";
    QVERIFY2(QFile::exists(toolOutput), qPrintable(toolOutput));

    params = defaultSetupParameters("tool-in-module/use-outside-project/use-outside-project.qbs");
    setupJob.reset(qbs::Project().setupProject(params, m_logSink, 0));
    QVERIFY(waitForFinished(setupJob.get()));
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    const auto project = setupJob->project();
    const auto projectData = project.projectData();
    const auto products = projectData.products();
    QCOMPARE(products.size(), 1);
    const qbs::ProductData product = products.front();
    const auto groups = product.groups();
    for (const qbs::GroupData &group : groups)
        QVERIFY(group.name() != "thetool binary");
    buildJob.reset(setupJob->project().buildAllProducts(qbs::BuildOptions()));
    QVERIFY(waitForFinished(buildJob.get()));
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    const QString toolOutput2 = relativeProductBuildDir("user-outside-project")
            + "/tool-output.txt";
    QVERIFY2(QFile::exists(toolOutput2), qPrintable(toolOutput2));
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

void TestApi::transformerData()
{
    const qbs::SetupProjectParameters params
            = defaultSetupParameters("transformer-data/transformer-data.qbs");
    const std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(params,
                                                                                    m_logSink, 0));
    QVERIFY(waitForFinished(setupJob.get()));
    QVERIFY2(!setupJob->error().hasError(), qPrintable(setupJob->error().toString()));
    const qbs::Project project = setupJob->project();
    const std::unique_ptr<qbs::BuildJob> buildJob(project.buildAllProducts(qbs::BuildOptions()));
    QVERIFY(waitForFinished(buildJob.get()));
    QVERIFY2(!buildJob->error().hasError(), qPrintable(buildJob->error().toString()));
    qbs::ErrorInfo error;
    const qbs::ProjectTransformerData projectTData = project.transformerData(&error);
    QVERIFY2(!error.hasError(), qPrintable(error.toString()));
    QCOMPARE(projectTData.size(), 1);
    const qbs::ProductTransformerData productTData = projectTData.first().second;
    QCOMPARE(productTData.size(), 2);
    bool firstTransformerFound = false;
    bool secondTransformerFound = false;
    for (const qbs::TransformerData &tData : productTData) {
        if (tData.inputs().empty()) {
            firstTransformerFound = true;
            QCOMPARE(tData.outputs().size(), 1);
            QCOMPARE(QFileInfo(tData.outputs().first().filePath()).fileName(),
                     QString("artifact1"));
            QCOMPARE(tData.commands().size(), 1);
            QCOMPARE(tData.commands().first().type(), qbs::RuleCommand::JavaScriptCommandType);
        } else {
            secondTransformerFound = true;
            QCOMPARE(tData.inputs().size(), 1);
            QCOMPARE(QFileInfo(tData.inputs().first().filePath()).fileName(), QString("artifact1"));
            QCOMPARE(tData.outputs().size(), 1);
            QCOMPARE(QFileInfo(tData.outputs().first().filePath()).fileName(),
                     QString("artifact2"));
            QCOMPARE(tData.commands().size(), 1);
            QCOMPARE(tData.commands().first().type(), qbs::RuleCommand::JavaScriptCommandType);
        }
    }
    QVERIFY(firstTransformerFound);
    QVERIFY(secondTransformerFound);
}

void TestApi::transformers()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("transformers/transformers.qbs");
    VERIFY_NO_ERROR(errorInfo);
}

void TestApi::typeChange()
{
    BuildDescriptionReceiver receiver;
    qbs::ErrorInfo errorInfo = doBuildProject("type-change", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY2(!receiver.descriptions.contains("compiling"), qPrintable(receiver.descriptions));

    WAIT_FOR_NEW_TIMESTAMP();
    QFile projectFile("type-change.qbs");
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    QByteArray content = projectFile.readAll();
    content.replace("//", "");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    errorInfo = doBuildProject("type-change", &receiver);
    VERIFY_NO_ERROR(errorInfo);
    QVERIFY2(receiver.descriptions.contains("compiling"), qPrintable(receiver.descriptions));
}

void TestApi::uic()
{
    const qbs::ErrorInfo errorInfo = doBuildProject("uic");
    VERIFY_NO_ERROR(errorInfo);
}


qbs::ErrorInfo TestApi::doBuildProject(
    const QString &projectFilePath, BuildDescriptionReceiver *buildDescriptionReceiver,
    ProcessResultReceiver *procResultReceiver, TaskReceiver *taskReceiver,
    const qbs::BuildOptions &options, const QVariantMap &overriddenValues)
{
    qbs::SetupProjectParameters params = defaultSetupParameters(projectFilePath);
    params.setOverriddenValues(overriddenValues);
    params.setDryRun(options.dryRun());
    const std::unique_ptr<qbs::SetupProjectJob> setupJob(qbs::Project().setupProject(params,
                                                                                    m_logSink, 0));
    if (taskReceiver) {
        connect(setupJob.get(), &qbs::AbstractJob::taskStarted,
                taskReceiver, &TaskReceiver::handleTaskStart);
    }
    waitForFinished(setupJob.get());
    if (setupJob->error().hasError())
        return setupJob->error();
    const std::unique_ptr<qbs::BuildJob> buildJob(setupJob->project().buildAllProducts(options));
    if (buildDescriptionReceiver) {
        connect(buildJob.get(), &qbs::BuildJob::reportCommandDescription,
                buildDescriptionReceiver, &BuildDescriptionReceiver::handleDescription);
    }
    if (procResultReceiver) {
        connect(buildJob.get(), &qbs::BuildJob::reportProcessResult,
                procResultReceiver, &ProcessResultReceiver::handleProcessResult);
    }
    waitForFinished(buildJob.get());
    return buildJob->error();
}

QTEST_MAIN(TestApi)

#include "tst_api.moc"
