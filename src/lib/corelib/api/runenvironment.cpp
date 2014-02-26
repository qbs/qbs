/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "runenvironment.h"

#include <api/projectdata.h>
#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/preferences.h>
#include <tools/propertyfinder.h>

#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopedPointer>
#include <QTemporaryFile>
#include <QVariantMap>

#include <stdlib.h>

namespace qbs {
using namespace Internal;

class RunEnvironment::RunEnvironmentPrivate
{
public:
    RunEnvironmentPrivate(const ResolvedProductPtr &product,
            const QProcessEnvironment &environment, Settings *settings, const Logger &logger)
        : engine(logger)
        , resolvedProduct(product)
        , environment(environment)
        , settings(settings)
        , logger(logger)
    {
    }

    ScriptEngine engine;
    const ResolvedProductPtr resolvedProduct;
    const QProcessEnvironment environment;
    Settings * const settings;
    Logger logger;
};

RunEnvironment::RunEnvironment(const ResolvedProductPtr &product,
        const QProcessEnvironment &environment, Settings *settings, const Logger &logger)
    : d(new RunEnvironmentPrivate(product, environment, settings, logger))
{
}

RunEnvironment::~RunEnvironment()
{
    delete d;
}

int RunEnvironment::runShell()
{
    d->resolvedProduct->setupBuildEnvironment(&d->engine, d->environment);

    const QString productId = d->resolvedProduct->name;
    d->logger.qbsInfo() << Tr::tr("Starting shell for target '%1'.").arg(productId);
    const QProcessEnvironment environment = d->resolvedProduct->buildEnvironment;
#if defined(Q_OS_LINUX)
    clearenv();
#endif
    foreach (const QString &key, environment.keys())
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

int RunEnvironment::runTarget(const QString &targetBin, const QStringList &arguments)
{
    const QStringList targetOS = PropertyFinder().propertyValue(
                d->resolvedProduct->moduleProperties->value(),
                QLatin1String("qbs"),
                QLatin1String("targetOS")).toStringList();

    QString targetExecutable = targetBin;
    QStringList targetArguments = arguments;
    const QString completeSuffix = QFileInfo(targetBin).completeSuffix();

    if (targetOS.contains(QLatin1String("windows"))) {
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

    // Only check if the target is executable if we're not running it through another
    // known application such as msiexec or wine, as we can't check in this case anyways
    if (targetBin == targetExecutable && !QFileInfo(targetExecutable).isExecutable()) {
        d->logger.qbsLog(LoggerError) << Tr::tr("File '%1' is not an executable.")
                                .arg(QDir::toNativeSeparators(targetExecutable));
        return EXIT_FAILURE;
    }

    d->resolvedProduct->setupRunEnvironment(&d->engine, d->environment);

    d->logger.qbsInfo() << Tr::tr("Starting target '%1'.").arg(QDir::toNativeSeparators(targetBin));
    QProcess process;
    process.setProcessEnvironment(d->resolvedProduct->runEnvironment);
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.start(targetExecutable, targetArguments);
    process.waitForFinished(-1);
    return process.exitCode();
}

} // namespace qbs
