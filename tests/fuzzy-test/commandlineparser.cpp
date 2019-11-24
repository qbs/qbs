/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "commandlineparser.h"

#include <QtCore/qfileinfo.h>

#include <cctype>

static QString profileOption() { return "--profile"; }
static QString startCommitOption() { return "--start-commit"; }
static QString maxDurationoption() { return "--max-duration"; }
static QString jobCountOption() { return "--jobs"; }
static QString logOption() { return "--log"; }

CommandLineParser::CommandLineParser() = default;

void CommandLineParser::parse(const QStringList &commandLine)
{
    m_profile.clear();
    m_startCommit.clear();
    m_maxDuration = 0;
    m_jobCount = 0;
    m_log = false;
    m_commandLine = commandLine;
    Q_ASSERT(!m_commandLine.empty());
    m_command = m_commandLine.takeFirst();
    while (!m_commandLine.empty()) {
        const QString arg = m_commandLine.takeFirst();
        if (arg == profileOption())
            assignOptionArgument(arg, m_profile);
        else if (arg == startCommitOption())
            assignOptionArgument(arg, m_startCommit);
        else if (arg == jobCountOption())
            assignOptionArgument(arg, m_jobCount);
        else if (arg == maxDurationoption())
            parseDuration();
        else if (arg == logOption())
            m_log = true;
        else
            throw ParseException(QStringLiteral("Unknown parameter '%1'").arg(arg));
    }
    if (m_profile.isEmpty())
        throw ParseException("No profile given.");
    if (m_startCommit.isEmpty())
        throw ParseException("No start commit given.");
}

QString CommandLineParser::usageString() const
{
    return QStringLiteral("%1 %2 <profile> %3 <start commit> [%4 <duration>] "
                               "[%5 <job count>] [%6]")
            .arg(QFileInfo(m_command).fileName(), profileOption(), startCommitOption(),
                 maxDurationoption(), jobCountOption(), logOption());
}

void CommandLineParser::assignOptionArgument(const QString &option, QString &argument)
{
    if (m_commandLine.empty())
        throw ParseException(QStringLiteral("Option '%1' needs an argument.").arg(option));
    argument = m_commandLine.takeFirst();
    if (argument.isEmpty()) {
        throw ParseException(QStringLiteral("Argument for option '%1' must not be empty.")
                             .arg(option));
    }
}

void CommandLineParser::assignOptionArgument(const QString &option, int &argument)
{
    QString numberString;
    assignOptionArgument(option, numberString);
    bool ok;
    argument = numberString.toInt(&ok);
    if (!ok || argument <= 0) {
        throw ParseException(QStringLiteral("Invalid argument '%1' for option '%2'.")
                             .arg(numberString, option));
    }
}

void CommandLineParser::parseDuration()
{
    QString durationString;
    QString choppedDurationString;
    assignOptionArgument(maxDurationoption(), durationString);
    choppedDurationString = durationString;
    const char suffix = durationString.at(durationString.size() - 1).toLatin1();
    const bool hasSuffix = !std::isdigit(suffix);
    if (hasSuffix)
        choppedDurationString.chop(1);
    bool ok;
    m_maxDuration = choppedDurationString.toInt(&ok);
    if (!ok || m_maxDuration <= 0) {
        throw ParseException(QStringLiteral("Invalid duration argument '%1'.")
                             .arg(durationString));
    }
    if (hasSuffix) {
        switch (suffix) {
        case 'm': break;
        case 'd': m_maxDuration *= 24; // Fall-through.
        case 'h': m_maxDuration *= 60; break;
        default:
            throw ParseException(QStringLiteral("Invalid duration suffix '%1'.")
                                 .arg(suffix));
        }
    }
}
