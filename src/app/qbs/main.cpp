/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#include "application.h"
#include <Qbs/oldsourceproject.h>
#include <Qbs/mainthreadcommunication.h>
#include <tools/logger.h>
#include <tools/options.h>
#include <buildgraph/buildgraph.h>
#include <buildgraph/executor.h>
#include <tools/runenvironment.h>
#include <tools/fakeconcurrent.h>
#include <tools/fileinfo.h>
#include <tools/persistence.h>
#include <tools/logsink.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>
#include <QtCore/QScopedPointer>
#include <QtCore/QDebug>
#include <QtCore/QDir>

#if defined(Q_OS_UNIX)
#include <errno.h>
#endif

enum ExitCodes
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
    Application app(argc, argv);
    app.init();

    qbs::CommandLineOptions options;
    qbs::ConsolePrintLogSink *logSink = new qbs::ConsolePrintLogSink;
    logSink->setColoredOutputEnabled(options.configurationValue("defaults/useColoredOutput", true).toBool());
    Qbs::ILogSink::setGlobalLogSink(logSink);
    Qbs::MainThreadCommunication::registerMetaType();
    QStringList arguments = app.arguments();
    arguments.removeFirst();

    if (arguments.count()) {
#if defined(Q_OS_UNIX)
        qputenv("PATH", QCoreApplication::applicationDirPath().toLocal8Bit() + ':' + QByteArray(qgetenv("PATH")));
#elif defined(Q_OS_WIN)
        qputenv("PATH", QCoreApplication::applicationDirPath().toLocal8Bit() + ';' + QByteArray(qgetenv("PATH")));
#endif
        QStringList args = app.arguments();
        args.takeFirst();
        QString app = args.takeFirst();
        if (!app.startsWith('-')) {
#if defined(Q_OS_UNIX)
            char **argvp = new char*[args.count() + 2];
            QList<QByteArray> bargs;
            bargs.append("qbs-" + app.toLocal8Bit());
            argvp[0] = bargs.last().data();
            int i = 1;
            foreach (const QString &s, args) {
                bargs.append(s.toLocal8Bit());
                argvp[i++] = bargs.last().data();
            }
            argvp[i] = 0;

            execvp(argvp[0], argvp);
            if (errno != ENOENT) {
                perror("execvp");
                return errno;
            }
#else
            int r = QProcess::execute ( "qbs-" + app, args );
            if (r != -2)
                return r;
#endif
        }
    }

    // read commandline
    if (!options.readCommandLineArguments(arguments)) {
        qbs::CommandLineOptions::printHelp();
        return ExitCodeErrorParsingCommandLine;
    }

    if (options.isHelpSet()) {
        qbs::CommandLineOptions::printHelp();
        return 0;
    }

    try {
        if (options.command() == qbs::CommandLineOptions::ConfigCommand) {
            options.configure();
            return 0;
        }
    } catch (qbs::Error &e) {
        fputs("qbs config: ", stderr);
        fputs(qPrintable(e.toString()), stderr);
        fputs("\n", stderr);
        return ExitCodeErrorParsingCommandLine;
    }

    if (options.projectFileName().isEmpty()) {
        qbsError("No project file found.");
        return ExitCodeErrorParsingCommandLine;
    } else {
        qbsInfo() << qbs::DontPrintLogLevel << "Found project file " << qPrintable(QDir::toNativeSeparators(options.projectFileName()));
    }

    if (options.command() == qbs::CommandLineOptions::CleanCommand) {
        // ### TODO: take selected products into account!
        QString errorMessage;

        const QString buildPath = qbs::FileInfo::resolvePath(QDir::currentPath(), QLatin1String("build"));
        qbs::removeDirectoryWithContents(buildPath, &errorMessage);

        if (!errorMessage.isEmpty()) {
            qbsError() << errorMessage;
            return ExitCodeErrorExecutionFailed;
        }
        return 0;
    }

    // some sanity checks
    foreach (const QString &searchPath, options.searchPaths()) {
        if (!qbs::FileInfo::exists(searchPath)) {
            qbsError("search path '%s' does not exist.\n"
                     "run 'qbs config --global paths/cubes $QBS_SOURCE_TREE/share/qbs'",
                     qPrintable(searchPath));

            return ExitCodeErrorParsingCommandLine;
        }
    }
    foreach (const QString &pluginPath, options.pluginPaths()) {
        if (!qbs::FileInfo::exists(pluginPath)) {
            qbsError("plugin path '%s' does not exist.\n"
                     "run 'qbs config --global paths/plugins $QBS_BUILD_TREE/plugins'",
                     qPrintable(pluginPath));
            return ExitCodeErrorParsingCommandLine;
        }
    }

    qbs::SourceProject sourceProject(options.settings());
    sourceProject.setSearchPaths(options.searchPaths());
    sourceProject.loadPlugins(options.pluginPaths());
    QFuture<bool> loadProjectFuture = qbs::FakeConcurrent::run(&qbs::SourceProject::loadProject,
                                                               &sourceProject,
                                                               options.projectFileName(),
                                                               options.buildConfigurations());
    loadProjectFuture.waitForFinished();
    foreach (const qbs::Error &error, sourceProject.errors()) {
        qbsError() << error.toString();
        return ExitCodeErrorLoadingProjectFailed;
    }

    if (options.command() == qbs::CommandLineOptions::StartShellCommand) {
        qbs::BuildProject::Ptr buildProject = sourceProject.buildProjects().first();
        qbs::BuildProduct::Ptr buildProduct = *buildProject->buildProducts().begin();
        qbs::RunEnvironment run(buildProduct->rProduct);
        return run.runShell();
    }
    if (options.isDumpGraphSet()) {
        foreach (qbs::BuildProject::Ptr buildPrj, sourceProject.buildProjects())
            foreach (qbs::BuildProduct::Ptr buildProduct, buildPrj->buildProducts())
                qbs::BuildGraph().dump(buildProduct);
        return 0;
    }

    // execute the build graph
    Qbs::BuildExecutor *buildExecutor = app.buildExecutor();
    buildExecutor->setMaximumJobs(options.jobs());
    buildExecutor->setRunOnceAndForgetModeEnabled(true);
    buildExecutor->setKeepGoingEnabled(options.isKeepGoingSet());
    buildExecutor->setDryRunEnabled(options.isDryRunSet());

    QDir currentDir;
    QStringList absoluteNamesChangedFiles;
    foreach (const QString &fileName, options.changedFiles())
        absoluteNamesChangedFiles += QDir::fromNativeSeparators(currentDir.absoluteFilePath(fileName));

    int result = 0;
    QFuture<bool> buildProjectFuture = qbs::FakeConcurrent::run(&Qbs::BuildExecutor::executeBuildProjects, buildExecutor,
                                                         sourceProject.buildProjects(), absoluteNamesChangedFiles, options.selectedProductNames());
    app.buildProjectFutureWatcher()->setFuture(buildProjectFuture);
    result = app.exec();
    if (buildExecutor->state() == Qbs::BuildExecutor::ExecutorError)
        return ExitCodeErrorExecutionFailed;

    // store the projects on disk
    try {
        foreach (qbs::BuildProject::Ptr project, sourceProject.buildProjects())
            project->store();
    } catch (qbs::Error &e) {
        qbsError() << e.toString();
        return ExitCodeErrorExecutionFailed;
    }

    if (options.command() == qbs::CommandLineOptions::RunCommand) {
        qbs::BuildProject::Ptr project = sourceProject.buildProjects().first();
        qbs::BuildProduct::Ptr productToRun;
        QString productFileName;
        QSet<QString> runnableFileTags;
#ifdef Q_OS_MAC
        runnableFileTags << "applicationbundle";
#else
        runnableFileTags << "application";
#endif
#ifdef Q_OS_MAC
        bool openProduct = false;
#endif // Q_OS_MAC
        if (options.runTargetName().isEmpty()) {
            foreach (qbs::BuildProduct::Ptr product, project->buildProducts()) {
                foreach (qbs::Artifact *targetArtifact, product->targetArtifacts) {
                    if (!targetArtifact->fileTags.intersect(runnableFileTags).isEmpty()) {
                        productToRun = product;
                        productFileName = targetArtifact->fileName;
#ifdef Q_OS_MAC
                        if (targetArtifact->fileTags.contains("applicationbundle"))
                            openProduct = true;
#endif // Q_OS_MAC
                        break;
                    }
                }
                if (productToRun)
                    break;
            }
        } else {
            foreach (qbs::BuildProduct::Ptr product, project->buildProducts()) {
                if (product->rProduct->name == options.runTargetName()) {
                    productToRun = product;
                    foreach (qbs::Artifact *targetArtifact, product->targetArtifacts) {
                        if (!targetArtifact->fileTags.intersect(runnableFileTags).isEmpty()) {
                            productToRun = product;
                            productFileName = targetArtifact->fileName;
                            break;
                        }
                    }
                    break;
                }
            }
        }

        if (!productToRun) {
            if (options.runTargetName().isEmpty())
                qbsError() << QObject::tr("Can't find a suitable product to run.");
            else
                qbsError() << QObject::tr("No such product: '%1'").arg(options.runTargetName());
            return ExitCodeErrorBuildFailure;
        }

        qbs::RunEnvironment run(productToRun->rProduct);
#ifdef Q_OS_MAC
        if (openProduct) {
            QStringList runArgs = options.runArgs();
            QString appBundleName = productFileName; // TODO: make appBundleName be the app bundle dir
            runArgs.prepend(appBundleName);
            return run.runTarget("/usr/bin/open", runArgs);
        }
#endif // Q_OS_MAC
        return run.runTarget(productFileName, options.runArgs());
    }

    return result;
}
