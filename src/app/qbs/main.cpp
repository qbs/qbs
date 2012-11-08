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
#include "consoleprogressobserver.h"
#include "showproperties.h"
#include "status.h"
#include "../shared/commandlineparser.h"

#include <qbs.h>
#include <logging/consolelogger.h>
#include <tools/hostosinfo.h>
#include <tools/runenvironment.h>

#include <QCoreApplication>
#include <QDir>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopedPointer>
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

static QList<Product> productsToUse(QbsEngine &qbsEngine, const QList<Project::Id> &projectIds,
                                    const QStringList &selectedProducts)
{
    QList<Product> products;
    QStringList productNames;
    const bool useAll = selectedProducts.isEmpty();
    foreach (const Project::Id projectId, projectIds) {
        const Project project = qbsEngine.retrieveProject(projectId);
        foreach (const Product &product, project.products()) {
            if (useAll || selectedProducts.contains(product.name())) {
                products << product;
                productNames << product.name();
            }
        }
    }

    foreach (const QString &productName, selectedProducts) {
        if (!productNames.contains(productName)) {
            qbsWarning() << QCoreApplication::translate("qbs", "No such product '%1'.")
                            .arg(productName);
        }
    }

    return products;
}

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

static void makeClean(QbsEngine &qbsEngine, const QList<Project::Id> &projectIds,
                     const QStringList &productsSetByUser, const BuildOptions &buildOptions)
{
    const QList<Product> products = productsToUse(qbsEngine, projectIds, productsSetByUser);
    qbsEngine.cleanProducts(products, buildOptions, QbsEngine::CleanupTemporaries);
}

// TODO: Don't take a random product.
static int runShell(QbsEngine &qbsEngine, const Project &project)
{
    try {
        RunEnvironment runEnvironment = qbsEngine.getRunEnvironment(project.products().first(),
                QProcessEnvironment::systemEnvironment());
        return runEnvironment.runShell();
    } catch (const Error &error) {
        qbsError() << error.toString();
        return EXIT_FAILURE;
    }
}

class AbstractBuilder : public QObject
{
    Q_OBJECT

protected:
    AbstractBuilder(QbsEngine &qbsEngine, const BuildOptions &buildOptions)
        : m_qbsEngine(qbsEngine), m_buildOptions(buildOptions)
    {
    }

    QbsEngine &qbsEngine() const { return m_qbsEngine; }
    const BuildOptions buildOptions() const { return m_buildOptions; }

private slots:
    void build()
    {
        try {
            doBuild();
            qApp->quit();
        } catch (const Error &error) {
            qbsError() << error.toString();
            qApp->exit(ExitCodeErrorBuildFailure);
        }
    }

private:
    virtual void doBuild() = 0;

    QbsEngine &m_qbsEngine;
    const BuildOptions m_buildOptions;
};

class ProductsBuilder : public AbstractBuilder
{
public:
    ProductsBuilder(QbsEngine &qbsEngine, const QList<Product> &products,
                    const BuildOptions &buildOptions)
        : AbstractBuilder(qbsEngine, buildOptions), m_products(products)
    {
    }

private:
    void doBuild() { qbsEngine().buildProducts(m_products, buildOptions()); }

    const QList<Product> m_products;
};

class ProjectsBuilder : public AbstractBuilder
{
public:
    ProjectsBuilder(QbsEngine &qbsEngine, const QList<Project::Id> &projectIds,
                    const BuildOptions &buildOptions)
        : AbstractBuilder(qbsEngine, buildOptions), m_projectIds(projectIds)
    {
    }

private:
    void doBuild() { qbsEngine().buildProjects(m_projectIds, buildOptions()); }

    const QList<Project::Id> m_projectIds;
};

static int build(QbsEngine &qbsEngine, const QList<Project::Id> &projectIds,
                 const QStringList &productsSetByUser, const BuildOptions &buildOptions)
{
    QScopedPointer<AbstractBuilder> builder;
    if (productsSetByUser.isEmpty()) {
        builder.reset(new ProjectsBuilder(qbsEngine, projectIds, buildOptions));
    } else {
        const QList<Product> products = productsToUse(qbsEngine, projectIds, productsSetByUser);
        builder.reset(new ProductsBuilder(qbsEngine, products, buildOptions));
    }
    QTimer::singleShot(0, builder.data(), SLOT(build()));
    return qApp->exec();
}

static int runTarget(QbsEngine &qbsEngine, const QList<Product> &products,
                     const QString &targetName, const QStringList &arguments)
{
    try {
        Product productToRun;
        QString productFileName;

        foreach (const Product &product, products) {
            const QString executable = qbsEngine.targetExecutable(product);
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

        if (!productToRun.id().isValid()) {
            if (targetName.isEmpty())
                qbsError() << QObject::tr("Can't find a suitable product to run.");
            else
                qbsError() << QObject::tr("No such target: '%1'").arg(targetName);
            return ExitCodeErrorBuildFailure;
        }

        RunEnvironment runEnvironment = qbsEngine.getRunEnvironment(productToRun,
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

    QList<Project::Id> projectIds;
    QbsEngine qbsEngine;
    qbsEngine.setBuildRoot(QDir::currentPath());
    QScopedPointer<ConsoleProgressObserver> observer;
    if (parser.showProgress()) {
        observer.reset(new ConsoleProgressObserver);
        qbsEngine.setProgressObserver(observer.data());
    }
    try {
        foreach (const QVariantMap &buildConfig, parser.buildConfigurations()) {
            const Project::Id projectId
                    = qbsEngine.setupProject(parser.projectFileName(), buildConfig);
            projectIds << projectId;
        }
    } catch (const Error &error) {
        qbsError() << error.toString();
        return ExitCodeErrorLoadingProjectFailed;
    }

    try {
        switch (parser.command()) {
        case CommandLineParser::CleanCommand:
            makeClean(qbsEngine, projectIds, parser.products(), parser.buildOptions());
            break;
        case CommandLineParser::StartShellCommand:
            return runShell(qbsEngine, qbsEngine.retrieveProject(projectIds.first()));
        case CommandLineParser::StatusCommand: {
            QList<Project> projects;
            foreach (const Project::Id &id, projectIds)
                projects << qbsEngine.retrieveProject(id);
            return printStatus(projects);
        }
        case CommandLineParser::PropertiesCommand:
            return showProperties(productsToUse(qbsEngine, projectIds, parser.products()));
        case CommandLineParser::BuildCommand:
            return build(qbsEngine, projectIds, parser.products(), parser.buildOptions());
        case CommandLineParser::RunCommand: {
            const int buildExitCode = build(qbsEngine, projectIds, parser.products(),
                                            parser.buildOptions());
            if (buildExitCode != 0)
                return buildExitCode;
            const QList<Product> products = productsToUse(qbsEngine, projectIds, parser.products());
            return runTarget(qbsEngine, products, parser.runTargetName(), parser.runArgs());
        }
        }
    } catch (const Error &error) {
        qbsError() << error.toString();
        return EXIT_FAILURE;
    }
}

#include "main.moc"
