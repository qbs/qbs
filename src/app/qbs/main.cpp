/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#include "application.h"
#include "status.h"

#include <Qbs/sourceproject.h>
#include <Qbs/runenvironment.h>
#include <tools/hostosinfo.h>
#include <tools/logger.h>
#include <tools/options.h>
#include <buildgraph/buildgraph.h>
#include <buildgraph/executor.h>
#include <tools/fileinfo.h>
#include <tools/persistence.h>
#include <tools/logsink.h>

#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QScopedPointer>

#if defined(Q_OS_UNIX)
#include <errno.h>
#endif

enum ExitCode
{
    ExitCodeOK = 0,
    ExitCodeErrorParsingCommandLine = 1,
    ExitCodeErrorCommandNotImplemented = 2,
    ExitCodeErrorExecutionFailed = 3,
    ExitCodeErrorLoadingProjectFailed = 4,
    ExitCodeErrorBuildFailure = 5
};

int main(int argc, char *argv[])
{
    qbs::Application app(argc, argv);
    app.init();

    qbs::ConsoleLogger cl;

    QStringList arguments = app.arguments();
    arguments.removeFirst();

    if (!arguments.isEmpty()) {
        qputenv("PATH", QCoreApplication::applicationDirPath().toLocal8Bit()
            + qbs::HostOsInfo::pathListSeparator().toLatin1() + QByteArray(qgetenv("PATH")));
        QStringList subProcessArgs = arguments;
        const QString subProcess = subProcessArgs.takeFirst();
        if (!subProcess.startsWith('-')) {
            const int exitCode = QProcess::execute("qbs-" + subProcess, subProcessArgs);
            if (exitCode != -2)
                return exitCode;
        }
    }

    // read commandline
    qbs::CommandLineOptions options;
    if (!options.parseCommandLine(arguments)) {
        options.printHelp();
        return ExitCodeErrorParsingCommandLine;
    }

    if (options.isHelpSet()) {
        options.printHelp();
        return 0;
    }

    if (options.projectFileName().isEmpty()) {
        qbs::qbsError("No project file found.");
        return ExitCodeErrorParsingCommandLine;
    } else {
        qbs::qbsInfo() << qbs::DontPrintLogLevel << "Found project file "
            << qPrintable(QDir::toNativeSeparators(options.projectFileName()));
    }

    if (options.command() == qbs::CommandLineOptions::CleanCommand) {
        // ### TODO: take selected products into account!
        QString errorMessage;

        const QString buildPath = qbs::FileInfo::resolvePath(QDir::currentPath(),
                QLatin1String("build"));
        if (!qbs::removeDirectoryWithContents(buildPath, &errorMessage)) {
            qbs::qbsError() << errorMessage;
            return ExitCodeErrorExecutionFailed;
        }
        return 0;
    }

    // some sanity checks
    foreach (const QString &searchPath, options.searchPaths()) {
        if (!qbs::FileInfo::exists(searchPath)) {
            qbs::qbsError("search path '%s' does not exist.\n"
                     "run 'qbs config --global preferences.qbsPath $QBS_SOURCE_TREE/share/qbs'",
                     qPrintable(searchPath));

            return ExitCodeErrorParsingCommandLine;
        }
    }
    foreach (const QString &pluginPath, options.pluginPaths()) {
        if (!qbs::FileInfo::exists(pluginPath)) {
            qbs::qbsError("plugin path '%s' does not exist.\n"
                     "run 'qbs config --global preferences.pluginsPath $QBS_BUILD_TREE/plugins'",
                     qPrintable(pluginPath));
            return ExitCodeErrorParsingCommandLine;
        }
    }

    Qbs::SourceProject sourceProject;
    try {
        sourceProject.setSettings(options.settings());
        sourceProject.setSearchPaths(options.searchPaths());
        sourceProject.loadPlugins(options.pluginPaths());
        sourceProject.loadProject(options.projectFileName(), options.buildConfigurations());
    } catch (const qbs::Error &error) {
        qbs::qbsError() << error.toString();
        return ExitCodeErrorLoadingProjectFailed;
    }

    if (options.command() == qbs::CommandLineOptions::StartShellCommand) {
        Qbs::BuildProject buildProject = sourceProject.buildProjects().first();
        Qbs::BuildProduct buildProduct = buildProject.buildProducts().first();
        Qbs::RunEnvironment run(buildProduct);
        return run.runShell();
    }
    if (options.isDumpGraphSet()) {
        foreach (Qbs::BuildProject buildProject, sourceProject.buildProjects())
            foreach (Qbs::BuildProduct buildProduct, buildProject.buildProducts())
                buildProduct.dump();
        return 0;
    }

    if (options.command() == qbs::CommandLineOptions::StatusCommand)
        return qbs::printStatus(options, sourceProject);

    if (options.command() == qbs::CommandLineOptions::PropertiesCommand) {
        const bool showAll = (options.selectedProductNames().count() == 0);
        foreach (const Qbs::BuildProject& buildProject, sourceProject.buildProjects()) {
            foreach (const Qbs::BuildProduct& buildProduct, buildProject.buildProducts()) {
                if (showAll || options.selectedProductNames().contains(buildProduct.name()))
                    buildProduct.dumpProperties();
            }
        }
        return 0;
    }

    // execute the build graph
    int exitCode = 0;
    qbs::Executor::BuildResult buildResult = qbs::Executor::SuccessfulBuild;
    {
        QScopedPointer<qbs::Executor> executor(new qbs::Executor());
        app.setExecutor(executor.data());
        QObject::connect(executor.data(), SIGNAL(finished()), &app, SLOT(quit()), Qt::QueuedConnection);
        QObject::connect(executor.data(), SIGNAL(error()), &app, SLOT(quit()), Qt::QueuedConnection);
        executor->setMaximumJobs(options.jobs());
        executor->setRunOnceAndForgetModeEnabled(true);
        executor->setKeepGoing(options.isKeepGoingSet());
        executor->setDryRun(options.isDryRunSet());

        QDir currentDir;
        QStringList absoluteNamesChangedFiles;
        foreach (const QString &fileName, options.changedFiles())
            absoluteNamesChangedFiles += QDir::fromNativeSeparators(currentDir.absoluteFilePath(fileName));

        executor->build(sourceProject.internalBuildProjects(), absoluteNamesChangedFiles, options.selectedProductNames());
        exitCode = app.exec();
        app.setExecutor(0);
        buildResult = executor->buildResult();
        if (executor->state() == qbs::Executor::ExecutorError)
            return ExitCodeErrorExecutionFailed;
        if (buildResult != qbs::Executor::SuccessfulBuild)
            exitCode = ExitCodeErrorBuildFailure;

        // store the projects on disk
        try {
            foreach (Qbs::BuildProject buildProject, sourceProject.buildProjects())
                buildProject.storeBuildGraph();
        } catch (const qbs::Error &e) {
            qbs::qbsError() << e.toString();
            return ExitCodeErrorExecutionFailed;
        }
    }

    if (options.command() == qbs::CommandLineOptions::RunCommand
            && buildResult == qbs::Executor::SuccessfulBuild) {
        Qbs::BuildProject buildProject = sourceProject.buildProjects().first();
        Qbs::BuildProduct productToRun;
        QString productFileName;

        foreach (const Qbs::BuildProduct &buildProduct, buildProject.buildProducts()) {
            if (!options.runTargetName().isEmpty() && options.runTargetName() != buildProduct.targetName())
                continue;

            if (options.runTargetName().isEmpty() || options.runTargetName() == buildProduct.targetName()) {
                if (buildProduct.isExecutable()) {
                    productToRun = buildProduct;
                    productFileName = buildProduct.executablePath();
                    break;
                }
            }
        }

        if (!productToRun.isValid()) {
            if (options.runTargetName().isEmpty())
                qbs::qbsError() << QObject::tr("Can't find a suitable product to run.");
            else
                qbs::qbsError() << QObject::tr("No such product: '%1'").arg(options.runTargetName());
            return ExitCodeErrorBuildFailure;
        }

        Qbs::RunEnvironment run(productToRun);
        return run.runTarget(productFileName, options.runArgs());
    }

    return exitCode;
}
