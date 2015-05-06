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
#include "fuzzytester.h"

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>

static bool parseCommandLine(const QStringList &commandLine, QString &profile,
                             QString &startCommi, int &maxDuration, int &jobCount, bool &log);
static bool runTest(const QString &profile, const QString &startCommit, int maxDuration,
                    int jobCount, bool log);

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString profile;
    QString startCommit;
    int maxDuration;
    int jobCount;
    bool log;
    if (!parseCommandLine(app.arguments(), profile, startCommit, maxDuration, jobCount, log))
        return EXIT_FAILURE;
    if (!runTest(profile, startCommit, maxDuration, jobCount, log))
        return EXIT_FAILURE;
    std::cout << "Test finished successfully." << std::endl;
    return EXIT_SUCCESS;
}

bool parseCommandLine(const QStringList &commandLine, QString &profile, QString &startCommit,
                      int &maxDuration, int &jobCount, bool &log)
{
    CommandLineParser cmdParser;
    try {
        cmdParser.parse(commandLine);
    } catch (const ParseException &e) {
        std::cerr << "Invalid command line: " << qPrintable(e.errorMessage) << std::endl;
        std::cerr << "Usage:" << std::endl << qPrintable(cmdParser.usageString()) << std::endl;
        return false;
    }
    profile = cmdParser.profile();
    startCommit = cmdParser.startCommit();
    maxDuration = cmdParser.maxDurationInMinutes();
    jobCount = cmdParser.jobCount();
    log = cmdParser.log();
    return true;
}

bool runTest(const QString &profile, const QString &startCommit, int maxDuration, int jobCount,
             bool log)
{
    try {
        FuzzyTester().runTest(profile, startCommit, maxDuration, jobCount, log);
    } catch (const TestError &e) {
        std::cerr << qPrintable(e.errorMessage) << std::endl;
        return false;
    }
    return true;
}
