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
#ifndef QBS_BENCHMARKER_BENCHMARKER_H
#define QBS_BENCHMARKER_BENCHMARKER_H

#include "activities.h"

#include <QtCore/qhash.h>
#include <QtCore/qstring.h>
#include <QtCore/qtemporarydir.h>

QT_BEGIN_NAMESPACE
class QStringList;
QT_END_NAMESPACE

namespace qbsBenchmarker {

class BenchmarkResult
{
public:
    qint64 oldInstructionCount;
    qint64 newInstructionCount;
    qint64 oldPeakMemoryUsage;
    qint64 newPeakMemoryUsage;
};
typedef QHash<Activity, BenchmarkResult> BenchmarkResults;

class Benchmarker
{
public:
    Benchmarker(Activities activities, const QString &oldCommit, const QString &newCommit,
                const QString &testProject, const QString &qbsRepo);
    ~Benchmarker();

    void benchmark();

    BenchmarkResults results() const { return m_results; }

private:
    void rememberCurrentRepoState();
    void buildQbs(const QString &buildDir) const;

    const Activities m_activities;
    const QString m_oldCommit;
    const QString m_newCommit;
    const QString m_testProject;
    const QString m_qbsRepo;
    QString m_commitToRestore;
    QTemporaryDir m_baseOutputDir;
    BenchmarkResults m_results;
};

} // namespace qbsBenchmarker

#endif // Include guard.
