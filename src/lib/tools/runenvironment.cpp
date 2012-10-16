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

#include <logging/logger.h>
#include <tools/settings.h>

#include <QDir>
#include <QProcess>
#include <QScriptEngine>

#include <stdio.h>
#include <stdlib.h>

// TODO: Not needed (see below)
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace qbs {

RunEnvironment::RunEnvironment(const Qbs::Private::ResolvedProduct &resolvedproduct)
    : m_resolvedProduct(resolvedproduct)
{
}

RunEnvironment::RunEnvironment(const Qbs::BuildProduct &buildProduct)
    : m_resolvedProduct(buildProduct.privateResolvedProject())
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
    int exitCode = m_resolvedProduct.setupBuildEnvironment();

    // TODO: Just use QProcess here in both cases. No need for platform-dependent methods.
    if (exitCode == 0) {
        QString productId = m_resolvedProduct.productId();
        qbs::qbsInfo("Starting shell for target '%s'.", qPrintable(productId));
#ifdef Q_OS_UNIX
        qbs::Settings::Ptr settings = qbs::Settings::create();
        QByteArray shellProcess = settings->value("shell", "/bin/sh").toString().toLocal8Bit();
        QByteArray prompt = "PS1=qbs " + productId.toLocal8Bit() + " $ ";

        QStringList environmentStringList = m_resolvedProduct.buildEnvironmentStringList();

        char *argv[2];
        argv[0] = shellProcess.data();
        argv[1] = 0;
        char **env = new char *[environmentStringList.count() + 1];
        int i = 0;
        foreach (const QString &s, environmentStringList){
            QByteArray b = s.toLocal8Bit();
            if (b.startsWith("PS1")) {
                b = prompt;
            }
            env[i] = (char*)malloc(b.size() + 1);
            memcpy(env[i], b.data(), b.size());
            env[i][b.size()] = 0;
            ++i;
        }
        env[i] = 0;
        execve(argv[0], argv, env);
        qbs::qbsError("Executing the shell failed.");
        exitCode = 890;
#else
        foreach (const QString &s, m_resolvedProduct.buildEnvironmentStringList()) {
            int idx = s.indexOf(QLatin1Char('='));
            qputenv(s.left(idx).toLocal8Bit(), s.mid(idx + 1, s.length() - idx - 1).toLocal8Bit());
        }
        QByteArray shellProcess = qgetenv("COMSPEC");
        if (shellProcess.isEmpty())
            shellProcess = "cmd";
        shellProcess.append(" /k prompt [qbs] " + qgetenv("PROMPT"));
        exitCode = system(shellProcess);
#endif
    }

    return exitCode;
}

int RunEnvironment::runTarget(const QString &targetBin, const QStringList &arguments)
{
    int exitCode = 0;

    if (!QFileInfo(targetBin).isExecutable()) {
        qbs::qbsError("File '%s' is not an executable.", qPrintable(targetBin));
        exitCode = 1;
    }

    if (exitCode == 0) {
        exitCode = m_resolvedProduct.setupRunEnvironment();

        if (exitCode == 0) {
            qbs::qbsInfo("Starting target '%s'.", qPrintable(QDir::toNativeSeparators(targetBin)));
            QProcess process;
            process.setProcessEnvironment(m_resolvedProduct.runEnvironment());
            process.setProcessChannelMode(QProcess::ForwardedChannels);
            process.start(targetBin, arguments);
            process.waitForFinished(-1);
            exitCode = process.exitCode();
        }
    }

    return exitCode;
}

} // namespace qbs
