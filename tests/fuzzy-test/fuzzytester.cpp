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
#include "fuzzytester.h"

#include <QtCore/qdir.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qfile.h>
#include <QtCore/qprocess.h>
#include <QtCore/qsettings.h>

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <random>

static QString resolveIncrementalActivity() { return "resolve-incremental"; }
static QString buildIncrementalActivity() { return "build-incremental"; }
static QString buildFromScratchActivity() { return "build-from-scratch"; }

FuzzyTester::FuzzyTester()
{
    loadSettings();
}

FuzzyTester::~FuzzyTester()
{
    storeSettings();
}

void FuzzyTester::runTest(const QString &profile, const QString &startCommit,
                          int maxDurationInMinutes, int jobCount, bool log)
{
    m_profile = profile;
    m_jobCount = jobCount;
    m_log = log;

    runGit(QStringList() << "rev-parse" << "HEAD", &m_headCommit);
    qDebug("HEAD is %s", qPrintable(m_headCommit));

    qDebug("Trying to find a buildable commit to start with...");
    const QString workingStartCommit = findWorkingStartCommit(startCommit);
    qDebug("Found buildable start commit %s.", qPrintable(workingStartCommit));
    QStringList allCommits = findAllCommits(workingStartCommit);
    qDebug("The test set comprises all %d commits between the start commit and HEAD.",
           allCommits.size());

    // Shuffle the initial sequence. Otherwise all invocations of the tool with the same start
    // commit would try the same sequence of commits.
    std::srand(std::time(nullptr));
    std::shuffle(allCommits.begin(), allCommits.end(), std::mt19937(std::random_device()()));

    quint64 run = 0;
    QStringList buildSequence(workingStartCommit);
    QElapsedTimer timer;
    const qint64 maxDurationInMillis = maxDurationInMinutes * 60 * 1000;
    if (maxDurationInMillis != 0)
        timer.start();

    bool timerHasExpired = false;
    while (std::next_permutation(allCommits.begin(), allCommits.end()) && !timerHasExpired) {
        qDebug("Testing permutation %llu...", ++run);
        const auto &allCommitsImmutable = allCommits;
        for (const QString &currentCommit : allCommitsImmutable) {
           if (timer.isValid() && timer.hasExpired(maxDurationInMillis)) {
               timerHasExpired = true;
               break;
           }

            m_currentCommit = currentCommit;
            buildSequence << currentCommit;
            checkoutCommit(currentCommit);
            qDebug("Testing incremental build #%d (%s)", buildSequence.size() - 1,
                   qPrintable(currentCommit));

            // Doing "resolve" and "build" separately introduces additional possibilities
            // for errors, as information from change tracking has to be serialized correctly.
            QString qbsError;
            m_currentActivity = resolveIncrementalActivity();
            bool success = runQbs(defaultBuildDir(), QStringLiteral("resolve"), &qbsError);
            if (success) {
                m_currentActivity = buildIncrementalActivity();
                success = runQbs(defaultBuildDir(), QStringLiteral("build"), &qbsError);
            }
            m_currentActivity = buildFromScratchActivity();
            if (success) {
                if (!doCleanBuild(&qbsError)) {
                    QString message = "An incremental build succeeded "
                                      "with a commit for which a clean build failed.";
                    if (!m_log) {
                        message += QStringLiteral("\nThe qbs error message "
                                "for the clean build was: '%1'").arg(qbsError);
                    }
                    throwIncrementalBuildError(message, buildSequence);
                }
            } else {
                qDebug("Incremental build failed. Checking whether clean build works...");
                if (doCleanBuild()) {
                    QString message = "An incremental build failed "
                                      "with a commit for which a clean build succeeded.";
                    if (!m_log) {
                        message += QStringLiteral("\nThe qbs error message for "
                                "the incremental build was: '%1'").arg(qbsError);
                    }
                    throwIncrementalBuildError(message, buildSequence);
                } else {
                    qDebug("Clean build also fails. Continuing.");
                }
            }
        }
    }

    if (timerHasExpired)
        qDebug("Maximum duration reached.");
    else
        qDebug("All possible permutations were tried.");
}

void FuzzyTester::checkoutCommit(const QString &commit)
{
    runGit(QStringList() << "checkout" << commit);
    runGit(QStringList() << "submodule" << "update" << "--init");
}

QStringList FuzzyTester::findAllCommits(const QString &startCommit)
{
    QString allCommitsString;
    runGit(QStringList() << "log" << (startCommit + "~1.." + m_headCommit) << "--format=format:%h",
           &allCommitsString);
    return allCommitsString.simplified().split(QLatin1Char(' '),
#if (QT_VERSION < QT_VERSION_CHECK(5, 14, 0))
                                               QString::SkipEmptyParts);
#else
                                               Qt::SkipEmptyParts);
#endif
}

QString FuzzyTester::findWorkingStartCommit(const QString &startCommit)
{
    const QStringList allCommits = findAllCommits(startCommit);
    QString qbsError;
    m_currentActivity = buildFromScratchActivity();
    for (auto it = allCommits.crbegin(), end = allCommits.crend(); it != end; ++it) {
        m_currentCommit = *it;
        if (m_unbuildableCommits.contains(m_currentCommit)) {
            qDebug("Skipping known bad commit %s.", qPrintable(m_currentCommit));
            continue;
        }
        checkoutCommit(m_currentCommit);
        removeDir(defaultBuildDir());
        if (runQbs(defaultBuildDir(), QStringLiteral("build"), &qbsError)) {
            m_buildableCommits << m_currentCommit;
            return m_currentCommit;
        }
        qDebug("Commit %s is not buildable.", qPrintable(m_currentCommit));
        m_unbuildableCommits << m_currentCommit;
    }
    throw TestError(QStringLiteral("Cannot run test: Failed to find a single commit that "
            "builds successfully with qbs. The last qbs error was: '%1'").arg(qbsError));
}

void FuzzyTester::runGit(const QStringList &arguments, QString *output)
{
    QProcess git;
    git.start("git", arguments);
    if (!git.waitForStarted())
        throw TestError("Failed to start git. It is expected to be in the PATH.");
    if (!git.waitForFinished(300000) || git.exitStatus() != QProcess::NormalExit) // 5 minutes ought to be enough for everyone
        throw TestError(QStringLiteral("git failed: %1").arg(git.errorString()));
    if (git.exitCode() != 0) {
        throw TestError(QStringLiteral("git failed: %1")
                        .arg(QString::fromLocal8Bit(git.readAllStandardError())));
    }
    if (output)
        *output = QString::fromLocal8Bit(git.readAllStandardOutput()).trimmed();
}

bool FuzzyTester::runQbs(const QString &buildDir, const QString &command, QString *errorOutput)
{
    if (errorOutput)
        errorOutput->clear();
    QProcess qbs;
    QStringList commandLine = QStringList(command) << "-d" << buildDir;
    if (m_log) {
        commandLine << "-vv";
        const size_t maxLoggedCommits = 2;
        Q_ASSERT(m_commitsWithLogFiles.size() <= maxLoggedCommits + 1);
        if (m_commitsWithLogFiles.size() == maxLoggedCommits + 1) {
            static const QStringList allActivities = QStringList() << resolveIncrementalActivity()
                    << buildIncrementalActivity() << buildFromScratchActivity();
            const QString oldCommit = m_commitsWithLogFiles.front();
            m_commitsWithLogFiles.pop();
            for (const QString &a : allActivities)
                QFile::remove(logFilePath(oldCommit, a));
        }
        qbs.setStandardErrorFile(logFilePath(m_currentCommit, m_currentActivity));
        if (m_commitsWithLogFiles.empty() || m_commitsWithLogFiles.back() != m_currentCommit)
            m_commitsWithLogFiles.push(m_currentCommit);
    } else {
        commandLine << "-qq";
    }
    if (m_jobCount != 0)
        commandLine << "--jobs" << QString::number(m_jobCount);
    commandLine << ("profile:" + m_profile);
    qbs.start("qbs", commandLine);
    if (!qbs.waitForStarted()) {
        throw TestError(QStringLiteral("Failed to start qbs. It is expected to be "
                "in the PATH. QProcess error string: '%1'").arg(qbs.errorString()));
    }
    if (!qbs.waitForFinished(-1) || qbs.exitCode() != 0) {
        if (errorOutput)
            *errorOutput = QString::fromLocal8Bit(qbs.readAllStandardError());
        return false;
    }
    return true;
}

void FuzzyTester::removeDir(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.removeRecursively()) {
        throw TestError(QStringLiteral("Failed to remove temporary dir '%1'.")
                        .arg(dir.absolutePath()));
    }
}

bool FuzzyTester::doCleanBuild(QString *errorMessage)
{
    if (m_unbuildableCommits.contains(m_currentCommit)) {
        qDebug("Commit is known not to be buildable, not running qbs.");
        return false;
    }
    if (m_buildableCommits.contains(m_currentCommit)) {
        qDebug("Commit is known to be buildable, not running qbs.");
        return true;
    }
    const QString cleanBuildDir = "fuzzytest-verification-build";
    removeDir(cleanBuildDir);
    if (runQbs(cleanBuildDir, QStringLiteral("build"), errorMessage)) {
        m_buildableCommits << m_currentCommit;
        return true;
    }
    m_unbuildableCommits << m_currentCommit;
    return false;
}

void FuzzyTester::throwIncrementalBuildError(const QString &message,
                                             const QStringList &commitSequence)
{
    const QString commitSequenceString = commitSequence.join(QLatin1Char(','));
    throw TestError(QStringLiteral("Found qbs bug with incremental build!\n"
            "%1\n"
            "The sequence of commits was: %2.").arg(message, commitSequenceString));
}

QString FuzzyTester::logFilePath(const QString &commit, const QString &activity)
{
    return "log." + commit + '.' + activity;
}

QString FuzzyTester::defaultBuildDir()
{
    return "fuzzytest-build";
}


static QString organization() { return "QtProject"; }
static QString app() { return "qbs-fuzzy-tester"; }
static QString unbuildableCommitsKey() { return "unbuildable-commits"; }
static QString buildableCommitsKey() { return "buildable-commits"; }

void FuzzyTester::loadSettings()
{
    QSettings s(organization(), app());
    m_unbuildableCommits = s.value(unbuildableCommitsKey()).toStringList();
    m_buildableCommits = s.value(buildableCommitsKey()).toStringList();
}

void FuzzyTester::storeSettings() const
{
    QSettings s(organization(), app());
    s.setValue(unbuildableCommitsKey(), m_unbuildableCommits);
    s.setValue(buildableCommitsKey(), m_buildableCommits);
}
