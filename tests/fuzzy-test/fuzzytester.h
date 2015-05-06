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
#ifndef QBS_FUZZYTESTER_H
#define QBS_FUZZYTESTER_H

#include <QQueue>
#include <QStringList>

#include <exception>

class TestError {
public:
    TestError(const QString &errorMessage) : errorMessage(errorMessage) {}
    ~TestError() throw() {}

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
    int m_jobCount;
    bool m_log;
    QString m_headCommit;
    QString m_currentCommit;
    QString m_currentActivity;
    QQueue<QString> m_commitsWithLogFiles;
    QStringList m_unbuildableCommits;
    QStringList m_buildableCommits;
};

#endif // Include guard.
