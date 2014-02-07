/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "commandlineparser.h"
#include "fuzzytester.h"

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>

static bool parseCommandLine(const QStringList &commandLine, QString &profile,
                             QString &startCommit);
static bool runTest(const QString &profile, const QString &startCommit);

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString profile;
    QString startCommit;
    if (!parseCommandLine(app.arguments(), profile, startCommit))
        return EXIT_FAILURE;
    if (!runTest(profile, startCommit))
        return EXIT_FAILURE;
    std::cout << "Test finished successfully." << std::endl;
    return EXIT_SUCCESS;
}

bool parseCommandLine(const QStringList &commandLine, QString &profile, QString &startCommit)
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
    return true;
}

bool runTest(const QString &profile, const QString &startCommit)
{
    try {
        FuzzyTester().runTest(profile, startCommit);
    } catch (const TestError &e) {
        std::cerr << qPrintable(e.errorMessage) << std::endl;
        return false;
    }
    return true;
}
