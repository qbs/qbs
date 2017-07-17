/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "commandlinefrontend.h"

#include "application.h"
#include "consoleprogressobserver.h"
#include "status.h"
#include "parser/commandlineoption.h"
#include "../shared/logging/consolelogger.h"

#include <qbs.h>
#include <api/runenvironment.h>
#include <logging/translator.h>
#include <tools/qbsassert.h>
#include <tools/projectgeneratormanager.h>
#include <tools/qttools.h>
#include <tools/shellutils.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qprocess.h>
#include <QtCore/qtimer.h>

#include <cstdlib>
#include <cstdio>

namespace qbs {
using namespace Internal;

CommandLineFrontend::CommandLineFrontend(const CommandLineParser &parser, Settings *settings,
                                         QObject *parent)
    : QObject(parent)
    , m_parser(parser)
    , m_settings(settings)
    , m_observer(0)
    , m_cancelStatus(CancelStatusNone)
    , m_cancelTimer(new QTimer(this))
{
}

CommandLineFrontend::~CommandLineFrontend()
{
    m_cancelTimer->stop();
}

// Called from interrupt handler. Don't do anything non-trivial here.
void CommandLineFrontend::cancel()
{
    m_cancelStatus = CancelStatusRequested;
}

void CommandLineFrontend::checkCancelStatus()
{
    switch (m_cancelStatus) {
    case CancelStatusNone:
        break;
    case CancelStatusRequested:
        m_cancelStatus = CancelStatusCanceling;
        m_cancelTimer->stop();
        if (m_resolveJobs.isEmpty() && m_buildJobs.isEmpty())
            std::exit(EXIT_FAILURE);
        foreach (AbstractJob * const job, m_resolveJobs)
            job->cancel();
        foreach (AbstractJob * const job, m_buildJobs)
            job->cancel();
        break;
    case CancelStatusCanceling:
        QBS_ASSERT(false, return);
        break;
    }
}

void CommandLineFrontend::start()
{
    try {
        switch (m_parser.command()) {
        case RunCommandType:
        case ShellCommandType:
            if (m_parser.products().count() > 1) {
                throw ErrorInfo(Tr::tr("Invalid use of command '%1': Cannot use more than one "
                                   "product.\nUsage: %2")
                            .arg(m_parser.commandName(), m_parser.commandDescription()));
            }
            Q_FALLTHROUGH();
        case StatusCommandType:
        case InstallCommandType:
        case DumpNodesTreeCommandType:
            if (m_parser.buildConfigurations().count() > 1) {
                QString error = Tr::tr("Invalid use of command '%1': There can be only one "
                               "build configuration.\n").arg(m_parser.commandName());
                error += Tr::tr("Usage: %1").arg(m_parser.commandDescription());
                throw ErrorInfo(error);
            }
            break;
        default:
            break;
        }

        if (m_parser.showVersion()) {
            puts(QBS_VERSION);
            qApp->exit(EXIT_SUCCESS);
            return;
        }
        if (m_parser.showProgress())
            m_observer = new ConsoleProgressObserver;
        SetupProjectParameters params;
        params.setEnvironment(QProcessEnvironment::systemEnvironment());
        params.setProjectFilePath(m_parser.projectFilePath());
        params.setDryRun(m_parser.dryRun());
        params.setForceProbeExecution(m_parser.forceProbesExecution());
        params.setWaitLockBuildGraph(m_parser.waitLockBuildGraph());
        params.setLogElapsedTime(m_parser.logTime());
        params.setSettingsDirectory(m_settings->baseDirectory());
        params.setOverrideBuildGraphData(m_parser.command() == ResolveCommandType);
        params.setPropertyCheckingMode(ErrorHandlingMode::Strict);
        if (!m_parser.buildBeforeInstalling() || m_parser.command() == DumpNodesTreeCommandType)
            params.setRestoreBehavior(SetupProjectParameters::RestoreOnly);
        foreach (const QVariantMap &buildConfig, m_parser.buildConfigurations()) {
            QVariantMap userConfig = buildConfig;
            const QString configurationKey = QLatin1String("qbs.configurationName");
            const QString profileKey = QLatin1String("qbs.profile");
            const QString installRootKey = QLatin1String("qbs.installRoot");
            QString installRoot = userConfig.value(installRootKey).toString();
            if (!installRoot.isEmpty() && QFileInfo(installRoot).isRelative()) {
                installRoot.prepend(QLatin1Char('/')).prepend(QDir::currentPath());
                userConfig.insert(installRootKey, installRoot);
            }
            const QString configurationName = userConfig.take(configurationKey).toString();
            const QString profileName = userConfig.take(profileKey).toString();
            const Preferences prefs(m_settings);
            params.setSearchPaths(prefs.searchPaths(QDir::cleanPath(QCoreApplication::applicationDirPath()
                    + QLatin1String("/" QBS_RELATIVE_SEARCH_PATH))));
            params.setPluginPaths(prefs.pluginPaths(QDir::cleanPath(QCoreApplication::applicationDirPath()
                    + QLatin1String("/" QBS_RELATIVE_PLUGINS_PATH))));
            params.setLibexecPath(QDir::cleanPath(QCoreApplication::applicationDirPath()
                    + QLatin1String("/" QBS_RELATIVE_LIBEXEC_PATH)));
            params.setTopLevelProfile(profileName);
            params.setConfigurationName(configurationName);
            params.setBuildRoot(buildDirectory(profileName));
            params.setOverriddenValues(userConfig);
            SetupProjectJob * const job = Project().setupProject(params,
                    ConsoleLogger::instance().logSink(), this);
            connectJob(job);
            m_resolveJobs << job;
        }

        /*
         * Progress reporting on the terminal gets a bit tricky when resolving several projects
         * concurrently, since we cannot show multiple progress bars at the same time. Instead,
         * we just set the total effort to the number of projects and increase the progress
         * every time one of them finishes, ignoring the progress reports from the jobs themselves.
         * (Yes, that does mean it will take disproportionately long for the first progress
         * notification to arrive.)
         */
        if (m_parser.showProgress() && resolvingMultipleProjects())
            m_observer->initialize(tr("Setting up projects"), m_resolveJobs.count());

        // Check every two seconds whether we received a cancel request. This value has been
        // experimentally found to be acceptable.
        // Note that this polling approach is not problematic here, since we are doing work anyway,
        // so there's no danger of waking up the processor for no reason.
        connect(m_cancelTimer, &QTimer::timeout, this, &CommandLineFrontend::checkCancelStatus);
        m_cancelTimer->start(2000);
    } catch (const ErrorInfo &error) {
        qbsError() << error.toString();
        if (m_buildJobs.isEmpty() && m_resolveJobs.isEmpty()) {
            qApp->exit(EXIT_FAILURE);
        } else {
            cancel();
            checkCancelStatus();
        }
    }
}

void CommandLineFrontend::handleCommandDescriptionReport(const QString &highlight,
                                                         const QString &message)
{
    qbsInfo() << MessageTag(highlight) << message;
}

void CommandLineFrontend::handleJobFinished(bool success, AbstractJob *job)
{
    try {
        job->deleteLater();
        if (!success) {
            qbsError() << job->error().toString();
            m_resolveJobs.removeOne(job);
            m_buildJobs.removeOne(job);
            if (m_resolveJobs.isEmpty() && m_buildJobs.isEmpty()) {
                qApp->exit(EXIT_FAILURE);
                return;
            }
            cancel();
        } else if (SetupProjectJob * const setupJob = qobject_cast<SetupProjectJob *>(job)) {
            m_resolveJobs.removeOne(job);
            m_projects << setupJob->project();
            if (m_observer && resolvingMultipleProjects())
                m_observer->incrementProgressValue();
            if (m_resolveJobs.isEmpty())
                handleProjectsResolved();
        } else if (qobject_cast<InstallJob *>(job)) {
            if (m_parser.command() == RunCommandType)
                qApp->exit(runTarget());
            else
                qApp->quit();
        } else { // Build or clean.
            m_buildJobs.removeOne(job);
            if (m_buildJobs.isEmpty()) {
                switch (m_parser.command()) {
                case RunCommandType:
                case InstallCommandType:
                    install();
                    break;
                case GenerateCommandType:
                    generate();
                    // fall through
                case BuildCommandType:
                case CleanCommandType:
                    qApp->quit();
                    break;
                default:
                    Q_ASSERT_X(false, Q_FUNC_INFO, "Missing case in switch statement");
                }
            }
        }
    } catch (const ErrorInfo &error) {
        qbsError() << error.toString();
        qApp->exit(EXIT_FAILURE);
    }
}

void CommandLineFrontend::handleNewTaskStarted(const QString &description, int totalEffort)
{
    // If the user does not want a progress bar, we just print the current activity.
    if (!m_parser.showProgress()) {
        if (!m_parser.logTime())
            qbsInfo() << description;
        return;
    }
    if (isBuilding()) {
        m_totalBuildEffort += totalEffort;
        if (++m_buildEffortsRetrieved == m_buildEffortsNeeded) {
            m_observer->initialize(tr("Building"), m_totalBuildEffort);
            if (m_currentBuildEffort > 0)
                m_observer->setProgressValue(m_currentBuildEffort);
        }
    } else if (!resolvingMultipleProjects()) {
        m_observer->initialize(description, totalEffort);
    }
}

void CommandLineFrontend::handleTotalEffortChanged(int totalEffort)
{
    // Can only happen when resolving.
    if (m_parser.showProgress() && !isBuilding() && !resolvingMultipleProjects())
        m_observer->setMaximum(totalEffort);
}

void CommandLineFrontend::handleTaskProgress(int value, AbstractJob *job)
{
    if (isBuilding()) {
        int &currentJobEffort = m_buildEfforts[job];
        m_currentBuildEffort += value - currentJobEffort;
        currentJobEffort = value;
        if (m_buildEffortsRetrieved == m_buildEffortsNeeded)
            m_observer->setProgressValue(m_currentBuildEffort);
    } else if (!resolvingMultipleProjects()) {
        m_observer->setProgressValue(value);
    }
}

void CommandLineFrontend::handleProcessResultReport(const qbs::ProcessResult &result)
{
    bool hasOutput = !result.stdOut().isEmpty() || !result.stdErr().isEmpty();
    if (!hasOutput && result.success())
        return;

    LogWriter w = result.success() ? qbsInfo() : qbsError();
    w << shellQuote(QDir::toNativeSeparators(result.executableFilePath()), result.arguments())
      << (hasOutput ? QString::fromLatin1("\n") : QString())
      << (result.stdOut().isEmpty() ? QString() : result.stdOut().join(QLatin1Char('\n')));
    if (!result.stdErr().isEmpty())
        w << result.stdErr().join(QLatin1Char('\n')) << MessageTag(QStringLiteral("stdErr"));
}

bool CommandLineFrontend::resolvingMultipleProjects() const
{
    return isResolving() && m_resolveJobs.count() + m_projects.count() > 1;
}

bool CommandLineFrontend::isResolving() const
{
    return !m_resolveJobs.isEmpty();
}

bool CommandLineFrontend::isBuilding() const
{
    return !m_buildJobs.isEmpty();
}

CommandLineFrontend::ProductMap CommandLineFrontend::productsToUse() const
{
    ProductMap products;
    QStringList productNames;
    const bool useAll = m_parser.products().isEmpty();
    foreach (const Project &project, m_projects) {
        QList<ProductData> &productList = products[project];
        const ProjectData projectData = project.projectData();
        foreach (const ProductData &product, projectData.allProducts()) {
            if (useAll || m_parser.products().contains(product.name())) {
                productList << product;
                productNames << product.name();
            }
        }
    }

    foreach (const QString &productName, m_parser.products()) {
        if (!productNames.contains(productName))
            throw ErrorInfo(Tr::tr("No such product '%1'.").arg(productName));
    }

    return products;
}

void CommandLineFrontend::handleProjectsResolved()
{
    if (m_cancelStatus != CancelStatusNone)
        throw ErrorInfo(Tr::tr("Execution canceled."));
    switch (m_parser.command()) {
    case ResolveCommandType:
        qApp->quit();
        break;
    case CleanCommandType:
        makeClean();
        break;
    case ShellCommandType:
        qApp->exit(runShell());
        break;
    case StatusCommandType:
        qApp->exit(printStatus(m_projects.first().projectData()));
        break;
    case BuildCommandType:
    case GenerateCommandType:
        build();
        break;
    case InstallCommandType:
    case RunCommandType:
        if (m_parser.buildBeforeInstalling())
            build();
        else
            install();
        break;
    case UpdateTimestampsCommandType:
        updateTimestamps();
        qApp->quit();
        break;
    case DumpNodesTreeCommandType:
        dumpNodesTree();
        qApp->quit();
        break;
    case HelpCommandType:
        Q_ASSERT_X(false, Q_FUNC_INFO, "Impossible.");
    }
}

void CommandLineFrontend::makeClean()
{
    if (m_parser.products().isEmpty()) {
        foreach (const Project &project, m_projects) {
            m_buildJobs << project.cleanAllProducts(m_parser.cleanOptions(project.profile()), this);
        }
    } else {
        const ProductMap &products = productsToUse();
        for (ProductMap::ConstIterator it = products.begin(); it != products.end(); ++it) {
            m_buildJobs << it.key().cleanSomeProducts(it.value(),
                                                      m_parser.cleanOptions(it.key().profile()),
                                                      this);
        }
    }
    connectBuildJobs();
}

int CommandLineFrontend::runShell()
{
    const ProductData productToRun = getTheOneRunnableProduct();
    RunEnvironment runEnvironment = m_projects.first().getRunEnvironment(productToRun,
            m_parser.installOptions(m_projects.first().profile()),
            QProcessEnvironment::systemEnvironment(), m_settings);
    return runEnvironment.doRunShell();
}

BuildOptions CommandLineFrontend::buildOptions(const Project &project) const
{
    BuildOptions options = m_parser.buildOptions(m_projects.first().profile());
    if (options.maxJobCount() <= 0) {
        const QString profileName = project.profile();
        QBS_CHECK(!profileName.isEmpty());
        options.setMaxJobCount(Preferences(m_settings, profileName).jobs());
    }
    return options;
}

QString CommandLineFrontend::buildDirectory(const QString &profileName) const
{
    QString buildDir = m_parser.projectBuildDirectory();
    if (buildDir.isEmpty()) {
        buildDir = Preferences(m_settings, profileName).defaultBuildDirectory();
        if (buildDir.isEmpty()) {
            qbsDebug() << "No project build directory given; using current directory.";
            buildDir = QDir::currentPath();
        } else {
            qbsDebug() << "No project build directory given; using directory from preferences.";
        }
    }

    QString projectName(QFileInfo(m_parser.projectFilePath()).baseName());
    buildDir.replace(BuildDirectoryOption::magicProjectString(), projectName);
    QString projectDir(QFileInfo(m_parser.projectFilePath()).path());
    buildDir.replace(BuildDirectoryOption::magicProjectDirString(), projectDir);
    if (!QFileInfo(buildDir).isAbsolute())
        buildDir = QDir::currentPath() + QLatin1Char('/') + buildDir;
    buildDir = QDir::cleanPath(buildDir);
    return buildDir;
}

void CommandLineFrontend::build()
{
    if (m_parser.products().isEmpty()) {
        const Project::ProductSelection productSelection = m_parser.withNonDefaultProducts()
                ? Project::ProductSelectionWithNonDefault : Project::ProductSelectionDefaultOnly;
        foreach (const Project &project, m_projects)
            m_buildJobs << project.buildAllProducts(buildOptions(project), productSelection, this);
    } else {
        const ProductMap &products = productsToUse();
        for (ProductMap::ConstIterator it = products.begin(); it != products.end(); ++it)
            m_buildJobs << it.key().buildSomeProducts(it.value(), buildOptions(it.key()), this);
    }
    connectBuildJobs();

    /*
     * Progress reporting for the build jobs works as follows: We know that for every job,
     * the newTaskStarted() signal is emitted exactly once (unless there's an error). So we add up
     * the respective total efforts as they come in. Once all jobs have reported their total
     * efforts, we can start the overall progress report.
     */
    m_buildEffortsNeeded = m_buildJobs.count();
    m_buildEffortsRetrieved = 0;
    m_totalBuildEffort = 0;
    m_currentBuildEffort = 0;
}

void CommandLineFrontend::generate()
{
    const QString generatorName = m_parser.generateOptions().generatorName();
    auto generator = ProjectGeneratorManager::findGenerator(generatorName);
    if (!generator) {
        const QString generatorNames = ProjectGeneratorManager::loadedGeneratorNames()
                .join(QLatin1String("\n\t"));
        if (generatorName.isEmpty()) {
            throw ErrorInfo(Tr::tr("No generator specified. Available generators:\n\t%1")
                            .arg(generatorNames));
        }

        throw ErrorInfo(Tr::tr("No generator named '%1'. Available generators:\n\t%2")
                        .arg(generatorName)
                        .arg(generatorNames));
    }

    generator->generate(m_projects,
                        m_parser.buildConfigurations(),
                        m_parser.installOptions(QString()),
                        m_parser.settingsDir(),
                        ConsoleLogger::instance(m_settings));
}

int CommandLineFrontend::runTarget()
{
    const ProductData productToRun = getTheOneRunnableProduct();
    const QString executableFilePath = productToRun.targetExecutable();
    if (executableFilePath.isEmpty()) {
        throw ErrorInfo(Tr::tr("Cannot run: Product '%1' is not an application.")
                    .arg(productToRun.name()));
    }
    RunEnvironment runEnvironment = m_projects.first().getRunEnvironment(productToRun,
            m_parser.installOptions(m_projects.first().profile()),
            QProcessEnvironment::systemEnvironment(), m_settings);
    return runEnvironment.doRunTarget(executableFilePath, m_parser.runArgs());
}

void CommandLineFrontend::updateTimestamps()
{
    const ProductMap &products = productsToUse();
    for (ProductMap::ConstIterator it = products.constBegin(); it != products.constEnd(); ++it) {
        Project p = it.key();
        p.updateTimestamps(it.value());
    }
}

void CommandLineFrontend::dumpNodesTree()
{
    QFile stdOut;
    stdOut.open(stdout, QIODevice::WriteOnly);
    const ErrorInfo error = m_projects.first().dumpNodesTree(stdOut, productsToUse()
                                                             .value(m_projects.first()));
    if (error.hasError())
        throw error;
}

void CommandLineFrontend::connectBuildJobs()
{
    foreach (AbstractJob * const job, m_buildJobs)
        connectBuildJob(job);
}

void CommandLineFrontend::connectBuildJob(AbstractJob *job)
{
    connectJob(job);

    BuildJob *bjob = qobject_cast<BuildJob *>(job);
    if (!bjob)
        return;

    connect(bjob, &BuildJob::reportCommandDescription,
            this, &CommandLineFrontend::handleCommandDescriptionReport);
    connect(bjob, &BuildJob::reportProcessResult,
            this, &CommandLineFrontend::handleProcessResultReport);
}

void CommandLineFrontend::connectJob(AbstractJob *job)
{
    connect(job, &AbstractJob::finished,
            this, &CommandLineFrontend::handleJobFinished);
    connect(job, &AbstractJob::taskStarted,
            this, &CommandLineFrontend::handleNewTaskStarted);
    connect(job, &AbstractJob::totalEffortChanged,
            this, &CommandLineFrontend::handleTotalEffortChanged);
    if (m_parser.showProgress()) {
        connect(job, &AbstractJob::taskProgress,
                this, &CommandLineFrontend::handleTaskProgress);
    }
}

ProductData CommandLineFrontend::getTheOneRunnableProduct()
{
    QBS_CHECK(m_projects.count() == 1); // Has been checked earlier.

    if (m_parser.products().count() == 1) {
        foreach (const ProductData &p, m_projects.first().projectData().allProducts()) {
            if (p.name() == m_parser.products().first())
                return p;
        }
        QBS_CHECK(false);
    }
    QBS_CHECK(m_parser.products().count() == 0);

    QList<ProductData> runnableProducts;
    foreach (const ProductData &p, m_projects.first().projectData().allProducts()) {
        if (p.isRunnable())
            runnableProducts << p;
    }

    if (runnableProducts.count() == 1)
        return runnableProducts.first();

    if (runnableProducts.isEmpty()) {
        throw ErrorInfo(Tr::tr("Cannot execute command '%1': Project has no runnable product.")
                        .arg(m_parser.commandName()));
    }

    ErrorInfo error(Tr::tr("Ambiguous use of command '%1': No product given, but project "
                           "has more than one runnable product.").arg(m_parser.commandName()));
    error.append(Tr::tr("Use the '--products' option with one of the following products:"));
    foreach (const ProductData &p, runnableProducts) {
        QString productRepr = QLatin1String("\t") + p.name();
        if (p.profile() != m_projects.first().profile()) {
            productRepr.append(QLatin1String(" [")).append(p.profile())
                    .append(QLatin1Char(']'));
        }
        error.append(productRepr);
    }
    throw error;
}

void CommandLineFrontend::install()
{
    Q_ASSERT(m_projects.count() == 1);
    const Project project = m_projects.first();
    InstallJob *installJob;
    if (m_parser.products().isEmpty()) {
        const Project::ProductSelection productSelection = m_parser.withNonDefaultProducts()
                ? Project::ProductSelectionWithNonDefault : Project::ProductSelectionDefaultOnly;
        installJob = project.installAllProducts(m_parser.installOptions(project.profile()),
                                                productSelection);
    } else {
        const Project project = m_projects.first();
        const ProductMap products = productsToUse();
        installJob = project.installSomeProducts(products.value(project),
                                                 m_parser.installOptions(project.profile()));
    }
    connectJob(installJob);
}

} // namespace qbs
