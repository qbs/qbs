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


#include "runenvironment.h"

#include <tools/settings.h>

#include <QDebug>
#include <QDir>
#include <QProcess>
#include <QScriptEngine>

#include <stdio.h>
#include <stdlib.h>
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace Qbs {

RunEnvironment::RunEnvironment(const Qbs::Private::ResolvedProduct &resolvedproduct)
    : m_resolvedProduct(resolvedproduct)
{
}

RunEnvironment::RunEnvironment(const BuildProduct &buildProduct)
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

    if (exitCode == 0) {
        QString productId = m_resolvedProduct.productId();
        printf("Starting shell for target '%s'.\n", qPrintable(productId));
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
        qDebug("executing the shell failed");
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
        fprintf(stderr, "File '%s' is not an executable.\n", qPrintable(targetBin));
        exitCode = 1;
    }

    if (exitCode == 0) {
        exitCode = m_resolvedProduct.setupRunEnvironment();

        if (exitCode == 0) {
            printf("Starting target '%s'.\n", qPrintable(QDir::toNativeSeparators(targetBin)));
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

} // namespace Qbs
