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

#include "benchmarker.h"
#include "commandlineparser.h"
#include "exception.h"

#include <QtCore/qcoreapplication.h>

#include <cstdlib>
#include <iostream>

using namespace qbsBenchmarker;

static bool hasRegression = false;

static int relativeChange(qint64 oldVal, qint64 newVal)
{
    return newVal == 0 ? 0 : newVal * 100 / oldVal - 100;
}

static QByteArray relativeChangeString(int change)
{
    QByteArray changeString = QByteArray::number(change);
    changeString += " %";
    if (change > 0)
        changeString.prepend('+');
    return changeString;
}

static void printResults(Activity activity, const BenchmarkResults &results,
                         int regressionThreshold)
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
    int change = relativeChange(result.oldInstructionCount, result.newInstructionCount);
    if (change > regressionThreshold)
        hasRegression = true;
    std::cout << indent << "Relative change: "
              << relativeChangeString(change).constData()
              << std::endl;
    std::cout << indent << "Old peak memory usage: " << result.oldPeakMemoryUsage << " Bytes"
              << std::endl;
    std::cout << indent
              << "New peak memory usage: " << result.newPeakMemoryUsage << " Bytes" << std::endl;
    change = relativeChange(result.oldPeakMemoryUsage, result.newPeakMemoryUsage);
    if (change > regressionThreshold)
        hasRegression = true;
    std::cout << indent << "Relative change: "
              << relativeChangeString(change).constData()
              << std::endl;
}

static void printResults(Activities activities, const BenchmarkResults &results,
                         int regressionThreshold)
{
    if (activities & ActivityResolving)
        printResults(ActivityResolving, results, regressionThreshold);
    if (activities & ActivityRuleExecution)
        printResults(ActivityRuleExecution, results, regressionThreshold);
    if (activities & ActivityNullBuild)
        printResults(ActivityNullBuild, results, regressionThreshold);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    CommandLineParser clParser;
    try {
        clParser.parse();
    } catch (const Exception &e) {
        std::cerr << qPrintable(e.description()) << std::endl;
        return EXIT_FAILURE;
    }

    Benchmarker benchmarker(clParser.activies(), clParser.oldCommit(), clParser.newCommit(),
                            clParser.testProjectFilePath(), clParser.qbsRepoDirPath());
    try {
        benchmarker.benchmark();
        printResults(clParser.activies(), benchmarker.results(), clParser.regressionThreshold());
        if (hasRegression) {
            benchmarker.keepRawData();
            std::cout << "Performance regression detected. Raw benchmarking data available "
                         "under " << qPrintable(benchmarker.rawDataBaseDir()) << '.' << std::endl;
        }
    } catch (const Exception &e) {
        benchmarker.keepRawData();
        std::cerr << qPrintable(e.description()) << std::endl;
        std::cerr << "Build data available under " << qPrintable(benchmarker.rawDataBaseDir())
                  << '.' << std::endl;
        return EXIT_FAILURE;
    }
}
