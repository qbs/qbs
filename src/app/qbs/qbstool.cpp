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
#include "qbstool.h"

#include <tools/hostosinfo.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qprocess.h>

#include <iostream>

static QString toolPrefix() { return QStringLiteral("qbs-"); }
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
    const QStringList toolFileNames = QDir(qbsBinDir()).entryList(QStringList(toolPrefix()
            + QStringLiteral("*%1").arg(suffix)), QDir::Files, QDir::Name);
    QStringList toolNames;
    const int prefixLength = toolPrefix().size();
    for (const QString &toolFileName : toolFileNames) {
        toolNames << toolFileName.mid(prefixLength,
                                      toolFileName.size() - prefixLength - suffix.size());
    }
    return toolNames;
}
