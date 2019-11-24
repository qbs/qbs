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
#ifndef QBS_FUZZYTEST_COMMANDLINEPARSER_H
#define QBS_FUZZYTEST_COMMANDLINEPARSER_H

#include <QtCore/qstringlist.h>

#include <exception>

class ParseException : public std::exception
{
public:
    ParseException(QString error) : errorMessage(std::move(error)) { }
    ~ParseException() throw() override = default;

    QString errorMessage;

private:
    const char *what() const throw() override { return qPrintable(errorMessage); }
};

class CommandLineParser
{
public:
    CommandLineParser();

    void parse(const QStringList &commandLine);

    QString profile() const { return m_profile; }
    QString startCommit() const { return m_startCommit; }
    int maxDurationInMinutes() const { return m_maxDuration; }
    int jobCount() const { return m_jobCount; }
    bool log() const { return m_log; }

    QString usageString() const;

private:
    void assignOptionArgument(const QString &option, QString &argument);
    void assignOptionArgument(const QString &option, int &argument);
    void parseDuration();

    QStringList m_commandLine;
    QString m_command;
    QString m_profile;
    QString m_startCommit;
    int m_maxDuration = 0;
    int m_jobCount = 0;
    bool m_log = false;
};

#endif // Include guard.
