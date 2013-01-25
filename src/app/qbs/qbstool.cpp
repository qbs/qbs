/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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
#include "qbstool.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcess>

#include <iostream>

static QString toolPrefix() { return QLatin1String("qbs-"); }
static QString qbsBinDir() { return QCoreApplication::applicationDirPath(); }

static QString qbsToolFilePath(const QString &toolName)
{
    return qbsBinDir() + QLatin1Char('/') + toolPrefix() + toolName;
}

void QbsTool::runTool(const QString &toolName, const QStringList &arguments)
{
    m_failedToStart = m_failedToFinish = false;
    m_exitCode = -1;
    QProcess toolProc;
    toolProc.start(qbsToolFilePath(toolName), arguments);
    if (!toolProc.waitForStarted())
        m_failedToStart = true;
     if (!toolProc.waitForFinished())
        m_failedToFinish = true;
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
    QStringList toolFileNames = QDir(qbsBinDir()).entryList(QStringList(toolPrefix()
            + QLatin1Char('*')), QDir::Files, QDir::Name);
    QStringList toolNames;
    const int prefixLength = toolPrefix().count();
    foreach (const QString &toolFileName, toolFileNames)
        toolNames << toolFileName.mid(prefixLength);
    return toolNames;
}
