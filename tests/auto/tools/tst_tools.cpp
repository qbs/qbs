/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#include <tools/logger.h>
#include <tools/fileinfo.h>
#include <tools/options.h>
#include <QDir>
#include <QtTest>

class TestFileInfo : public QObject
{
    Q_OBJECT
private slots:
    void testCommandLineOptions()
    {
        QStringList args;
        args.append("-vvv");
        args.append("-k");
        qbs::CommandLineOptions options;
        options.readCommandLineArguments(args);
        QCOMPARE(qbs::Logger::instance().level(), qbs::LoggerInfo);
        QCOMPARE(options.command(), qbs::CommandLineOptions::BuildCommand);
        QVERIFY(options.isKeepGoingSet());
    }

    void testFileInfo()
    {
        QCOMPARE(qbs::FileInfo::fileName("C:/waffl/copter.exe"), QString("copter.exe"));
        QCOMPARE(qbs::FileInfo::baseName("C:/waffl/copter.exe.lib"), QString("copter"));
        QCOMPARE(qbs::FileInfo::completeBaseName("C:/waffl/copter.exe.lib"), QString("copter.exe"));
        QCOMPARE(qbs::FileInfo::path("abc"), QString("."));
        QCOMPARE(qbs::FileInfo::path("/abc/lol"), QString("/abc"));
        QVERIFY(!qbs::FileInfo::isAbsolute("bla/lol"));
        QVERIFY(qbs::FileInfo::isAbsolute("/bla/lol"));
#ifdef Q_OS_WIN
        QVERIFY(qbs::FileInfo::isAbsolute("C:\\bla\\lol"));
#endif
        QCOMPARE(qbs::FileInfo::resolvePath("/abc/lol", "waffl"), QString("/abc/lol/waffl"));
    }

    void testProjectFileLookup()
    {
        const QString srcDir = QLatin1String(SRCDIR);
        const QString noProjectsDir = srcDir + QLatin1String("data/dirwithnoprojects");
        const QString oneProjectDir = srcDir + QLatin1String("data/dirwithoneproject");
        const QString multiProjectsDir = srcDir + QLatin1String("data/dirwithmultipleprojects");
        Q_ASSERT(QDir(noProjectsDir).exists() && QDir(oneProjectDir).exists()
                && QDir(multiProjectsDir).exists());
        qbs::CommandLineOptions options;
        const QStringList args(QLatin1String("-f"));
        QString projectFilePath = multiProjectsDir + QLatin1String("/project.qbp");
        QVERIFY(options.readCommandLineArguments(args + QStringList(projectFilePath)));
        QCOMPARE(projectFilePath, options.projectFileName());
        projectFilePath = oneProjectDir + QLatin1String("/project.qbp");
        QVERIFY(options.readCommandLineArguments(args + QStringList(oneProjectDir)));
        QCOMPARE(projectFilePath, options.projectFileName());
        QVERIFY(!options.readCommandLineArguments(args + QStringList(noProjectsDir)));
        QVERIFY(!options.readCommandLineArguments(args + QStringList(multiProjectsDir)));
    }
};

QTEST_APPLESS_MAIN(TestFileInfo)

#include "tst_tools.moc"
