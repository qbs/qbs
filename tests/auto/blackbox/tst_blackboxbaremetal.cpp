/****************************************************************************
**
** Copyright (C) 2020 Denis Shienkov <denis.shienkov@gmail.com>
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

#include "tst_blackboxbaremetal.h"

#include "../shared.h"

TestBlackboxBareMetal::TestBlackboxBareMetal()
    : TestBlackboxBase (SRCDIR "/testdata-baremetal", "blackbox-baremetal")
{
}

void TestBlackboxBareMetal::application_data()
{
    QTest::addColumn<QString>("testPath");
    QTest::newRow("one-object-application") << "/one-object-application";
    QTest::newRow("two-object-application") << "/two-object-application";
}

void TestBlackboxBareMetal::application()
{
    QFETCH(QString, testPath);
    QDir::setCurrent(testDataDir + testPath);
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxBareMetal::staticLibraryDependencies()
{
    QDir::setCurrent(testDataDir + "/static-library-dependencies");
    QCOMPARE(runQbs(QStringList{"-p", "lib-a,lib-b,lib-c,lib-d,lib-e"}), 0);
    QCOMPARE(runQbs(QStringList{"--command-echo-mode", "command-line"}), 0);
    const QByteArray output = m_qbsStdout + '\n' + m_qbsStderr;
    QVERIFY(output.contains("lib-a"));
    QVERIFY(output.contains("lib-b"));
    QVERIFY(output.contains("lib-c"));
    QVERIFY(output.contains("lib-d"));
    QVERIFY(output.contains("lib-e"));
}

void TestBlackboxBareMetal::userIncludePaths()
{
    QDir::setCurrent(testDataDir + "/user-include-paths");
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxBareMetal::systemIncludePaths()
{
    QDir::setCurrent(testDataDir + "/system-include-paths");
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxBareMetal::distributionIncludePaths()
{
    QDir::setCurrent(testDataDir + "/distribution-include-paths");
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxBareMetal::preincludeHeaders()
{
    QDir::setCurrent(testDataDir + "/preinclude-headers");
    QCOMPARE(runQbs(), 0);
}

QTEST_MAIN(TestBlackboxBareMetal)
