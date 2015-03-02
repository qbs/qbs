/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/
#include "qbstool.h"

#include <tools/hostosinfo.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

#include <iostream>

static QString toolPrefix() { return QLatin1String("qbs-"); }
static QString qbsBinDir() { return QCoreApplication::applicationDirPath(); }

static QString qbsToolFilePath(const QString &toolName)
{
    return qbsBinDir() + QLatin1Char('/') + toolPrefix()
            + qbs::Internal::HostOsInfo::appendExecutableSuffix(toolName);
}

void QbsTool::runTool(const QString &toolName, const QStringList &arguments)
{
    m_failedToStart = false;
    m_exitCode = -1;
    const QString filePath = qbsToolFilePath(toolName);
    const QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile() || !fi.isExecutable()) {
        m_failedToStart = true;
        return;
    }
    QProcess toolProc;
    toolProc.start(filePath, arguments);
    if (!toolProc.waitForStarted())
        m_failedToStart = true;
    toolProc.waitForFinished(-1);
    m_exitCode = toolProc.exitCode();
    m_stdout = QString::fromLocal8Bit(toolProc.readAllStandardOutput());
    m_stderr = QString::fromLocal8Bit(toolProc.readAllStandardError());
}

bool QbsTool::tryToRunTool(const QString &toolName, const QStringList &arguments, int *exitCode)
{
    QbsTool tool;
    tool.runTool(toolName, arguments);
    if (exitCode)
        *exitCode = tool.exitCode();
    if (tool.failedToStart())
        return false;
    std::cout << qPrintable(tool.stdOut());
    std::cerr << qPrintable(tool.stdErr());
    return true;
}

QStringList QbsTool::allToolNames()
{
    const QString suffix = QLatin1String(QBS_HOST_EXE_SUFFIX);
    QStringList toolFileNames = QDir(qbsBinDir()).entryList(QStringList(toolPrefix()
            + QString::fromLocal8Bit("*%1").arg(suffix)), QDir::Files, QDir::Name);
    QStringList toolNames;
    const int prefixLength = toolPrefix().count();
    foreach (const QString &toolFileName, toolFileNames) {
        toolNames << toolFileName.mid(prefixLength,
                                      toolFileName.count() - prefixLength - suffix.count());
    }
    return toolNames;
}
