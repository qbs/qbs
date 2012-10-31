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
#include <language/sourceproject.h>
#include <logging/consolelogger.h>
#include <logging/logger.h>
#include <tools/hostosinfo.h>
#include <tools/runenvironment.h>

#include <QCoreApplication>
#include <QDir>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>

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

static int runShell(SourceProject &sourceProject, const ResolvedProject::ConstPtr &project)
{
    try {
        RunEnvironment runEnvironment = sourceProject.getRunEnvironment(project->products.first(),
                QProcessEnvironment::systemEnvironment());
        return runEnvironment.runShell();
    } catch (const Error &error) {
        qbsError() << error.toString();
        return EXIT_FAILURE;
    }
}

class BuildRunner : public QObject
{
    Q_OBJECT
public:
    BuildRunner(SourceProject &sourceProject, const QList<ResolvedProject::ConstPtr> &projects,
                const BuildOptions &buildOptions)
        : m_sourceProject(sourceProject), m_projects(projects), m_buildOptions(buildOptions)
    {
    }

private slots:
    void build()
    {
        try {
            m_sourceProject.buildProjects(m_projects, m_buildOptions);
            qApp->quit();
        } catch (const Error &error) {
            qbsError() << error.toString();
            qApp->exit(ExitCodeErrorBuildFailure);
        }
    }

private:
    SourceProject &m_sourceProject;
    const QList<ResolvedProject::ConstPtr> m_projects;
    const BuildOptions m_buildOptions;
};

static int buildProjects(Application &app, SourceProject &sourceProject,
        const QList<ResolvedProject::ConstPtr> &projects, const BuildOptions &buildOptions)
{
    BuildRunner buildRunner(sourceProject, projects, buildOptions);
    QTimer::singleShot(0, &buildRunner, SLOT(build()));
    return app.exec();
}

static int runTarget(SourceProject &sourceProject, const QList<ResolvedProject::ConstPtr> &projects,
                     const QString &targetName, const QStringList &arguments)
{
    try {
        ResolvedProduct::ConstPtr productToRun;
        QString productFileName;

        foreach (const ResolvedProject::ConstPtr &project, projects) {
            foreach (const ResolvedProduct::ConstPtr &product, project->products) {
                const QString executable = sourceProject.targetExecutable(product);
                if (executable.isEmpty())
                    continue;
                if (!targetName.isEmpty() && !executable.endsWith(targetName))
                    continue;
                if (!productFileName.isEmpty()) {
                    qbsError() << QObject::tr("There is more than one executable target in "
                                              "the project. Please specify which target "
                                              "you want to run.");
                    return EXIT_FAILURE;
                }
                productFileName = executable;
                productToRun = product;
            }
        }

        if (!productToRun) {
            if (targetName.isEmpty())
                qbsError() << QObject::tr("Can't find a suitable product to run.");
            else
                qbsError() << QObject::tr("No such target: '%1'").arg(targetName);
            return ExitCodeErrorBuildFailure;
        }

        RunEnvironment runEnvironment = sourceProject.getRunEnvironment(productToRun,
                                                                        QProcessEnvironment::systemEnvironment());
        return runEnvironment.runTarget(productFileName, arguments);
    } catch (const Error &error) {
        qbsError() << error.toString();
        return EXIT_FAILURE;
    }
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

    QList<ResolvedProject::ConstPtr> resolvedProjects;
    SourceProject sourceProject;
    sourceProject.setBuildRoot(QDir::currentPath());
    try {
        foreach (const QVariantMap &buildConfig, parser.buildConfigurations()) {
            const ResolvedProject::ConstPtr resolvedProject
                    = sourceProject.setupResolvedProject(parser.projectFileName(), buildConfig);
            resolvedProjects << resolvedProject;
        }
    } catch (const Error &error) {
        qbsError() << error.toString();
        return ExitCodeErrorLoadingProjectFailed;
    }

    switch (parser.command()) {
    case CommandLineParser::CleanCommand:
        return makeClean();
    case CommandLineParser::StartShellCommand:
        return runShell(sourceProject, resolvedProjects.first());
    case CommandLineParser::StatusCommand:
        return printStatus(parser.projectFileName(), resolvedProjects);
    case CommandLineParser::PropertiesCommand:
        return showProperties(resolvedProjects, parser.buildOptions());
    case CommandLineParser::BuildCommand:
        return buildProjects(app, sourceProject, resolvedProjects, parser.buildOptions());
    case CommandLineParser::RunCommand: {
        const int buildExitCode = buildProjects(app, sourceProject, resolvedProjects,
                                                parser.buildOptions());
        if (buildExitCode != 0)
            return buildExitCode;
        return runTarget(sourceProject, resolvedProjects, parser.runTargetName(), parser.runArgs());
    }
    }
}

#include "main.moc"
