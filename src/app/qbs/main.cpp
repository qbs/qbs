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

#include <app/shared/commandlineparser.h>
#include <buildgraph/buildgraph.h>
#include <buildgraph/executor.h>
#include <language/qbsengine.h>
#include <language/sourceproject.h>
#include <logging/consolelogger.h>
#include <logging/logger.h>
#include <tools/hostosinfo.h>
#include <tools/persistence.h>
#include <tools/runenvironment.h>

#include <QCoreApplication>
#include <QDir>
#include <QProcess>

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
            + qbs::HostOsInfo::pathListSeparator().toLatin1() + QByteArray(qgetenv("PATH")));
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
    const QString buildPath = qbs::FileInfo::resolvePath(QDir::currentPath(),
            QLatin1String("build"));
    if (!qbs::removeDirectoryWithContents(buildPath, &errorMessage)) {
        qbs::qbsError() << errorMessage;
        return ExitCodeErrorExecutionFailed;
    }
    return 0;
}

static int runShell(const qbs::SourceProject &sourceProject)
{
    Qbs::BuildProject buildProject = sourceProject.buildProjects().first();
    Qbs::BuildProduct buildProduct = buildProject.buildProducts().first();
    qbs::RunEnvironment run(buildProduct);
    return run.runShell();
}

static int showProperties(const qbs::SourceProject &sourceProject,
        const qbs::BuildOptions &buildOptions)
{
    const QStringList &selectedProducts = buildOptions.selectedProductNames;
    const bool showAll = selectedProducts.isEmpty();
    foreach (const Qbs::BuildProject& buildProject, sourceProject.buildProjects()) {
        foreach (const Qbs::BuildProduct& buildProduct, buildProject.buildProducts()) {
            if (showAll || selectedProducts.contains(buildProduct.name()))
                buildProduct.dumpProperties();
        }
    }
    return 0;
}

static int buildProject(qbs::Application &app, const qbs::SourceProject sourceProject,
        const qbs::BuildOptions &buildOptions)
{
    qbs::Executor executor;
    app.setExecutor(&executor);
    QObject::connect(&executor, SIGNAL(finished()), &app, SLOT(quit()), Qt::QueuedConnection);
    QObject::connect(&executor, SIGNAL(error()), &app, SLOT(quit()), Qt::QueuedConnection);
    executor.setEngine(sourceProject.engine());
    executor.setBuildOptions(buildOptions);
    executor.build(sourceProject.internalBuildProjects());
    app.exec();
    app.setExecutor(0);
    if (executor.state() == qbs::Executor::ExecutorError)
        return ExitCodeErrorExecutionFailed;

    // store the projects on disk
    try {
        foreach (const Qbs::BuildProject &buildProject, sourceProject.buildProjects())
            buildProject.storeBuildGraph();
    } catch (const qbs::Error &e) {
        qbs::qbsError() << e.toString();
        return ExitCodeErrorExecutionFailed;
    }

    return executor.buildResult() == qbs::Executor::SuccessfulBuild ? 0 : ExitCodeErrorBuildFailure;
}

static int runTarget(const qbs::SourceProject &sourceProject, const QString &targetName,
        const QStringList &arguments)
{
    Qbs::BuildProject buildProject = sourceProject.buildProjects().first();
    Qbs::BuildProduct productToRun;
    QString productFileName;

    foreach (const Qbs::BuildProduct &buildProduct, buildProject.buildProducts()) {
        if (!targetName.isEmpty() && targetName != buildProduct.targetName())
            continue;
        if (targetName.isEmpty() || targetName == buildProduct.targetName()) {
            if (buildProduct.isExecutable()) {
                productToRun = buildProduct;
                productFileName = buildProduct.executablePath();
                break;
            }
        }
    }

    if (!productToRun.isValid()) {
        if (targetName.isEmpty())
            qbs::qbsError() << QObject::tr("Can't find a suitable product to run.");
        else
            qbs::qbsError() << QObject::tr("No such product: '%1'").arg(targetName);
        return ExitCodeErrorBuildFailure;
    }

    qbs::RunEnvironment run(productToRun);
    return run.runTarget(productFileName, arguments);
}


int main(int argc, char *argv[])
{
    qbs::ConsoleLogger cl;

    qbs::Application app(argc, argv);
    app.init();
    QStringList arguments = app.arguments();
    arguments.removeFirst();

    int toolExitCode;
    if (tryToRunTool(arguments, toolExitCode))
        return toolExitCode;

    qbs::CommandLineParser parser;
    if (!parser.parseCommandLine(arguments)) {
        parser.printHelp();
        return ExitCodeErrorParsingCommandLine;
    }

    if (parser.isHelpSet()) {
        parser.printHelp();
        return 0;
    }

    qbs::QbsEngine engine;
    qbs::SourceProject sourceProject(&engine);
    try {
        sourceProject.setSettings(parser.settings());
        sourceProject.loadPlugins();
        sourceProject.loadProject(parser.projectFileName(), parser.buildConfigurations());
    } catch (const qbs::Error &error) {
        qbs::qbsError() << error.toString();
        return ExitCodeErrorLoadingProjectFailed;
    }

    switch (parser.command()) {
    case qbs::CommandLineParser::CleanCommand:
        return makeClean();
    case qbs::CommandLineParser::StartShellCommand:
        return runShell(sourceProject);
    case qbs::CommandLineParser::StatusCommand:
        return qbs::printStatus(parser.projectFileName(), sourceProject);
    case qbs::CommandLineParser::PropertiesCommand:
        return showProperties(sourceProject, parser.buildOptions());
    case qbs::CommandLineParser::BuildCommand:
        return buildProject(app, sourceProject, parser.buildOptions());
    case qbs::CommandLineParser::RunCommand: {
        const int buildExitCode = buildProject(app, sourceProject, parser.buildOptions());
        if (buildExitCode != 0)
            return buildExitCode;
        return runTarget(sourceProject, parser.runTargetName(), parser.runArgs());
    }
    }
}
