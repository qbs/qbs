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
static QString detectOption() { return QStringLiteral("--detect"); }
static QString typeOption() { return QStringLiteral("--type"); }
static QString settingsDirOption() { return QStringLiteral("--settings-dir"); }
static QString systemOption() { return QStringLiteral("--system"); }

void CommandLineParser::parse(const QStringList &commandLine)
{
    m_commandLine = commandLine;
    Q_ASSERT(!m_commandLine.empty());
    m_command = QFileInfo(m_commandLine.takeFirst()).fileName();
    m_helpRequested = false;
    m_autoDetectionMode = false;
    m_compilerPath.clear();
    m_toolchainType.clear();
    m_profileName.clear();
    m_settingsDir.clear();

    if (m_commandLine.empty())
        throwError(Tr::tr("No command-line arguments provided."));

    while (!m_commandLine.empty()) {
        const QString arg = m_commandLine.front();
        if (!arg.startsWith(QLatin1Char('-')))
            break;
        m_commandLine.removeFirst();
        if (arg == helpOptionShort() || arg == helpOptionLong())
            m_helpRequested = true;
        else if (arg == detectOption())
            m_autoDetectionMode = true;
        else if (arg == systemOption())
            m_settingsScope = qbs::Settings::SystemScope;
        else if (arg == typeOption())
            assignOptionArgument(typeOption(), m_toolchainType);
        else if (arg == settingsDirOption())
            assignOptionArgument(settingsDirOption(), m_settingsDir);
    }

    if (m_helpRequested || m_autoDetectionMode) {
        if (!m_commandLine.empty())
            complainAboutExtraArguments();
        return;
    }

    switch (m_commandLine.size()) {
    case 0:
    case 1:
        throwError(Tr::tr("Not enough command-line arguments provided."));
    case 2:
        m_compilerPath = m_commandLine.at(0);
        m_profileName = m_commandLine.at(1);
        m_profileName.replace(QLatin1Char('.'), QLatin1Char('-'));
        break;
    default:
        complainAboutExtraArguments();
    }
}

void CommandLineParser::throwError(const QString &message)
{
    qbs::ErrorInfo error(Tr::tr("Syntax error: %1").arg(message));
    error.append(usageString());
    throw error;
}

QString CommandLineParser::usageString() const
{
    QString s = Tr::tr("This tool creates qbs profiles from toolchains.\n");
    s += Tr::tr("Usage:\n");
    s += Tr::tr("    %1 [%2 <settings directory>] [%4] %3\n")
            .arg(m_command, settingsDirOption(), detectOption(), systemOption());
    s += Tr::tr("    %1 [%3 <settings directory>] [%4] [%2 <toolchain type>] "
                "<compiler path> <profile name>\n")
            .arg(m_command, typeOption(), settingsDirOption(), systemOption());
    s += Tr::tr("    %1 %2|%3\n").arg(m_command, helpOptionShort(), helpOptionLong());
    s += Tr::tr("The first form tries to auto-detect all known toolchains, looking them up "
                "via the PATH environment variable.\n");
    s += Tr::tr("The second form creates one profile for one toolchain. It will attempt "
                "to find out the toolchain type automatically.\nIn case the compiler has "
                "an unusual file name, you may need to provide the '--type' option.");
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
