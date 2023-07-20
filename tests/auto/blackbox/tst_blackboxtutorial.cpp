/****************************************************************************
**
** Copyright (C) 2024 Ivan Komissarov (abbapoh@gmail.com)
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
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

#include "tst_blackboxtutorial.h"

#include <QtCore/qdir.h>
#include <QtCore/qdiriterator.h>

static QStringList collectProjects(const QString &dirPath)
{
    QStringList result;
    QDir dir(dirPath);
    const auto subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto &subDir : subDirs) {
        const auto path = dir.filePath(subDir);
        if (!QFileInfo::exists(path + "/myproject.qbs"))
            continue;
        result.append(dir.relativeFilePath(path));
    }
    return result;
}

TestBlackboxTutorial::TestBlackboxTutorial()
    : TestBlackboxBase(SRCDIR "/../../../tutorial/", "blackbox-tutorial")
{}

void TestBlackboxTutorial::tutorial_data()
{
    QTest::addColumn<QString>("project");

    const auto projects = collectProjects(testDataDir);
    for (const auto &project : projects) {
        QTest::newRow(project.toUtf8().data()) << project;
    }
}

void TestBlackboxTutorial::tutorial()
{
    QFETCH(QString, project);

    QVERIFY(QDir::setCurrent(testDataDir + "/" + project));
    QCOMPARE(runQbs(), 0);
}

QTEST_MAIN(TestBlackboxTutorial)
