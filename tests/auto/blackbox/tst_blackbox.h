/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#ifndef TST_BLACKBOX_H
#define TST_BLACKBOX_H

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QtTest>

class TestBlackbox : public QObject
{
    Q_OBJECT
    const QString testDataDir;
    const QString testSourceDir;
    const QString qbsExecutableFilePath;
    const QString buildProfile;
    const QString buildDir;
    const QString buildGraphPath;

public:
    TestBlackbox();

protected:
    int runQbs(QStringList arguments = QStringList(), bool expectFailure = false);
    void rmDirR(const QString &dir);
    void touch(const QString &fn);

public slots:
    void initTestCase();
    void init();
    void cleanup();

private slots:
    void build_project_data();
    void build_project();
    void build_project_dry_run_data();
    void build_project_dry_run();
    void track_qrc();
    void track_qobject_change();
    void trackAddFile();
    void trackRemoveFile();
    void trackAddFileTag();
    void trackRemoveFileTag();
    void trackAddMocInclude();
    void wildcardRenaming();
    void recursiveRenaming();
    void recursiveWildcards();
    void updateTimestamps();
};

#endif // TST_BLACKBOX_H
