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
#include "commandlinefrontend.h"

#include "application.h"
#include "consoleprogressobserver.h"
#include "showproperties.h"
#include "status.h"

#include <qbs.h>
#include <api/runenvironment.h>
#include <logging/translator.h>

#include <QDir>
#include <QProcessEnvironment>
#include <cstdlib>

namespace qbs {

CommandLineFrontend::CommandLineFrontend(const CommandLineParser &parser, QObject *parent)
    : QObject(parent), m_parser(parser), m_observer(0)
{
}

void CommandLineFrontend::cancel()
{
    foreach (AbstractJob * const job, m_resolveJobs)
        job->cancel();
    foreach (AbstractJob * const job, m_buildJobs)
        job->cancel();
}

void CommandLineFrontend::start()
{
    try {
        switch (m_parser.command()) {
        case ShellCommandType:
            if (m_parser.products().count() > 1) {
                throw Error(Tr::tr("Invalid use of command '%1': Cannot use more than one "
                                   "product.\nUsage: %2")
                            .arg(m_parser.commandName(), m_parser.commandDescription()));
            }
            // Fall-through intended.
        case RunCommandType:
        case PropertiesCommandType:
        case StatusCommandType:
            if (m_parser.buildConfigurations().count() > 1) {
                QString error = Tr::tr("Invalid use of command '%1': There can be only one "
                               "build configuration.\n").arg(m_parser.commandName());
                error += Tr::tr("Usage: %1").arg(m_parser.commandDescription());
                throw Error(error);
            }
            break;
        default:
            break;
        }

        if (m_parser.showProgress())
            m_observer = new ConsoleProgressObserver;
        foreach (const QVariantMap &buildConfig, m_parser.buildConfigurations()) {
            SetupProjectJob * const job = Project::setupProject(m_parser.projectFilePath(),
                                                                buildConfig, QDir::currentPath(), this);
            connectJob(job);
            m_resolveJobs << job;
        }

        /*
         * Progress reporting on the terminal gets a bit tricky when resolving several projects
         * concurrently, since we cannot show multiple progress bars at the same time. Instead,
         * we just set the total effort to the number of projects and increase the progress
         * every time one of them finishes, ingoring the progress reports from the jobs themselves.
         * (Yes, that does mean it will take disproportionately long for the first progress
         * notification to arrive.)
         */
        if (m_parser.showProgress() && resolvingMultipleProjects())
            m_observer->initialize(tr("Setting up projects"), m_resolveJobs.count());
    } catch (const Error &error) {
        qbsError() << error.toString();
        if (m_buildJobs.isEmpty() && m_resolveJobs.isEmpty())
            qApp->exit(EXIT_FAILURE);
        else
            cancel();
    }
}

void CommandLineFrontend::handleJobFinished(bool success, AbstractJob *job)
{
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
    } else { // Build or clean.
        m_buildJobs.removeOne(job);
        if (m_buildJobs.isEmpty()) {
            if (m_parser.command() == RunCommandType)
                qApp->exit(runTarget());
            else
                qApp->quit();
        }
    }
}

void CommandLineFrontend::handleNewTaskStarted(const QString &description, int totalEffort)
{
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
        foreach (const ProductData &product, projectData.products()) {
            if (useAll || m_parser.products().contains(product.name())) {
                productList << product;
                productNames << product.name();
            }
        }
    }

    foreach (const QString &productName, m_parser.products()) {
        if (!productNames.contains(productName))
            throw Error(Tr::tr("No such product '%1'.").arg(productName));
    }

    return products;
}

void CommandLineFrontend::handleProjectsResolved()
{
    try {
        switch (m_parser.command()) {
        case CleanCommandType:
            makeClean();
            break;
        case ShellCommandType:
            if (m_parser.products().count() == 0
                    && m_projects.first().projectData().products().count() > 1) {
                throw Error(Tr::tr("Ambiguous use of command '%1': No product given for project "
                                   "with more than one product.\nUsage: %2")
                            .arg(m_parser.commandName(), m_parser.commandDescription()));
            }
            qApp->exit(runShell());
            break;
        case StatusCommandType: {
            qApp->exit(printStatus(m_projects.first().projectData()));
            break;
        }
        case PropertiesCommandType: {
            QList<ProductData> products;
            const ProductMap &p = productsToUse();
            foreach (const QList<ProductData> &pProducts, p)
                products << pProducts;
            qApp->exit(showProperties(products));
            break;
        }
        case BuildCommandType:
        case RunCommandType:
            build();
            break;
        case HelpCommandType:
            Q_ASSERT_X(false, Q_FUNC_INFO, "Impossible.");
        }
    } catch (const Error &error) {
        qbsError() << error.toString();
        qApp->exit(EXIT_FAILURE);
    }
}

void CommandLineFrontend::makeClean()
{
    if (m_parser.products().isEmpty()) {
        foreach (const Project &project, m_projects) {
            m_buildJobs << project.cleanAllProducts(m_parser.buildOptions(),
                                                     Project::CleanupTemporaries, this);
        }
    } else {
        const ProductMap &products = productsToUse();
        for (ProductMap::ConstIterator it = products.begin(); it != products.end(); ++it) {
            m_buildJobs << it.key().cleanSomeProducts(it.value(), m_parser.buildOptions(),
                                                      Project::CleanupTemporaries, this);

        }
    }
    connectBuildJobs();
}

int CommandLineFrontend::runShell()
{
    const ProductMap &productMap = productsToUse();
    Q_ASSERT(productMap.count() == 1);
    const Project &project = productMap.begin().key();
    const QList<ProductData> &products = productMap.begin().value();
    Q_ASSERT(products.count() == 1);
    RunEnvironment runEnvironment = project.getRunEnvironment(products.first(),
            QProcessEnvironment::systemEnvironment());
    return runEnvironment.runShell();
}

void CommandLineFrontend::build()
{
    if (m_parser.products().isEmpty()) {
        foreach (const Project &project, m_projects)
            m_buildJobs << project.buildAllProducts(m_parser.buildOptions(), this);
    } else {
        const ProductMap &products = productsToUse();
        for (ProductMap::ConstIterator it = products.begin(); it != products.end(); ++it)
            m_buildJobs << it.key().buildSomeProducts(it.value(), m_parser.buildOptions(), this);
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

int CommandLineFrontend::runTarget()
{
    ProductData productToRun;
    QString productFileName;

    const QString targetName = m_parser.runTargetName();
    Q_ASSERT(m_projects.count() == 1);
    const Project &project = m_projects.first();
    foreach (const ProductData &product, productsToUse().value(project)) {
        const QString executable = project.targetExecutable(product);
        if (executable.isEmpty())
            continue;
        if (!targetName.isEmpty() && !executable.endsWith(targetName))
            continue;
        if (!productFileName.isEmpty()) {
            qbsError() << tr("There is more than one executable target in "
                                      "the project. Please specify which target "
                                      "you want to run.");
            return EXIT_FAILURE;
        }
        productFileName = executable;
        productToRun = product;
    }

    if (productToRun.name().isEmpty()) {
        if (targetName.isEmpty())
            qbsError() << tr("Can't find a suitable product to run.");
        else
            qbsError() << tr("No such target: '%1'").arg(targetName);
        return EXIT_FAILURE;
    }

    RunEnvironment runEnvironment = project.getRunEnvironment(productToRun,
            QProcessEnvironment::systemEnvironment());
    return runEnvironment.runTarget(productFileName, m_parser.runArgs());
}

void CommandLineFrontend::connectBuildJobs()
{
    foreach (AbstractJob * const job, m_buildJobs)
        connectJob(job);
}

void CommandLineFrontend::connectJob(AbstractJob *job)
{
    connect(job, SIGNAL(finished(bool, qbs::AbstractJob*)),
            SLOT(handleJobFinished(bool, qbs::AbstractJob*)));
    if (m_parser.showProgress()) {
        connect(job, SIGNAL(taskStarted(QString,int,qbs::AbstractJob*)),
                SLOT(handleNewTaskStarted(QString,int)));
        connect(job, SIGNAL(taskProgress(int,qbs::AbstractJob*)),
                SLOT(handleTaskProgress(int,qbs::AbstractJob*)));
    }
}

} // namespace qbs
