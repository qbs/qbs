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
** a written agreement between you and The Qt Company.  For licensing terms and
** conditions see http://www.qt.io/terms-conditions.  For further information
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
#include "commandlineparser.h"

#include <QFileInfo>

#include <cctype>

static QString profileOption() { return "--profile"; }
static QString startCommitOption() { return "--start-commit"; }
static QString maxDurationoption() { return "--max-duration"; }
static QString jobCountOption() { return "--jobs"; }
static QString logOption() { return "--log"; }

CommandLineParser::CommandLineParser()
{
}

void CommandLineParser::parse(const QStringList &commandLine)
{
    m_profile.clear();
    m_startCommit.clear();
    m_maxDuration = 0;
    m_jobCount = 0;
    m_log = false;
    m_commandLine = commandLine;
    Q_ASSERT(!m_commandLine.isEmpty());
    m_command = m_commandLine.takeFirst();
    while (!m_commandLine.isEmpty()) {
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
            throw ParseException(QString::fromLocal8Bit("Unknown parameter '%1'").arg(arg));
    }
    if (m_profile.isEmpty())
        throw ParseException("No profile given.");
    if (m_startCommit.isEmpty())
        throw ParseException("No start commit given.");
}

QString CommandLineParser::usageString() const
{
    return QString::fromLocal8Bit("%1 %2 <profile> %3 <start commit> [%4 <duration>] "
                                  "[%5 <job count>] [%6]")
            .arg(QFileInfo(m_command).fileName(), profileOption(), startCommitOption(),
                 maxDurationoption(), jobCountOption(), logOption());
}

void CommandLineParser::assignOptionArgument(const QString &option, QString &argument)
{
    if (m_commandLine.isEmpty())
        throw ParseException(QString::fromLocal8Bit("Option '%1' needs an argument.").arg(option));
    argument = m_commandLine.takeFirst();
    if (argument.isEmpty()) {
        throw ParseException(QString::fromLocal8Bit("Argument for option '%1' must not be empty.")
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
        throw ParseException(QString::fromLocal8Bit("Invalid argument '%1' for option '%2'.")
                             .arg(numberString, option));
    }
}

void CommandLineParser::parseDuration()
{
    QString durationString;
    QString choppedDurationString;
    assignOptionArgument(maxDurationoption(), durationString);
    choppedDurationString = durationString;
    const char suffix = durationString.at(durationString.count() - 1).toLatin1();
    const bool hasSuffix = !std::isdigit(suffix);
    if (hasSuffix)
        choppedDurationString.chop(1);
    bool ok;
    m_maxDuration = choppedDurationString.toInt(&ok);
    if (!ok || m_maxDuration <= 0) {
        throw ParseException(QString::fromLocal8Bit("Invalid duration argument '%1'.")
                             .arg(durationString));
    }
    if (hasSuffix) {
        switch (suffix) {
        case 'm': break;
        case 'd': m_maxDuration *= 24; // Fall-through.
        case 'h': m_maxDuration *= 60; break;
        default:
            throw ParseException(QString::fromLocal8Bit("Invalid duration suffix '%1'.")
                                 .arg(suffix));
        }
    }
}
