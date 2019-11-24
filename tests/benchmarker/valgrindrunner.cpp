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
#include "valgrindrunner.h"

#include "exception.h"
#include "runsupport.h"

#include <QtCore/qbuffer.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfuture.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qxmlstream.h>

#include <QtConcurrent/qtconcurrentrun.h>

#include <deque>
#include <mutex>

namespace qbsBenchmarker {

ValgrindRunner::ValgrindRunner(Activities activities, QString testProject,
                               const QString &qbsBuildDir, const QString &baseOutputDir)
    : m_activities(activities)
    , m_testProject(std::move(testProject))
    , m_qbsBinary(qbsBuildDir + "/bin/qbs")
    , m_baseOutputDir(baseOutputDir)
{
    if (!QDir::root().mkpath(m_baseOutputDir))
        throw Exception(QStringLiteral("Failed to create directory '%1'.").arg(baseOutputDir));
}

void ValgrindRunner::run()
{
    std::deque<QFuture<void>> futures;
    if (m_activities & ActivityResolving)
        futures.push_back(QtConcurrent::run(this, &ValgrindRunner::traceResolving));
    if (m_activities & ActivityRuleExecution)
        futures.push_back(QtConcurrent::run(this, &ValgrindRunner::traceRuleExecution));
    if (m_activities & ActivityNullBuild)
        futures.push_back(QtConcurrent::run(this, &ValgrindRunner::traceNullBuild));
    while (!futures.empty()) {
        futures.front().waitForFinished();
        futures.pop_front();
    }
}

void ValgrindRunner::traceResolving()
{
    const QString buildDirCallgrind = m_baseOutputDir + "/build-dir.resolving.callgrind";
    const QString buildDirMassif = m_baseOutputDir + "/build-dir.resolving.massif";
    traceActivity(ActivityResolving, buildDirCallgrind, buildDirMassif);
}

void ValgrindRunner::traceRuleExecution()
{
    const QString buildDirCallgrind = m_baseOutputDir + "/build-dir.rule-execution.callgrind";
    const QString buildDirMassif = m_baseOutputDir + "/build-dir.rule-execution.massif";
    runProcess(qbsCommandLine("resolve", buildDirCallgrind, false));
    runProcess(qbsCommandLine("resolve", buildDirMassif, false));
    traceActivity(ActivityRuleExecution, buildDirCallgrind, buildDirMassif);
}

void ValgrindRunner::traceNullBuild()
{
    const QString buildDirCallgrind = m_baseOutputDir + "/build-dir.null-build.callgrind";
    const QString buildDirMassif = m_baseOutputDir + "/build-dir.null-build.massif";
    runProcess(qbsCommandLine("build", buildDirCallgrind, false));
    runProcess(qbsCommandLine("build", buildDirMassif, false));
    traceActivity(ActivityNullBuild, buildDirCallgrind, buildDirMassif);
}

void ValgrindRunner::traceActivity(Activity activity, const QString &buildDirCallgrind,
                                   const QString &buildDirMassif)
{
    QString activityString;
    QString qbsCommand;
    bool dryRun;
    switch (activity) {
    case ActivityResolving:
        activityString = "resolving";
        qbsCommand = "resolve";
        dryRun = false;
        break;
    case ActivityRuleExecution:
        activityString = "rule-execution";
        qbsCommand = "build";
        dryRun = true;
        break;
    case ActivityNullBuild:
        activityString = "null-build";
        qbsCommand = "build";
        dryRun = false;
        break;
    }

    const QString outFileCallgrind = m_baseOutputDir + "/outfile." + activityString + ".callgrind";
    const QString outFileMassif = m_baseOutputDir + "/outfile." + activityString + ".massif";
    QFuture<qint64> callGrindFuture = QtConcurrent::run(this, &ValgrindRunner::runCallgrind,
            qbsCommand, buildDirCallgrind, dryRun, outFileCallgrind);
    QFuture<qint64> massifFuture = QtConcurrent::run(this, &ValgrindRunner::runMassif, qbsCommand,
            buildDirMassif, dryRun, outFileMassif);
    callGrindFuture.waitForFinished();
    massifFuture.waitForFinished();
    addToResults(ValgrindResult(activity, callGrindFuture.result(), massifFuture.result()));
}

QStringList ValgrindRunner::qbsCommandLine(const QString &command, const QString &buildDir,
                                           bool dryRun) const
{
    QStringList commandLine = QStringList() << m_qbsBinary << command << "-qq" << "-d" << buildDir
                                            << "-f" << m_testProject;
    if (dryRun)
        commandLine << "--dry-run";
    return commandLine;
}

QStringList ValgrindRunner::wrapForValgrind(const QStringList &commandLine, const QString &tool,
                                            const QString &outFile) const
{
    return QStringList() << "valgrind" << "--smc-check=all" << "--trace-children=yes"
                         << ("--tool=" + tool) << ("--" + tool + "-out-file=" + outFile)
                         << commandLine;
}

QStringList ValgrindRunner::valgrindCommandLine(const QString &qbsCommand, const QString &buildDir,
        bool dryRun, const QString &tool, const QString &outFile) const
{
    return wrapForValgrind(qbsCommandLine(qbsCommand, buildDir, dryRun), tool, outFile);
}

void ValgrindRunner::addToResults(const ValgrindResult &result)
{
    std::lock_guard<std::mutex> locker(m_resultsMutex);
    m_results.push_back(result);
}

qint64 ValgrindRunner::runCallgrind(const QString &qbsCommand, const QString &buildDir,
                                     bool dryRun, const QString &outFile)
{
    runProcess(valgrindCommandLine(qbsCommand, buildDir, dryRun, "callgrind", outFile));
    QFile f(outFile);
    if (!f.open(QIODevice::ReadOnly)) {
        throw Exception(QStringLiteral("Failed to open file '%1': %2")
                        .arg(outFile, f.errorString()));
    }
    while (!f.atEnd()) {
        const QByteArray line = f.readLine().trimmed();
        static const QByteArray magicString = "summary: ";
        if (!line.startsWith(magicString))
            continue;
        const QByteArray icString = line.mid(magicString.size());
        bool ok;
        const qint64 iCount = icString.toLongLong(&ok);
        if (!ok) {
            throw Exception(QStringLiteral("Unexpected line in callgrind output file "
                                                "'%1': '%2'.")
                            .arg(outFile, QString::fromLocal8Bit(line)));
        }
        return iCount;
    }

    throw Exception(QStringLiteral("Failed to find summary line in callgrind "
                                        "output file '%1'.").arg(outFile));
}

qint64 ValgrindRunner::runMassif(const QString &qbsCommand, const QString &buildDir, bool dryRun,
                                  const QString &outFile)
{
    runProcess(valgrindCommandLine(qbsCommand, buildDir, dryRun, "massif", outFile));
    QByteArray ms_printOutput;
    runProcess(QStringList() << "ms_print" << outFile, QString(), &ms_printOutput);
    QBuffer buffer(&ms_printOutput);
    buffer.open(QIODevice::ReadOnly);
    QByteArray peakSnapshot;
    const QString exceptionStringPattern = QStringLiteral("Failed to extract peak memory "
            "usage from file '%1': %2").arg(outFile);
    while (!buffer.atEnd()) {
        const QByteArray line = buffer.readLine();
        static const QByteArray magicString = " (peak)";
        const int magicStringOffset = line.indexOf(magicString);
        if (magicStringOffset == -1)
            continue;
        int delimiterOffset = line.lastIndexOf(',', magicStringOffset);
        if (delimiterOffset == -1)
            delimiterOffset = line.lastIndexOf('[', magicStringOffset);
        if (delimiterOffset == -1) {
            const QString details = QStringLiteral("Failed to extract peak snapshot from "
                    "line '%1'.").arg(QString::fromLocal8Bit(line));
            throw Exception(exceptionStringPattern.arg(details));
        }
        peakSnapshot = line.mid(delimiterOffset + 1, magicStringOffset - delimiterOffset).trimmed();
        break;
    }
    if (peakSnapshot.isEmpty())
        throw Exception(exceptionStringPattern.arg("No peak marker found"));
    while (!buffer.atEnd()) {
        const QByteArray line = buffer.readLine().simplified();
        if (!line.startsWith(peakSnapshot + ' '))
            continue;
        const QList<QByteArray> entries = line.split(' ');
        if (entries.size() != 6) {
            const QString details = QStringLiteral("Expected 6 entries in line '%1', but "
                    "there are %2.").arg(QString::fromLocal8Bit(line)).arg(entries.size());
            throw Exception(exceptionStringPattern.arg(details));
        }
        QByteArray peakMemoryString = entries.at(2);
        peakMemoryString.replace(',', QByteArray());
        bool ok;
        qint64 peakMemoryUsage = peakMemoryString.toLongLong(&ok);
        if (!ok) {
            const QString details = QStringLiteral("Failed to parse peak memory value '%1' "
                    "as a number.").arg(QString::fromLocal8Bit(peakMemoryString));
            throw Exception(exceptionStringPattern.arg(details));
        }
        return peakMemoryUsage;
    }

    const QString details = QStringLiteral("Failed to find snapshot '%1'.")
            .arg(QString::fromLocal8Bit(peakSnapshot));
    throw Exception(exceptionStringPattern.arg(details));
}

} // namespace qbsBenchmarker
