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

CommandLineParser::CommandLineParser()
{
}

void CommandLineParser::parse()
{
    QCommandLineParser parser;
    parser.setApplicationDescription("This tool aims to detect qbs performance regressions "
                                     "using valgrind.");
    parser.addHelpOption();
    QCommandLineOption oldCommitOption("old-commit", "The old qbs commit.", "old commit");
    parser.addOption(oldCommitOption);
    QCommandLineOption newCommitOption("new-commit", "The new qbs commit.", "new commit");
    parser.addOption(newCommitOption);
    QCommandLineOption testProjectOption("test-project",
            "The example project to use for the benchmark.", "project file path");
    parser.addOption(testProjectOption);
    QCommandLineOption qbsRepoOption("qbs-repo", "The qbs repository.", "repo path");
    parser.addOption(qbsRepoOption);
    QCommandLineOption activitiesOption("activities",
            QString::fromLatin1("The activities to benchmark. Possible values (CSV): %1,%2,%3,%4")
                    .arg(resolveActivity(), ruleExecutionActivity(), nullBuildActivity(),
                         allActivities()), "activities", allActivities());
    parser.addOption(activitiesOption);
    parser.process(*QCoreApplication::instance());
    QList<QCommandLineOption> mandatoryOptions = QList<QCommandLineOption>()
            << oldCommitOption << newCommitOption << testProjectOption << qbsRepoOption;
    foreach (const QCommandLineOption &o, mandatoryOptions) {
        if (!parser.isSet(o))
            throwException(o.names().first(), parser.helpText());
        if (parser.value(o).isEmpty())
            throwException(o.names().first(), QString(), parser.helpText());
    }
    m_oldCommit = parser.value(oldCommitOption);
    m_newCommit = parser.value(newCommitOption);
    m_testProjectFilePath = parser.value(testProjectOption);
    m_qbsRepoDirPath = parser.value(qbsRepoOption);
    const QStringList activitiesList = parser.value(activitiesOption).split(',');
    m_activities = 0;
    foreach (const QString &activityString, activitiesList) {
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
            throwException(activitiesOption.names().first(), activityString, parser.helpText());
        }
    }
}

void CommandLineParser::throwException(const QString &optionName, const QString &illegalValue,
                                       const QString &helpText)
{
    const QString errorText(QString::fromLatin1("Error parsing command line: Illegal value '%1' "
            "for option '--%2'.\n%3").arg(illegalValue, optionName, helpText));
    throw Exception(errorText);
}

void CommandLineParser::throwException(const QString &missingOption, const QString &helpText)
{
    const QString errorText(QString::fromLatin1("Error parsing command line: Missing mandatory "
            "option '--%1'.\n%3").arg(missingOption, helpText));
    throw Exception(errorText);
}


} // namespace qbsBenchmarker
