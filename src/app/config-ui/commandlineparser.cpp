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
#include "commandlineparser.h"

#include <logging/translator.h>
#include <tools/error.h>

#include <QtCore/qfileinfo.h>

using qbs::Internal::Tr;

static QString helpOptionShort() { return QStringLiteral("-h"); }
static QString helpOptionLong() { return QStringLiteral("--help"); }
static QString settingsDirOption() { return QStringLiteral("--settings-dir"); }
static QString systemOption() { return QStringLiteral("--system"); }

void CommandLineParser::parse(const QStringList &commandLine)
{
    m_commandLine = commandLine;
    Q_ASSERT(!m_commandLine.empty());
    m_command = QFileInfo(m_commandLine.takeFirst()).fileName();
    m_helpRequested = false;
    m_settingsDir.clear();

    if (m_commandLine.empty())
        return;
    const QString &arg = m_commandLine.front();
    if (arg == helpOptionShort() || arg == helpOptionLong()) {
        m_commandLine.removeFirst();
        m_helpRequested = true;
    } else if (arg == systemOption()) {
        m_commandLine.removeFirst();
        m_settingsScope = qbs::Settings::SystemScope;
    } else if (arg == settingsDirOption()) {
        m_commandLine.removeFirst();
        assignOptionArgument(settingsDirOption(), m_settingsDir);
    }

    if (!m_commandLine.empty())
        complainAboutExtraArguments();
}

void CommandLineParser::throwError(const QString &message)
{
    qbs::ErrorInfo error(Tr::tr("Syntax error: %1").arg(message));
    error.append(usageString());
    throw error;
}

QString CommandLineParser::usageString() const
{
    QString s = Tr::tr("This tool displays qbs settings in a GUI.\n"
                       "If you have more than a few settings, this might be preferable to "
                       "plain \"qbs config\", as it presents a hierarchical view.\n");
    s += Tr::tr("Usage:\n");
    s += Tr::tr("    %1 [%2 <settings directory>] [%5] [%3|%4]\n")
            .arg(m_command, settingsDirOption(), helpOptionShort(), helpOptionLong(),
                 systemOption());
    return s;
}

void CommandLineParser::assignOptionArgument(const QString &option, QString &argument)
{
    if (m_commandLine.empty())
        throwError(Tr::tr("Option '%1' needs an argument.").arg(option));
    argument = m_commandLine.takeFirst();
    if (argument.isEmpty())
        throwError(Tr::tr("Argument for option '%1' must not be empty.").arg(option));
}

void CommandLineParser::complainAboutExtraArguments()
{
    throwError(Tr::tr("Extraneous command-line arguments '%1'.")
               .arg(m_commandLine.join(QLatin1Char(' '))));
}
