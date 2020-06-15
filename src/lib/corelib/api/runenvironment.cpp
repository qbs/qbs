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

#include "runenvironment.h"

#include <api/projectdata.h>
#include <buildgraph/environmentscriptrunner.h>
#include <buildgraph/productinstaller.h>
#include <buildgraph/rulesevaluationcontext.h>
#include <language/language.h>
#include <language/propertymapinternal.h>
#include <language/scriptengine.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/installoptions.h>
#include <tools/preferences.h>
#include <tools/qbsassert.h>
#include <tools/shellutils.h>
#include <tools/stringconstants.h>

#include <QtCore/qdir.h>
#include <QtCore/qprocess.h>
#include <QtCore/qprocess.h>
#include <QtCore/qtemporaryfile.h>
#include <QtCore/qvariant.h>

#include <cstdlib>
#include <regex>

namespace qbs {
using namespace Internal;

class RunEnvironment::RunEnvironmentPrivate
{
public:
    RunEnvironmentPrivate(ResolvedProductPtr product, TopLevelProjectConstPtr project,
            InstallOptions installOptions, const QProcessEnvironment &environment,
            QStringList setupRunEnvConfig, Settings *settings, Logger logger)
        : resolvedProduct(std::move(product))
        , project(std::move(project))
        , installOptions(std::move(installOptions))
        , environment(environment)
        , setupRunEnvConfig(std::move(setupRunEnvConfig))
        , settings(settings)
        , logger(std::move(logger))
        , evalContext(this->logger)
    {
    }

    void checkProduct()
    {
        if (!resolvedProduct)
            throw ErrorInfo(Tr::tr("Cannot run: No such product."));
        if (!resolvedProduct->enabled) {
            throw ErrorInfo(Tr::tr("Cannot run disabled product '%1'.")
                            .arg(resolvedProduct->fullDisplayName()));
        }
    }

    const ResolvedProductPtr resolvedProduct;
    const TopLevelProjectConstPtr project;
    InstallOptions installOptions;
    const QProcessEnvironment environment;
    const QStringList setupRunEnvConfig;
    Settings * const settings;
    Logger logger;
    RulesEvaluationContext evalContext;
};

RunEnvironment::RunEnvironment(const ResolvedProductPtr &product,
        const TopLevelProjectConstPtr &project, const InstallOptions &installOptions,
        const QProcessEnvironment &environment, const QStringList &setupRunEnvConfig,
        Settings *settings, const Logger &logger)
    : d(new RunEnvironmentPrivate(product, project, installOptions, environment, setupRunEnvConfig,
                                  settings, logger))
{
}

RunEnvironment::~RunEnvironment()
{
    delete d;
}

int RunEnvironment::runShell(ErrorInfo *error)
{
    try {
        return doRunShell();
    } catch (const ErrorInfo &e) {
        if (error)
            *error = e;
        return -1;
    }
}

int RunEnvironment::runTarget(const QString &targetBin, const QStringList &arguments, bool dryRun,
                              ErrorInfo *error)
{
    try {
        return doRunTarget(targetBin, arguments, dryRun);
    } catch (const ErrorInfo &e) {
        if (error)
            *error = e;
        return -1;
    }
}

const QProcessEnvironment RunEnvironment::runEnvironment(ErrorInfo *error) const
{
    try {
        return getRunEnvironment();
    } catch (const ErrorInfo &e) {
        if (error)
            *error = e;
        return {};
    }
}

const QProcessEnvironment RunEnvironment::buildEnvironment(ErrorInfo *error) const
{
    try {
        return getBuildEnvironment();
    } catch (const ErrorInfo &e) {
        if (error)
            *error = e;
        return {};
    }
}

int RunEnvironment::doRunShell()
{
    if (d->resolvedProduct) {
        EnvironmentScriptRunner(d->resolvedProduct.get(), &d->evalContext,
                                d->project->environment).setupForBuild();
    }

    const QString productId = d->resolvedProduct ? d->resolvedProduct->name : QString();
    const QString configName = d->project->id();
    if (productId.isEmpty()) {
        d->logger.qbsInfo() << Tr::tr("Starting shell for configuration '%1'.").arg(configName);
    } else {
        d->logger.qbsInfo() << Tr::tr("Starting shell for product '%1' in configuration '%2'.")
                               .arg(productId, configName);
    }
    const QProcessEnvironment environment = d->resolvedProduct
            ? d->resolvedProduct->buildEnvironment : d->project->environment;
#if defined(Q_OS_LINUX)
    clearenv();
#endif
    const auto keys = environment.keys();
    for (const QString &key : keys)
        qputenv(key.toLocal8Bit().constData(), environment.value(key).toLocal8Bit());
    QString command;
    if (HostOsInfo::isWindowsHost()) {
        command = environment.value(QStringLiteral("COMSPEC"));
        if (command.isEmpty())
            command = QStringLiteral("cmd");
        const QString prompt = environment.value(QStringLiteral("PROMPT"));
        command += QLatin1String(" /k prompt [qbs] ") + prompt;
    } else {
        const QVariantMap qbsProps =
                (d->resolvedProduct ? d->resolvedProduct->moduleProperties->value()
                                    : d->project->buildConfiguration())
                .value(StringConstants::qbsModule()).toMap();
        const QString profileName = qbsProps.value(StringConstants::profileProperty()).toString();
        command = Preferences(d->settings, profileName).shell();
        if (command.isEmpty())
            command = environment.value(QStringLiteral("SHELL"), QStringLiteral("/bin/sh"));

        // Yes, we have to use this procedure. PS1 is not inherited from the environment.
        const QString prompt = QLatin1String("qbs ") + configName
                + (!productId.isEmpty() ? QLatin1Char(' ') + productId : QString())
                + QLatin1String(" $ ");
        QTemporaryFile envFile;
        if (envFile.open()) {
            if (command.endsWith(QLatin1String("bash")))
                command += QLatin1String(" --posix"); // Teach bash some manners.
            const QString promptLine = QLatin1String("PS1='") + prompt + QLatin1String("'\n");
            envFile.write(promptLine.toLocal8Bit());
            envFile.close();
            qputenv("ENV", envFile.fileName().toLocal8Bit());
        } else {
            d->logger.qbsWarning() << Tr::tr("Setting custom shell prompt failed.");
        }
    }

    // We cannot use QProcess, since it does not do stdin forwarding.
    return system(command.toLocal8Bit().constData());
}

static QString findExecutable(const QStringList &fileNames)
{
    const QStringList path = QString::fromLocal8Bit(qgetenv("PATH"))
            .split(HostOsInfo::pathListSeparator(), QBS_SKIP_EMPTY_PARTS);

    for (const QString &fileName : fileNames) {
        const QString exeFileName = HostOsInfo::appendExecutableSuffix(fileName);
        for (const QString &ppath : path) {
            const QString fullPath = ppath + QLatin1Char('/') + exeFileName;
            QFileInfo fi(fullPath);
            if (fi.exists() && fi.isFile() && fi.isExecutable())
                return QDir::cleanPath(fullPath);
        }
    }
    return {};
}

static std::string readAaptBadgingAttribute(const std::string &line)
{
    std::regex re("^[A-Za-z\\-]+:\\s+name='(.+?)'.*$");
    std::smatch match;
    if (std::regex_match(line, match, re))
        return match[1];
    return {};
}

static QString findMainIntent(const QString &aapt, const QString &apkFilePath)
{
    QString packageId;
    QString activity;
    QProcess aaptProcess;
    aaptProcess.start(aapt, QStringList()
                      << QStringLiteral("dump")
                      << QStringLiteral("badging")
                      << apkFilePath);
    if (aaptProcess.waitForFinished(-1)) {
        const auto lines = aaptProcess.readAllStandardOutput().split('\n');
        for (const auto &line : lines) {
            if (line.startsWith(QByteArrayLiteral("package:")))
                packageId = QString::fromStdString(readAaptBadgingAttribute(line.toStdString()));
            else if (line.startsWith(QByteArrayLiteral("launchable-activity:")))
                activity = QString::fromStdString(readAaptBadgingAttribute(line.toStdString()));
        }
    }

    if (!packageId.isEmpty() && !activity.isEmpty())
        return packageId + QStringLiteral("/") + activity;
    return {};
}

void RunEnvironment::printStartInfo(const QProcess &proc, bool dryRun)
{
    QString message = dryRun ? Tr::tr("Would start target.") : Tr::tr("Starting target.");
    message.append(QLatin1Char(' ')).append(Tr::tr("Full command line: %1")
            .arg(shellQuote(QStringList(QDir::toNativeSeparators(proc.program()))
                            << proc.arguments())));
    d->logger.qbsInfo() << message;
}

int RunEnvironment::doRunTarget(const QString &targetBin, const QStringList &arguments, bool dryRun)
{
    d->checkProduct();
    const QStringList targetOS = d->resolvedProduct->moduleProperties->qbsPropertyValue(
                QStringLiteral("targetOS")).toStringList();
    const QStringList toolchain = d->resolvedProduct->moduleProperties->qbsPropertyValue(
                QStringLiteral("toolchain")).toStringList();

    QString targetExecutable = targetBin;
    QStringList targetArguments = arguments;
    const QString completeSuffix = QFileInfo(targetBin).completeSuffix();

    if (targetOS.contains(QLatin1String("android"))) {
        const auto aapt = d->resolvedProduct->moduleProperties->moduleProperty(
                    QStringLiteral("Android.sdk"), QStringLiteral("aaptFilePath")).toString();
        const auto intent = findMainIntent(aapt, targetBin);
        const auto sdkDir = d->resolvedProduct->moduleProperties->moduleProperty(
                    QStringLiteral("Android.sdk"), QStringLiteral("sdkDir")).toString();
        targetExecutable = sdkDir + QStringLiteral("/platform-tools/adb");

        if (!dryRun) {
            QProcess process;
            process.setProcessChannelMode(QProcess::ForwardedChannels);
            process.start(targetExecutable, QStringList()
                          << StringConstants::androidInstallCommand()
                          << QStringLiteral("-r") // replace existing application
                          << QStringLiteral("-t") // allow test packages
                          << QStringLiteral("-d") // allow version code downgrade
                          << targetBin);
            if (!process.waitForFinished(-1)) {
                if (process.error() == QProcess::FailedToStart) {
                    throw ErrorInfo(Tr::tr("The process '%1' could not be started: %2")
                                    .arg(targetExecutable)
                                    .arg(process.errorString()));
                } else {
                    d->logger.qbsWarning()
                            << "QProcess error: " << process.errorString();
                }

                return EXIT_FAILURE;
            }

            targetArguments << QStringList()
                            << QStringLiteral("shell")
                            << QStringLiteral("am")
                            << QStringLiteral("start")
                            << QStringLiteral("-W") // wait for launch to complete
                            << QStringLiteral("-n")
                            << intent;
        }
    } else if (targetOS.contains(QLatin1String("ios")) || targetOS.contains(QLatin1String("tvos"))) {
        const QString bundlePath = targetBin + StringConstants::slashDotDot();

        if (targetOS.contains(QStringLiteral("ios-simulator"))
                || targetOS.contains(QStringLiteral("tvos-simulator"))) {
            const auto developerDir = d->resolvedProduct->moduleProperties->moduleProperty(
                        StringConstants::xcode(), QStringLiteral("developerPath")).toString();
            targetExecutable = developerDir + QStringLiteral("/usr/bin/simctl");
            const auto simulatorId = QStringLiteral("booted"); // TODO: parameterize
            const auto bundleId = d->resolvedProduct->moduleProperties->moduleProperty(
                        QStringLiteral("bundle"), QStringLiteral("identifier")).toString();

            if (!dryRun) {
                QProcess process;
                process.setProcessChannelMode(QProcess::ForwardedChannels);
                process.start(targetExecutable, QStringList()
                              << StringConstants::simctlInstallCommand()
                              << simulatorId
                              << QDir::cleanPath(bundlePath));
                if (!process.waitForFinished(-1)) {
                    if (process.error() == QProcess::FailedToStart) {
                        throw ErrorInfo(Tr::tr("The process '%1' could not be started: %2")
                                        .arg(targetExecutable)
                                        .arg(process.errorString()));
                    }

                    return EXIT_FAILURE;
                }

                targetArguments << QStringList()
                                << QStringLiteral("launch")
                                << QStringLiteral("--console")
                                << simulatorId
                                << bundleId
                                << arguments;
            }
        } else {
            if (QFileInfo(targetExecutable = findExecutable(QStringList()
                        << QStringLiteral("iostool"))).isExecutable()) {
                targetArguments = QStringList()
                        << QStringLiteral("-run")
                        << QStringLiteral("-bundle")
                        << QDir::cleanPath(bundlePath);
                if (!arguments.empty())
                    targetArguments << QStringLiteral("-extra-args") << arguments;
            } else if (QFileInfo(targetExecutable = findExecutable(QStringList()
                        << QStringLiteral("ios-deploy"))).isExecutable()) {
                targetArguments = QStringList()
                        << QStringLiteral("--no-wifi")
                        << QStringLiteral("--noninteractive")
                        << QStringLiteral("--bundle")
                        << QDir::cleanPath(bundlePath);
                if (!arguments.empty())
                    targetArguments << QStringLiteral("--args") << arguments;
            } else {
                d->logger.qbsLog(LoggerError)
                        << Tr::tr("No suitable deployment tools were found in the environment. "
                                  "Consider installing ios-deploy.");
                return EXIT_FAILURE;
            }
        }
    } else if (targetOS.contains(QLatin1String("windows"))) {
        if (completeSuffix == QLatin1String("msi")) {
            targetExecutable = QLatin1String("msiexec");
            targetArguments.prepend(QDir::toNativeSeparators(targetBin));
            targetArguments.prepend(QLatin1String("/package"));
        }

        // Run Windows executables through Wine when not on Windows
        if (!HostOsInfo::isWindowsHost()) {
            targetArguments.prepend(targetExecutable);
            targetExecutable = QStringLiteral("wine");
        }
    }

    if (toolchain.contains(QLatin1String("mono"))) {
        targetArguments.prepend(targetExecutable);
        targetExecutable = QStringLiteral("mono");
    }

    if (completeSuffix == QLatin1String("js")) {
        targetExecutable = d->resolvedProduct->moduleProperties->moduleProperty(
                    QStringLiteral("nodejs"), QStringLiteral("interpreterFilePath")).toString();
        if (targetExecutable.isEmpty())
            // The Node.js binary is called nodejs on Debian/Ubuntu-family operating systems due to
            // conflict with another package containing a binary named node
            targetExecutable = findExecutable(QStringList()
                                              << QStringLiteral("nodejs")
                                              << QStringLiteral("node"));
        targetArguments.prepend(targetBin);
    }

    // Only check if the target is executable if we're not running it through another
    // known application such as msiexec or wine, as we can't check in this case anyways
    QFileInfo fi(targetExecutable);
    if (!dryRun && targetBin == targetExecutable && (!fi.isFile() || !fi.isExecutable())) {
        d->logger.qbsLog(LoggerError) << Tr::tr("File '%1' is not an executable.")
                                .arg(QDir::toNativeSeparators(targetExecutable));
        return EXIT_FAILURE;
    }

    QProcessEnvironment env = d->environment;
    env.insert(QStringLiteral("QBS_RUN_FILE_PATH"), targetBin);
    EnvironmentScriptRunner(d->resolvedProduct.get(), &d->evalContext, env)
            .setupForRun(d->setupRunEnvConfig);

    QProcess process;
    process.setProcessEnvironment(d->resolvedProduct->runEnvironment);
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.setProgram(targetExecutable);
    process.setArguments(targetArguments);
    printStartInfo(process, dryRun);
    if (dryRun)
        return EXIT_SUCCESS;
    process.start();
    if (!process.waitForFinished(-1)) {
        if (process.error() == QProcess::FailedToStart) {
            QString errorPrefixString;
#ifdef Q_OS_UNIX
            if (QFileInfo(targetExecutable).isExecutable()) {
                const QString interpreter(shellInterpreter(targetExecutable));
                if (!interpreter.isEmpty()) {
                    errorPrefixString = Tr::tr("%1: bad interpreter: ").arg(interpreter);
                }
            }
#endif
            throw ErrorInfo(Tr::tr("The process '%1' could not be started: %2")
                            .arg(targetExecutable)
                            .arg(errorPrefixString + process.errorString()));
        } else {
            d->logger.qbsWarning()
                    << "QProcess error: " << process.errorString();
        }

        return EXIT_FAILURE;
    }
    return process.exitCode();
}

const QProcessEnvironment RunEnvironment::getRunEnvironment() const
{
    d->checkProduct();
    EnvironmentScriptRunner(d->resolvedProduct.get(), &d->evalContext, d->environment)
            .setupForRun(d->setupRunEnvConfig);
    return d->resolvedProduct->runEnvironment;
}

const QProcessEnvironment RunEnvironment::getBuildEnvironment() const
{
    if (!d->resolvedProduct)
        return d->environment;
    EnvironmentScriptRunner(d->resolvedProduct.get(), &d->evalContext, d->environment)
            .setupForBuild();
    return d->resolvedProduct->buildEnvironment;
}

} // namespace qbs
