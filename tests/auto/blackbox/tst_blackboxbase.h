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
#ifndef TST_BLACKBOXBASE_H
#define TST_BLACKBOXBASE_H

#include "../shared.h"

#include <QtCore/qmap.h>
#include <QtCore/qprocess.h>
#include <QtCore/qstringlist.h>

class QbsRunParameters
{
public:
    QbsRunParameters()
    {
        init();
    }

    QbsRunParameters(QString cmd, QStringList args = QStringList())
        : command(std::move(cmd)), arguments(std::move(args))
    {
        init();
    }

    QbsRunParameters(QStringList args)
        : arguments(std::move(args))
    {
        init();
    }

    void init()
    {
        expectFailure = false;
        expectCrash = false;
        profile = profileName();
        settingsDir = settings()->baseDirectory();
        environment = QProcessEnvironment::systemEnvironment();
    }

    QString command;
    QStringList arguments;
    QString buildDirectory;
    QProcessEnvironment environment;
    QString profile;
    QString settingsDir;
    QString workingDir;
    bool expectFailure = false;
    bool expectCrash = false;
};

class TestBlackboxBase : public QObject
{
    Q_OBJECT
public:
    TestBlackboxBase(const QString &testDataSrcDir, const QString &testName);

public slots:
    virtual void initTestCase();

protected:
    virtual void validateTestProfile();

    void setNeedsQt() { m_needsQt = true; }
    int runQbs(const QbsRunParameters &params = QbsRunParameters());
    void rmDirR(const QString &dir);
    static QByteArray unifiedLineEndings(const QByteArray &ba);
    static void sanitizeOutput(QByteArray *ba);
    static void ccp(const QString &sourceDirPath, const QString &targetDirPath);
    static QString findExecutable(const QStringList &fileNames);
    QMap<QString, QString> findJdkTools(int *status);
    static qbs::Version qmakeVersion(const QString &qmakeFilePath);

    const QString testDataDir;
    const QString testSourceDir;
    const QString qbsExecutableFilePath;
    const QString defaultInstallRoot;

    QByteArray m_qbsStderr;
    QByteArray m_qbsStdout;
    int m_needsQt = false;
};

#endif // TST_BLACKBOXBASE_H
