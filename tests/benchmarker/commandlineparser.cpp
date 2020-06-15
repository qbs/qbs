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

#include "exception.h"

#include <QtCore/qcommandlineoption.h>
#include <QtCore/qcommandlineparser.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qfileinfo.h>

namespace qbsBenchmarker {

static QString resolveActivity() { return "resolving"; }
static QString ruleExecutionActivity() { return "rule-execution"; }
static QString nullBuildActivity() { return "null-build"; }
static QString allActivities() { return "all"; }

CommandLineParser::CommandLineParser() = default;

void CommandLineParser::parse()
{
    QCommandLineParser parser;
    parser.setApplicationDescription("This tool aims to detect qbs performance regressions "
                                     "using valgrind.");
    parser.addHelpOption();
    QCommandLineOption oldCommitOption(QStringList{"old-commit", "o"}, "The old qbs commit.",
                                       "old commit");
    parser.addOption(oldCommitOption);
    QCommandLineOption newCommitOption(QStringList{"new-commit", "n"}, "The new qbs commit.",
                                       "new commit");
    parser.addOption(newCommitOption);
    QCommandLineOption testProjectOption(QStringList{"test-project", "p"},
            "The example project to use for the benchmark.", "project file path");
    parser.addOption(testProjectOption);
    QCommandLineOption qbsRepoOption(QStringList{"qbs-repo", "r"}, "The qbs repository.",
                                     "repo path");
    parser.addOption(qbsRepoOption);
    QCommandLineOption activitiesOption(QStringList{"activities", "a"},
            QStringLiteral("The activities to benchmark. Possible values (CSV): %1,%2,%3,%4")
                    .arg(resolveActivity(), ruleExecutionActivity(), nullBuildActivity(),
                         allActivities()), "activities", allActivities());
    parser.addOption(activitiesOption);
    QCommandLineOption thresholdOption(QStringList{"regression-threshold", "t"},
            "A relative increase higher than this is considered a performance regression. "
            "All temporary data from running the benchmarks will be kept if that happens.",
            "value in per cent");
    parser.addOption(thresholdOption);
    parser.process(*QCoreApplication::instance());
    const QList<QCommandLineOption> mandatoryOptions = QList<QCommandLineOption>()
            << oldCommitOption << newCommitOption << testProjectOption << qbsRepoOption;
    for (const QCommandLineOption &o : mandatoryOptions) {
        if (!parser.isSet(o))
            throwException(o.names().constFirst(), parser.helpText());
        if (parser.value(o).isEmpty())
            throwException(o.names().constFirst(), QString(), parser.helpText());
    }
    m_oldCommit = parser.value(oldCommitOption);
    m_newCommit = parser.value(newCommitOption);
    if (m_oldCommit == m_newCommit) {
        throw Exception(QStringLiteral("Error parsing command line: "
                "'new commit' and 'old commit' must be different commits.\n%1").arg(parser.helpText()));
    }
    m_testProjectFilePath = parser.value(testProjectOption);
    m_qbsRepoDirPath = parser.value(qbsRepoOption);
    const QStringList activitiesList = parser.value(activitiesOption).split(',');
    m_activities = Activities();
    for (const QString &activityString : activitiesList) {
        if (activityString == allActivities()) {
            m_activities = ActivityResolving | ActivityRuleExecution | ActivityNullBuild;
            break;
        } else if (activityString == resolveActivity()) {
            m_activities = ActivityResolving;
        } else if (activityString == ruleExecutionActivity()) {
            m_activities |= ActivityRuleExecution;
        } else if (activityString == nullBuildActivity()) {
            m_activities |= ActivityNullBuild;
        } else {
            throwException(activitiesOption.names().constFirst(),
                           activityString,
                           parser.helpText());
        }
    }
    m_regressionThreshold = 5;
    if (parser.isSet(thresholdOption)) {
        bool ok = true;
        const QString rawThresholdValue = parser.value(thresholdOption);
        m_regressionThreshold = rawThresholdValue.toInt(&ok);
        if (!ok)
            throwException(thresholdOption.names().constFirst(),
                           rawThresholdValue,
                           parser.helpText());
    }
}

void CommandLineParser::throwException(const QString &optionName, const QString &illegalValue,
                                       const QString &helpText)
{
    const QString errorText(QStringLiteral("Error parsing command line: Illegal value '%1' "
            "for option '--%2'.\n%3").arg(illegalValue, optionName, helpText));
    throw Exception(errorText);
}

void CommandLineParser::throwException(const QString &missingOption, const QString &helpText)
{
    const QString errorText(QStringLiteral("Error parsing command line: Missing mandatory "
            "option '--%1'.\n%3").arg(missingOption, helpText));
    throw Exception(errorText);
}


} // namespace qbsBenchmarker
