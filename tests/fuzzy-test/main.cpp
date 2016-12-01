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
#include "fuzzytester.h"

#include <QtCore/qcoreapplication.h>

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
