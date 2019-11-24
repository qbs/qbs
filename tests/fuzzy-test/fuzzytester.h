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
#ifndef QBS_FUZZYTESTER_H
#define QBS_FUZZYTESTER_H

#include <QtCore/qstringlist.h>

#include <exception>
#include <queue>

class TestError {
public:
    TestError(QString errorMessage) : errorMessage(std::move(errorMessage)) {}
    ~TestError() throw() = default;

    QString errorMessage;

private:
    const char *what() const throw() { return qPrintable(errorMessage); }
};


class FuzzyTester
{
public:
    FuzzyTester();
    ~FuzzyTester();

    void runTest(const QString &profile, const QString &startCommit, int maxDurationInMinutes,
                 int jobCount, bool log);

private:
    void checkoutCommit(const QString &commit);
    QStringList findAllCommits(const QString &startCommit);
    QString findWorkingStartCommit(const QString &startCommit);
    void runGit(const QStringList &arguments, QString *output = 0);
    bool runQbs(const QString &buildDir, const QString &command, QString *errorOutput = 0);
    void removeDir(const QString &dir);
    bool doCleanBuild(QString *errorMessage = 0);
    void throwIncrementalBuildError(const QString &message, const QStringList &commitSequence);

    void loadSettings();
    void storeSettings() const;

    static QString logFilePath(const QString &commit, const QString &activity);
    static QString defaultBuildDir();

    QString m_profile;
    int m_jobCount = 0;
    bool m_log = false;
    QString m_headCommit;
    QString m_currentCommit;
    QString m_currentActivity;
    std::queue<QString> m_commitsWithLogFiles;
    QStringList m_unbuildableCommits;
    QStringList m_buildableCommits;
};

#endif // Include guard.
