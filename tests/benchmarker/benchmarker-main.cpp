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

#include "benchmarker.h"
#include "commandlineparser.h"
#include "exception.h"

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>

using namespace qbsBenchmarker;

static QByteArray relativeChange(qint64 oldVal, qint64 newVal)
{
    QByteArray change = newVal == 0
            ? "0" : QByteArray::number(std::abs(newVal * 100 / oldVal - 100));
    change += " %";
    if (oldVal > newVal)
        change.prepend('-');
    else if (oldVal < newVal)
        change.prepend('+');
    return change;
}

static void printResults(Activity activity, const BenchmarkResults &results)
{
    std::cout << "========== Performance data for ";
    switch (activity) {
    case ActivityResolving:
        std::cout << "Resolving";
        break;
    case ActivityRuleExecution:
        std::cout << "Rule Execution";
        break;
    case ActivityNullBuild:
        std::cout << "Null Build";
        break;
    }
    std::cout << " ==========" << std::endl;
    const BenchmarkResult result = results.value(activity);
    const char * const indent = "    ";
    std::cout << indent << "Old instruction count: " << result.oldInstructionCount << std::endl;
    std::cout << indent << "New instruction count: " << result.newInstructionCount << std::endl;
    std::cout << indent << "Relative change: "
              << relativeChange(result.oldInstructionCount, result.newInstructionCount).constData()
              << std::endl;
    std::cout << indent << "Old peak memory usage: " << result.oldPeakMemoryUsage << " Bytes"
              << std::endl;
    std::cout << indent
              << "New peak memory usage: " << result.newPeakMemoryUsage << " Bytes" << std::endl;
    std::cout << indent << "Relative change: "
              << relativeChange(result.oldPeakMemoryUsage, result.newPeakMemoryUsage).constData()
              << std::endl;
}

static void printResults(Activities activities, const BenchmarkResults &results)
{
    if (activities & ActivityResolving)
        printResults(ActivityResolving, results);
    if (activities & ActivityRuleExecution)
        printResults(ActivityRuleExecution, results);
    if (activities & ActivityNullBuild)
        printResults(ActivityNullBuild, results);
}

int main(int argc, char *argv[])
{
    try {
        QCoreApplication app(argc, argv);
        CommandLineParser clParser;
        clParser.parse();
        Benchmarker benchmarker(clParser.activies(), clParser.oldCommit(), clParser.newCommit(),
                                clParser.testProjectFilePath(), clParser.qbsRepoDirPath());
        benchmarker.benchmark();
        printResults(clParser.activies(), benchmarker.results());
    } catch (const Exception &e) {
        std::cerr << qPrintable(e.description()) << std::endl;
        return EXIT_FAILURE;
    }
}
