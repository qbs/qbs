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
#ifndef QBS_BENCHMARKER_BENCHMARKRUNNER_H
#define QBS_BENCHMARKER_BENCHMARKRUNNER_H

#include "activities.h"

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

#include <mutex>

QT_BEGIN_NAMESPACE
class QStringList;
QT_END_NAMESPACE

namespace qbsBenchmarker {

class ValgrindResult
{
public:
    ValgrindResult(Activity a, qint64 ic, qint64 mem)
        : activity(a), instructionCount(ic), peakMemoryUsage(mem) {}

    Activity activity;
    qint64 instructionCount;
    qint64 peakMemoryUsage;
};

class ValgrindRunner
{
public:
    ValgrindRunner(Activities activities, QString testProject, const QString &qbsBuildDir,
                    const QString &baseOutputDir);

    void run();
    QList<ValgrindResult> results() const { return m_results; }

private:
    void traceResolving();
    void traceRuleExecution();
    void traceNullBuild();
    void traceActivity(Activity activity, const QString &buildDirCallgrind,
                       const QString &buildDirMassif);
    QStringList qbsCommandLine(const QString &command, const QString &buildDir, bool dryRun) const;
    QStringList wrapForValgrind(const QStringList &commandLine, const QString &tool,
                                const QString &outFile) const;
    QStringList valgrindCommandLine(const QString &qbsCommand, const QString &buildDir, bool dryRun,
            const QString &tool, const QString &outFile) const;
    void addToResults(const ValgrindResult &results);
    qint64 runCallgrind(const QString &qbsCommand, const QString &buildDir, bool dryRun,
                         const QString &outFile);
    qint64 runMassif(const QString &qbsCommand, const QString &buildDir, bool dryRun,
                      const QString &outFile);

    const Activities m_activities;
    const QString m_testProject;
    const QString m_qbsBinary;
    const QString m_baseOutputDir;
    QList<ValgrindResult> m_results;
    std::mutex m_resultsMutex;
};

} // namespace qbsBenchmarker

#endif // Include guard.
