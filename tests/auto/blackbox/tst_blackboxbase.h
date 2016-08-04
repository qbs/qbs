/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
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

#ifndef TST_BLACKBOXBASE_H
#define TST_BLACKBOXBASE_H

#include <QMap>
#include <QProcessEnvironment>
#include <QStringList>

class QbsRunParameters
{
public:
    QbsRunParameters()
    {
        init();
    }

    QbsRunParameters(const QString &cmd, const QStringList &args = QStringList())
        : command(cmd), arguments(args)
    {
        init();
    }

    QbsRunParameters(const QStringList &args)
        : arguments(args)
    {
        init();
    }

    void init()
    {
        expectFailure = false;
        useProfile = true;
        environment = QProcessEnvironment::systemEnvironment();
    }

    QString command;
    QStringList arguments;
    QString buildDirectory;
    QProcessEnvironment environment;
    bool expectFailure;
    bool useProfile;
};

class TestBlackboxBase : public QObject
{
    Q_OBJECT
public:
    TestBlackboxBase(const QString &testDataSrcDir, const QString &testName);

public slots:
    void initTestCase();

protected:
    int runQbs(const QbsRunParameters &params = QbsRunParameters());
    void rmDirR(const QString &dir);
    static QByteArray unifiedLineEndings(const QByteArray &ba);
    static void sanitizeOutput(QByteArray *ba);
    static void ccp(const QString &sourceDirPath, const QString &targetDirPath);
    static QString findExecutable(const QStringList &fileNames);
    QMap<QString, QString> findJdkTools(int *status);
    QMap<QString, QString> findNodejs(int *status);
    QMap<QString, QString> findTypeScript(int *status);
    QString findArchiver(const QString &fileName, int *status = nullptr);

    const QString testDataDir;
    const QString testSourceDir;
    const QString qbsExecutableFilePath;
    const QString defaultInstallRoot;

    QByteArray m_qbsStderr;
    QByteArray m_qbsStdout;
};

#endif // TST_BLACKBOXBASE_H
