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
#ifndef QBS_BENCHMARKER_BENCHMARKRUNNER_H
#define QBS_BENCHMARKER_BENCHMARKRUNNER_H

#include "activities.h"

#include <QList>
#include <QMutex>
#include <QString>

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
    ValgrindRunner(Activities activities, const QString &testProject, const QString &qbsBuildDir,
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
    QMutex m_resultsMutex;
};

} // namespace qbsBenchmarker

#endif // Include guard.
