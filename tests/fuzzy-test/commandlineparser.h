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
#ifndef QBS_FUZZYTEST_COMMANDLINEPARSER_H
#define QBS_FUZZYTEST_COMMANDLINEPARSER_H

#include <QStringList>

#include <exception>

class ParseException : public std::exception
{
public:
    ParseException(const QString &error) : errorMessage(error) { }
    ~ParseException() throw() {}

    QString errorMessage;

private:
    const char *what() const throw() { return qPrintable(errorMessage); }
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
    int m_maxDuration;
    int m_jobCount;
    bool m_log;
};

#endif // Include guard.
