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
#include <buildgraph/productinstaller.h>
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

#include <QtCore/qdir.h>
#include <QtCore/qprocess.h>
#include <QtCore/qprocess.h>
#include <QtCore/qscopedpointer.h>
#include <QtCore/qtemporaryfile.h>
#include <QtCore/qvariant.h>

#include <stdlib.h>

namespace qbs {
using namespace Internal;

class RunEnvironment::RunEnvironmentPrivate
{
public:
    RunEnvironmentPrivate(const ResolvedProductPtr &product, const InstallOptions &installOptions,
            const QProcessEnvironment &environment, Settings *settings, const Logger &logger)
        : resolvedProduct(product)
        , installOptions(installOptions)
        , environment(environment)
        , settings(settings)
        , logger(logger)
        , engine(this->logger, EvalContext::PropertyEvaluation)
    {
    }

    const ResolvedProductPtr resolvedProduct;
    InstallOptions installOptions;
    const QProcessEnvironment environment;
    Settings * const settings;
    Logger logger;
    ScriptEngine engine;
};

RunEnvironment::RunEnvironment(const ResolvedProductPtr &product,
        const InstallOptions &installOptions,
        const QProcessEnvironment &environment, Settings *settings, const Logger &logger)
    : d(new RunEnvironmentPrivate(product, installOptions, environment, settings, logger))
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

int RunEnvironment::runTarget(const QString &targetBin, const QStringList &arguments,
                              ErrorInfo *error)
{
    try {
        return doRunTarget(targetBin, arguments);
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
        return QProcessEnvironment();
    }
}

const QProcessEnvironment RunEnvironment::buildEnvironment(ErrorInfo *error) const
{
    try {
        return getBuildEnvironment();
    } catch (const ErrorInfo &e) {
        if (error)
            *error = e;
        return QProcessEnvironment();
    }
}

int RunEnvironment::doRunShell()
{
    d->resolvedProduct->setupBuildEnvironment(&d->engine, d->environment);

    const QString productId = d->resolvedProduct->name;
    d->logger.qbsInfo() << Tr::tr("Starting shell for target '%1'.").arg(productId);
    const QProcessEnvironment environment = d->resolvedProduct->buildEnvironment;
#if defined(Q_OS_LINUX)
    clearenv();
#endif
    for (const QString &key : environment.keys())
        qputenv(key.toLocal8Bit().constData(), environment.value(key).toLocal8Bit());
    QString command;
    QScopedPointer<QTemporaryFile> envFile;
    if (HostOsInfo::isWindowsHost()) {
        command = environment.value(QLatin1String("COMSPEC"));
        if (command.isEmpty())
            command = QLatin1String("cmd");
        const QString prompt = environment.value(QLatin1String("PROMPT"));
        command += QLatin1String(" /k prompt [qbs] ") + prompt;
    } else {
        const QVariantMap qbsProps = d->resolvedProduct->topLevelProject()->buildConfiguration()
                .value(QLatin1String("qbs")).toMap();
        const QString profileName = qbsProps.value(QLatin1String("profile")).toString();
        command = Preferences(d->settings, profileName).shell();
        if (command.isEmpty())
            command = environment.value(QLatin1String("SHELL"), QLatin1String("/bin/sh"));

        // Yes, we have to use this prcoedure. PS1 is not inherited from the environment.
        const QString prompt = QLatin1String("qbs ") + productId + QLatin1String(" $ ");
        envFile.reset(new QTemporaryFile);
        if (envFile->open()) {
            if (command.endsWith(QLatin1String("bash")))
                command += QLatin1String(" --posix"); // Teach bash some manners.
            const QString promptLine = QLatin1String("PS1='") + prompt + QLatin1String("'\n");
            envFile->write(promptLine.toLocal8Bit());
            envFile->close();
            qputenv("ENV", envFile->fileName().toLocal8Bit());
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
            .split(HostOsInfo::pathListSeparator(), QString::SkipEmptyParts);

    for (const QString &fileName : fileNames) {
        const QString exeFileName = HostOsInfo::appendExecutableSuffix(fileName);
        for (const QString &ppath : path) {
            const QString fullPath = ppath + QLatin1Char('/') + exeFileName;
            QFileInfo fi(fullPath);
            if (fi.exists() && fi.isFile() && fi.isExecutable())
                return QDir::cleanPath(fullPath);
        }
    }
    return QString();
}

int RunEnvironment::doRunTarget(const QString &targetBin, const QStringList &arguments)
{
    const QStringList targetOS = d->resolvedProduct->moduleProperties->qbsPropertyValue(
                QLatin1String("targetOS")).toStringList();
    const QStringList toolchain = d->resolvedProduct->moduleProperties->qbsPropertyValue(
                QLatin1String("toolchain")).toStringList();

    QString targetExecutable = targetBin;
    QStringList targetArguments = arguments;
    const QString completeSuffix = QFileInfo(targetBin).completeSuffix();

    if (targetOS.contains(QLatin1String("ios")) || targetOS.contains(QLatin1String("tvos"))) {
        const QString bundlePath = targetBin + QLatin1String("/..");

        if (QFileInfo(targetExecutable = findExecutable(QStringList()
                    << QStringLiteral("iostool"))).isExecutable()) {
            targetArguments = QStringList()
                    << QStringLiteral("-run")
                    << QStringLiteral("-bundle")
                    << QDir::cleanPath(bundlePath);
            if (!arguments.isEmpty())
                targetArguments << QStringLiteral("-extra-args") << arguments;
        } else if (QFileInfo(targetExecutable = findExecutable(QStringList()
                    << QStringLiteral("ios-deploy"))).isExecutable()) {
            targetArguments = QStringList()
                    << QStringLiteral("--no-wifi")
                    << QStringLiteral("--noninteractive")
                    << QStringLiteral("--bundle")
                    << QDir::cleanPath(bundlePath);
            if (!arguments.isEmpty())
                targetArguments << QStringLiteral("--args") << arguments;
        } else {
            d->logger.qbsLog(LoggerError)
                    << Tr::tr("No suitable deployment tools were found in the environment. "
                              "Consider installing ios-deploy.");
            return EXIT_FAILURE;
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
            targetExecutable = QLatin1String("wine");
        }
    }

    if (toolchain.contains(QLatin1String("mono"))) {
        targetArguments.prepend(targetExecutable);
        targetExecutable = QLatin1String("mono");
    }

    if (completeSuffix == QLatin1String("js")) {
        targetExecutable = d->resolvedProduct->moduleProperties->moduleProperty(
                    QLatin1String("nodejs"), QLatin1String("interpreterFilePath")).toString();
        if (targetExecutable.isEmpty())
            // The Node.js binary is called nodejs on Debian/Ubuntu-family operating systems due to
            // conflict with another package containing a binary named node
            targetExecutable = findExecutable(QStringList()
                                              << QLatin1String("nodejs")
                                              << QLatin1String("node"));
        targetArguments.prepend(targetBin);
    }

    // Only check if the target is executable if we're not running it through another
    // known application such as msiexec or wine, as we can't check in this case anyways
    QFileInfo fi(targetExecutable);
    if (targetBin == targetExecutable && (!fi.isFile() || !fi.isExecutable())) {
        d->logger.qbsLog(LoggerError) << Tr::tr("File '%1' is not an executable.")
                                .arg(QDir::toNativeSeparators(targetExecutable));
        return EXIT_FAILURE;
    }

    QProcessEnvironment env = d->environment;
    env.insert(QLatin1String("QBS_RUN_FILE_PATH"), targetBin);
    d->resolvedProduct->setupRunEnvironment(&d->engine, env);

    d->logger.qbsInfo() << Tr::tr("Starting target '%1'.").arg(QDir::toNativeSeparators(targetBin));
    QProcess process;
    process.setProcessEnvironment(d->resolvedProduct->runEnvironment);
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.start(targetExecutable, targetArguments);
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
    if (!d->resolvedProduct)
        return d->environment;
    d->resolvedProduct->setupRunEnvironment(&d->engine, d->environment);
    return d->resolvedProduct->runEnvironment;
}

const QProcessEnvironment RunEnvironment::getBuildEnvironment() const
{
    if (!d->resolvedProduct)
        return d->environment;
    d->resolvedProduct->setupBuildEnvironment(&d->engine, d->environment);
    return d->resolvedProduct->buildEnvironment;
}

} // namespace qbs
