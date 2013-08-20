/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#include "../../src/app/shared/qbssettings.h"

#include <api/jobs.h>
#include <api/project.h>
#include <api/projectdata.h>
#include <logging/ilogsink.h>
#include <tools/hostosinfo.h>
#include <tools/installoptions.h>
#include <tools/preferences.h>
#include <tools/setupprojectparameters.h>

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QScopedPointer>
#include <QTemporaryDir>
#include <QTest>

class LogSink: public qbs::ILogSink
{
    void doPrintWarning(const qbs::ErrorInfo &error) {
        qDebug("%s", qPrintable(error.toString()));
    }
    void doPrintMessage(qbs::LoggerLevel, const QString &, const QString &) { }
};

TestApi::TestApi() : m_logSink(new LogSink)
{
}

TestApi::~TestApi()
{
    delete m_logSink;
}

void TestApi::installableFiles()
{
    qbs::SetupProjectParameters setupParams = defaultSetupParameters();
    setupParams.setProjectFilePath(QDir::cleanPath(QLatin1String(SRCDIR "/../blackbox/testdata"
        "/installed_artifact/installed_artifact.qbs")));
    QScopedPointer<qbs::SetupProjectJob> job(qbs::Project::setupProject(setupParams,
                                                                        m_logSink, 0));
    QEventLoop loop;
    connect(job.data(), SIGNAL(finished(bool,qbs::AbstractJob*)), &loop, SLOT(quit()));
    loop.exec();
    QVERIFY2(!job->error().hasError(), qPrintable(job->error().toString()));
    qbs::Project project = job->project();
    qbs::ProjectData projectData = project.projectData();
    QCOMPARE(projectData.allProducts().count(), 1);
    qbs::ProductData product = projectData.allProducts().first();
    qbs::InstallOptions installOptions;
    installOptions.setInstallRoot(QLatin1String("/tmp"));
    QList<qbs::InstallableFile> installableFiles
            = project.installableFilesForProduct(product, installOptions);
    QCOMPARE(installableFiles.count(), 1);
    qbs::InstallableFile const application = installableFiles.first();
    QVERIFY(application.isExecutable());
    QString expectedTargetFilePath
            = qbs::Internal::HostOsInfo::appendExecutableSuffix(QLatin1String("/tmp/usr/bin/installedApp"));
    QCOMPARE(application.targetFilePath(), expectedTargetFilePath);

    setupParams.setProjectFilePath(QDir::cleanPath(QLatin1String(SRCDIR "/../blackbox/testdata"
        "/recursive_wildcards/recursive_wildcards.qbs")));
    job.reset(qbs::Project::setupProject(setupParams, m_logSink, 0));
    connect(job.data(), SIGNAL(finished(bool,qbs::AbstractJob*)), &loop, SLOT(quit()));
    loop.exec();
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

qbs::SetupProjectParameters TestApi::defaultSetupParameters() const
{
    qbs::SetupProjectParameters setupParams;
    setupParams.setDryRun(true); // So no build graph gets created.
    setupParams.setBuildRoot(QLatin1String("/blubb")); // Must be set and be absolute.
    setupParams.setRestoreBehavior(qbs::SetupProjectParameters::ResolveOnly); // No restoring.

    const QString qbsRootPath = QDir::cleanPath(QCoreApplication::applicationDirPath()
                                                + QLatin1String("/../"));
    SettingsPtr settings = qbsSettings();
    setupParams.setSearchPaths(qbs::Preferences(settings.data()).searchPaths(qbsRootPath));
    setupParams.setPluginPaths(qbs::Preferences(settings.data()).pluginPaths(qbsRootPath));
    QVariantMap buildConfig;
    buildConfig.insert(QLatin1String("qbs.profile"), QLatin1String("qbs_autotests"));
    buildConfig.insert(QLatin1String("qbs.buildVariant"), QLatin1String("debug"));
    setupParams.setBuildConfiguration(buildConfig);
    setupParams.expandBuildConfiguration(settings.data());
    return setupParams;
}

QTEST_MAIN(TestApi)
