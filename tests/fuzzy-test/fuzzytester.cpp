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
#include "fuzzytester.h"

#include <QDir>
#include <QProcess>

#include <algorithm>
#include <cstdlib>
#include <ctime>

FuzzyTester::FuzzyTester()
{
}

void FuzzyTester::runTest(const QString &profile, const QString &startCommit)
{
    m_profile = profile;

    runGit(QStringList() << "describe" << "HEAD", &m_headCommit);
    qDebug("HEAD is %s", qPrintable(m_headCommit));

    qDebug("Trying to find a buildable commit to start with...");
    const QString workingStartCommit = findWorkingStartCommit(startCommit);
    qDebug("Found buildable start commit %s.", qPrintable(workingStartCommit));
    QStringList allCommits = findAllCommits(workingStartCommit);
    qDebug("The test set comprises all %d commits between the start commit and HEAD.",
           allCommits.count());

    // Shuffle the initial sequence. Otherwise all invocations of the tool with the same start
    // commit would try the same sequence of commits.
    std::srand(std::time(0));
    std::random_shuffle(allCommits.begin(), allCommits.end());

    quint64 run = 0;
    QStringList buildSequence(workingStartCommit);

    // This is in effect an infinite loop for all but the tiniest commit sets.
    // That's not a problem -- if you want the application to finish early, just kill it.
    while (std::next_permutation(allCommits.begin(), allCommits.end())) {
        qDebug("Testing permutation %llu...", ++run);
        foreach (const QString &currentCommit, allCommits) {
            buildSequence << currentCommit;
            checkoutCommit(currentCommit);
            qDebug("Testing incremental build #%d (%s)", buildSequence.count() - 1,
                   qPrintable(currentCommit));
            QString qbsError;
            if (!runQbs(defaultBuildDir(), &qbsError)) {
                // An error could be due to the current commit being faulty. Check that it is
                // buildable on its own before reporting a qbs error.
                const QString otherDir = "fuzzytest-verification-build";
                removeDir(otherDir);
                if (runQbs(otherDir)) {
                    const QString buildSequenceString = buildSequence.join(QLatin1String(","));
                    throw TestError(QString::fromLocal8Bit("Found qbs bug with incremental build!\n"
                            "The error message was: '%1'\n"
                            "The sequence of commits was: %2.").arg(qbsError, buildSequenceString));
                }
            }
        }
    }
}

void FuzzyTester::checkoutCommit(const QString &commit)
{
    runGit(QStringList() << "checkout" << commit);
}

QStringList FuzzyTester::findAllCommits(const QString &startCommit)
{
    QString allCommitsString;
    runGit(QStringList() << "log" << (startCommit + "~1.." + m_headCommit) << "--format=format:%h",
           &allCommitsString);
    return allCommitsString.simplified().split(QLatin1Char(' '));
}

QString FuzzyTester::findWorkingStartCommit(const QString &startCommit)
{
    const QStringList allCommits = findAllCommits(startCommit);
    QString qbsError;
    for (int i = allCommits.count() - 1; i >= 0; --i) {
        QString currentCommit = allCommits.at(i);
        checkoutCommit(currentCommit);
        removeDir(defaultBuildDir());
        if (runQbs(defaultBuildDir(), &qbsError))
            return currentCommit;
        qDebug("Commit %s is not buildable.", qPrintable(currentCommit));
    }
    throw TestError(QString::fromLocal8Bit("Cannot run test: Failed to find a single commit that "
            "builds successfully with qbs. The last qbs error was: '%1'").arg(qbsError));
}

void FuzzyTester::runGit(const QStringList &arguments, QString *output)
{
    QProcess git;
    git.start("git", arguments);
    if (!git.waitForStarted())
        throw TestError("Failed to start git. It is expected to be in the PATH.");
    if (!git.waitForFinished(300000)) // 5 minutes ought to be enough for everyone
        throw TestError(QString::fromLocal8Bit("git failed: %1").arg(git.errorString()));
    if (output)
        *output = QString::fromLocal8Bit(git.readAllStandardOutput()).trimmed();
}

bool FuzzyTester::runQbs(const QString &buildDir, QString *errorOutput)
{
    if (errorOutput)
        errorOutput->clear();
    QProcess qbs;
    qbs.start("qbs", QStringList() << "-qq" << "-d" << buildDir << ("profile:" + m_profile));
    if (!qbs.waitForStarted())
        throw TestError("Failed to start qbs. It is expected to be in the PATH.");
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
        throw TestError(QString::fromLocal8Bit("Failed to remove temporary dir '%1'.")
                        .arg(dir.absolutePath()));
    }
}

QString FuzzyTester::defaultBuildDir()
{
    return "fuzzytest-build";
}
