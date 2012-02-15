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

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtScript/QScriptEngine>

#include <stdio.h>
#include <stdlib.h>
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace qbs {

RunEnvironment::RunEnvironment(ResolvedProduct::Ptr product)
    : m_product(product)
{
}

int RunEnvironment::runShell()
{
    try {
        QScriptEngine engine;
        m_product->setupBuildEnvironment(&engine, QProcessEnvironment::systemEnvironment());
    } catch (Error &e) {
        fprintf(stderr, "%s", qPrintable(e.toString()));
        return 1;
    }

    QString productId = m_product->project->id + '/' + m_product->name;
    printf("Starting shell for target '%s'.\n", qPrintable(productId));
#ifdef Q_OS_UNIX
    Settings::Ptr settings = Settings::create();
    QByteArray shellProcess = settings->value("shell", "/bin/sh").toString().toLocal8Bit();
    QByteArray prompt = "PS1=qbs " + productId.toLocal8Bit() + " $ ";

    QStringList senv = m_product->buildEnvironment.toStringList();

    char *argv[2];
    argv[0] = shellProcess.data();
    argv[1] = 0;
    char **env = new char *[senv.count() + 1];
    int i = 0;
    foreach (const QString &s, senv){
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
    return 890;
#else
    foreach (const QString &s, m_product->buildEnvironment.toStringList()) {
        int idx = s.indexOf(QLatin1Char('='));
        qputenv(s.left(idx).toLocal8Bit(), s.mid(idx + 1, s.length() - idx - 1).toLocal8Bit());
    }
    QByteArray shellProcess = qgetenv("COMSPEC");
    if (shellProcess.isEmpty())
        shellProcess = "cmd";
    shellProcess.append(" /k prompt [qbs] " + qgetenv("PROMPT"));
    return system(shellProcess);
#endif
}

int RunEnvironment::runTarget(const QString &targetBin, const QStringList &arguments)
{
    if (!QFileInfo(targetBin).isExecutable()) {
        fprintf(stderr, "File '%s' is not an executable.\n", qPrintable(targetBin));
        return 1;
    }

    try {
        QScriptEngine engine;
        m_product->setupRunEnvironment(&engine, QProcessEnvironment::systemEnvironment());
    } catch (Error &e) {
        fprintf(stderr, "%s", qPrintable(e.toString()));
        return 1;
    }

    printf("Starting target '%s'.\n", qPrintable(QDir::toNativeSeparators(targetBin)));
    QProcess proc;
    proc.setProcessEnvironment(m_product->runEnvironment);
    proc.setProcessChannelMode(QProcess::ForwardedChannels);
    proc.start(targetBin, arguments);
    proc.waitForFinished(-1);
    return proc.exitCode();
}

} // namespace qbs
