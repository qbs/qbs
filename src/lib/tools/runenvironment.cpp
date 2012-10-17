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

#include "runenvironment.h"

#include <language/qbsengine.h>
#include <logging/logger.h>
#include <tools/hostosinfo.h>
#include <tools/settings.h>

#include <QDir>
#include <QProcess>
#include <QScopedPointer>
#include <QTemporaryFile>

#include <stdlib.h>

namespace qbs {

RunEnvironment::RunEnvironment(QbsEngine *engine, const ResolvedProduct::Ptr &product)
    : m_engine(engine)
    , m_resolvedProduct(product)
{
}

RunEnvironment::~RunEnvironment()
{
}

RunEnvironment::RunEnvironment(const RunEnvironment &other)
    : m_resolvedProduct(other.m_resolvedProduct)
{
}

RunEnvironment &RunEnvironment::operator =(const RunEnvironment &other)
{
    m_resolvedProduct = other.m_resolvedProduct;

    return *this;
}

int RunEnvironment::runShell()
{
    try {
        m_resolvedProduct->setupBuildEnvironment(m_engine, QProcessEnvironment::systemEnvironment());
    } catch (const Error &e) {
        qbsError() << e.toString();
        return EXIT_FAILURE;
    }

    const QString productId = m_resolvedProduct->name;
    qbsInfo() << tr("Starting shell for target '%1'.").arg(productId);
    const QProcessEnvironment environment = m_resolvedProduct->buildEnvironment;
#ifdef Q_OS_UNIX
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
        const Settings::Ptr settings = Settings::create();
        command = settings->value(QLatin1String("shell")).toString();
        if (command.isEmpty())
            command = environment.value(QLatin1String("SHELL"), QLatin1String("/bin/sh"));

        // Yes, we have to use this prcoedure. PS1 is not inherited from the environment.
        const QString prompt = QLatin1String("qbs ") + productId + QLatin1String(" $ ");
        envFile.reset(new QTemporaryFile);
        if (envFile->open()) {
            if (command.endsWith(QLatin1String("bash")))
                command += " --posix"; // Teach bash some manners.
            const QString promptLine = QLatin1String("PS1='") + prompt + QLatin1String("'\n");
            envFile->write(promptLine.toLocal8Bit());
            envFile->close();
            qputenv("ENV", envFile->fileName().toLocal8Bit());
        } else {
            qbsWarning() << tr("Setting custom shell prompt failed.");
        }
    }

    // We cannot use QProcess, since it does not do stdin forwarding.
    return system(command.toLocal8Bit().constData());
}

int RunEnvironment::runTarget(const QString &targetBin, const QStringList &arguments)
{
    if (!QFileInfo(targetBin).isExecutable()) {
        qbsError("File '%s' is not an executable.", qPrintable(targetBin));
        return EXIT_FAILURE;
    }

    try {
        m_resolvedProduct->setupRunEnvironment(m_engine, QProcessEnvironment::systemEnvironment());
    } catch (const Error &e) {
        qbsError() << e.toString();
        return EXIT_FAILURE;
    }

    qbsInfo("Starting target '%s'.", qPrintable(QDir::toNativeSeparators(targetBin)));
    QProcess process;
    process.setProcessEnvironment(m_resolvedProduct->runEnvironment);
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.start(targetBin, arguments);
    process.waitForFinished(-1);
    return process.exitCode();
}

} // namespace qbs
