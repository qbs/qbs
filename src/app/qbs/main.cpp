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
#include "showproperties.h"
#include "status.h"

#include <app/shared/commandlineparser.h>
#include <buildgraph/buildgraph.h>
#include <buildgraph/executor.h>
#include <language/qbsengine.h>
#include <language/sourceproject.h>
#include <logging/consolelogger.h>
#include <logging/logger.h>
#include <tools/hostosinfo.h>
#include <tools/runenvironment.h>

#include <QCoreApplication>
#include <QDir>
#include <QProcess>

using namespace qbs;

enum ExitCode
{
    ExitCodeOK = 0,
    ExitCodeErrorParsingCommandLine = 1,
    ExitCodeErrorCommandNotImplemented = 2,
    ExitCodeErrorExecutionFailed = 3,
    ExitCodeErrorLoadingProjectFailed = 4,
    ExitCodeErrorBuildFailure = 5
};

static bool tryToRunTool(const QStringList &arguments, int &exitCode)
{
    if (arguments.isEmpty())
        return false;
    qputenv("PATH", QCoreApplication::applicationDirPath().toLocal8Bit()
            + HostOsInfo::pathListSeparator().toLatin1() + QByteArray(qgetenv("PATH")));
    QStringList subProcessArgs = arguments;
    const QString subProcess = subProcessArgs.takeFirst();
    if (subProcess.startsWith(QLatin1Char('-')))
        return false;
    exitCode = QProcess::execute(QLatin1String("qbs-") + subProcess, subProcessArgs);
    return exitCode != -2;
}

static int makeClean()
{
    // ### TODO: take selected products into account!
    QString errorMessage;
    const QString buildPath = FileInfo::resolvePath(QDir::currentPath(),
            QLatin1String("build"));
    if (!removeDirectoryWithContents(buildPath, &errorMessage)) {
        qbsError() << errorMessage;
        return ExitCodeErrorExecutionFailed;
    }
    return 0;
}

static int runShell(const SourceProject &sourceProject)
{
    BuildProject::Ptr buildProject = sourceProject.buildProjects().first();
    BuildProduct::Ptr buildProduct = *buildProject->buildProducts().begin();
    RunEnvironment run(sourceProject.engine(), buildProduct->rProduct);
    return run.runShell();
}

static int buildProject(Application &app, const SourceProject sourceProject,
        const BuildOptions &buildOptions)
{
    Executor executor;
    app.setExecutor(&executor);
    QObject::connect(&executor, SIGNAL(finished()), &app, SLOT(quit()), Qt::QueuedConnection);
    QObject::connect(&executor, SIGNAL(error()), &app, SLOT(quit()), Qt::QueuedConnection);
    executor.setEngine(sourceProject.engine());
    executor.setBuildOptions(buildOptions);
    TimedActivityLogger buildLogger(QLatin1String("Building project"), QString(),
            LoggerInfo);
    executor.build(sourceProject.buildProjects());
    app.exec();
    buildLogger.finishActivity();
    app.setExecutor(0);
    if (executor.state() == Executor::ExecutorError)
        return ExitCodeErrorExecutionFailed;

    // store the projects on disk
    try {
        foreach (const BuildProject::ConstPtr &buildProject, sourceProject.buildProjects())
            buildProject->store();
    } catch (const Error &e) {
        qbsError() << e.toString();
        return ExitCodeErrorExecutionFailed;
    }

    return executor.buildResult() == Executor::SuccessfulBuild ? 0 : ExitCodeErrorBuildFailure;
}

static Artifact *findTarget(const BuildProduct::ConstPtr &buildProduct,
                                 const QString &name)
{
    foreach (Artifact *artifact, buildProduct->targetArtifacts)
        if (artifact->filePath().endsWith(name))
            return artifact;
    return 0;
}

static int runTarget(const SourceProject &sourceProject, const QString &targetName,
        const QStringList &arguments)
{
    BuildProject::Ptr buildProject = sourceProject.buildProjects().first();
    BuildProduct::Ptr productToRun;
    QString productFileName;

    if (targetName.isEmpty()) {
        Artifact *executable = 0;
        foreach (const BuildProduct::Ptr &buildProduct, buildProject->buildProducts()) {
            foreach (Artifact *artifact, buildProduct->targetArtifacts) {
                if (artifact->fileTags.contains("application")) {
                    if (executable) {
                        qbsError() << QObject::tr("There's more than one executable target in "
                                                       "the project. Please specify which target "
                                                       "you want to run.");
                        return EXIT_FAILURE;
                    }
                    executable = artifact;
                }
            }
        }
        BuildProduct::Ptr buildProduct = *buildProject->buildProducts().begin();
        if (!executable) {
            qbsError() << QObject::tr("Cannot find executable target in product '%1'.")
                               .arg(productToRun->rProduct->name);
            return EXIT_FAILURE;
        }
        productToRun = buildProduct;
        productFileName = executable->filePath();
    } else {
        Artifact *executable = 0;
        foreach (const BuildProduct::Ptr &buildProduct, buildProject->buildProducts()) {
            executable = findTarget(buildProduct, targetName);
            if (!executable)
                continue;
            if (!executable->fileTags.contains("application")) {
                qbsError() << QObject::tr("%1 is not an executable target.").arg(executable->filePath());
                return EXIT_FAILURE;
            }
            productToRun = buildProduct;
            productFileName = executable->filePath();
        }
    }

    if (!productToRun) {
        if (targetName.isEmpty())
            qbsError() << QObject::tr("Can't find a suitable product to run.");
        else
            qbsError() << QObject::tr("No such target: '%1'").arg(targetName);
        return ExitCodeErrorBuildFailure;
    }

    RunEnvironment run(sourceProject.engine(), productToRun->rProduct);
    return run.runTarget(productFileName, arguments);
}


int main(int argc, char *argv[])
{
    ConsoleLogger cl;

    Application app(argc, argv);
    app.init();
    QStringList arguments = app.arguments();
    arguments.removeFirst();

    int toolExitCode;
    if (tryToRunTool(arguments, toolExitCode))
        return toolExitCode;

    CommandLineParser parser;
    if (!parser.parseCommandLine(arguments)) {
        parser.printHelp();
        return ExitCodeErrorParsingCommandLine;
    }

    if (parser.isHelpSet()) {
        parser.printHelp();
        return 0;
    }

    QbsEngine engine;
    SourceProject sourceProject(&engine);
    try {
        sourceProject.setSettings(parser.settings());
        sourceProject.loadPlugins();
        sourceProject.loadProject(parser.projectFileName(), parser.buildConfigurations());
    } catch (const Error &error) {
        qbsError() << error.toString();
        return ExitCodeErrorLoadingProjectFailed;
    }

    switch (parser.command()) {
    case CommandLineParser::CleanCommand:
        return makeClean();
    case CommandLineParser::StartShellCommand:
        return runShell(sourceProject);
    case CommandLineParser::StatusCommand:
        return printStatus(parser.projectFileName(), sourceProject);
    case CommandLineParser::PropertiesCommand:
        return showProperties(sourceProject, parser.buildOptions());
    case CommandLineParser::BuildCommand:
        return buildProject(app, sourceProject, parser.buildOptions());
    case CommandLineParser::RunCommand: {
        const int buildExitCode = buildProject(app, sourceProject, parser.buildOptions());
        if (buildExitCode != 0)
            return buildExitCode;
        return runTarget(sourceProject, parser.runTargetName(), parser.runArgs());
    }
    }
}
