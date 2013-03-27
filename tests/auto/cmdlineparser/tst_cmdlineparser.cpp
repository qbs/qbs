/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#include <app/qbs/parser/commandlineparser.h>
#include <app/shared/logging/consolelogger.h>
#include <app/shared/qbssettings.h>
#include <tools/buildoptions.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>

#include <QDir>
#include <QTemporaryFile>
#include <QTest>

using namespace qbs;

static SettingsPtr settings = qbsSettings();

class TestCmdLineParser : public QObject
{
    Q_OBJECT
public:
    TestCmdLineParser()
    {
        ConsoleLogger::instance().logSink()->setEnabled(false);
    }

private slots:
    void testValidCommandLine()
    {
        QTemporaryFile projectFile;
        QVERIFY(projectFile.open());
        const QStringList fileArgs = QStringList() << "-f" << projectFile.fileName();
        QStringList args;
        args.append("-vvk");
        args.append("-v");
        args << "--products" << "blubb";
        args << "--changed-files" << "foo,bar" << fileArgs;
        args << "--force";
        CommandLineParser parser;

        QVERIFY(parser.parseCommandLine(args, settings.data()));
        QCOMPARE(ConsoleLogger::instance().logSink()->logLevel(), LoggerTrace);
        QCOMPARE(parser.command(), BuildCommandType);
        QCOMPARE(parser.products(), QStringList() << "blubb");
        QCOMPARE(parser.buildOptions().changedFiles.count(), 2);
        QVERIFY(parser.buildOptions().keepGoing);
        QVERIFY(parser.force());
        QVERIFY(!parser.logTime());

        QVERIFY(parser.parseCommandLine(QStringList() << "-vvvqqq" << fileArgs, settings.data()));
        QCOMPARE(ConsoleLogger::instance().logSink()->logLevel(), defaultLogLevel());
        QVERIFY(!parser.force());

        QVERIFY(parser.parseCommandLine(QStringList() << "-t" << fileArgs, settings.data()));
        QVERIFY(parser.logTime());

        // Note: We cannot just check for !parser.logTime() here, because if the test is not
        // run in a terminal, "--show-progress" is ignored, in which case "--log-time" takes effect.
        QVERIFY(parser.parseCommandLine(QStringList() << "-t" << "--show-progress" << fileArgs,
                                        settings.data()));
        QVERIFY(parser.showProgress() != parser.logTime());

        QVERIFY(parser.parseCommandLine(QStringList() << "-vvqqq" << fileArgs, settings.data()));
        QCOMPARE(ConsoleLogger::instance().logSink()->logLevel(), LoggerWarning);

        QVERIFY(parser.parseCommandLine(QStringList() << "-vvvqq" << fileArgs, settings.data()));
        QCOMPARE(ConsoleLogger::instance().logSink()->logLevel(), LoggerDebug);

        QVERIFY(parser.parseCommandLine(QStringList() << "--log-level" << "trace" << fileArgs,
                                        settings.data()));
        QCOMPARE(ConsoleLogger::instance().logSink()->logLevel(), LoggerTrace);
    }

    void testInvalidCommandLine()
    {
        QTemporaryFile projectFile;
        QVERIFY(projectFile.open());
        const QStringList fileArgs = QStringList() << "-f" << projectFile.fileName();
        CommandLineParser parser;
        QVERIFY(!parser.parseCommandLine(QStringList() << "-x" << fileArgs, settings.data())); // Unknown short option.
        QVERIFY(!parser.parseCommandLine(QStringList() << "--xyz" << fileArgs, settings.data())); // Unknown long option.
        QVERIFY(!parser.parseCommandLine(QStringList() << "-vjv" << fileArgs, settings.data())); // Invalid position.
        QVERIFY(!parser.parseCommandLine(QStringList() << "-j" << fileArgs, settings.data()));  // Missing argument.
        QVERIFY(!parser.parseCommandLine(QStringList() << "-j" << "0" << fileArgs,
                                         settings.data())); // Wrong argument.
        QVERIFY(!parser.parseCommandLine(QStringList() << "--products" << fileArgs,
                                         settings.data()));  // Missing argument.
        QVERIFY(!parser.parseCommandLine(QStringList() << "--changed-files" << "," << fileArgs,
                                         settings.data())); // Wrong argument.
        QVERIFY(!parser.parseCommandLine(QStringList() << "--log-level" << "blubb" << fileArgs,
                                         settings.data())); // Wrong argument.
        QVERIFY(!parser.parseCommandLine(QStringList("properties") << fileArgs << "--force",
                settings.data())); // Invalid option for command.
    }

    void testProjectFileLookup()
    {
        const QString srcDir = QLatin1String(SRCDIR);
        const QString noProjectsDir = srcDir + QLatin1String("/data/dirwithnoprojects");
        const QString oneProjectDir = srcDir + QLatin1String("/data/dirwithoneproject");
        const QString multiProjectsDir = srcDir + QLatin1String("/data/dirwithmultipleprojects");
        QVERIFY(QDir(noProjectsDir).exists() && QDir(oneProjectDir).exists()
                && QDir(multiProjectsDir).exists());
        CommandLineParser parser;
        const QStringList args(QLatin1String("-f"));
        QString projectFilePath = multiProjectsDir + QLatin1String("/project.qbs");
        QVERIFY(parser.parseCommandLine(args + QStringList(projectFilePath), settings.data()));
        QCOMPARE(projectFilePath, parser.projectFilePath());
        projectFilePath = oneProjectDir + QLatin1String("/project.qbs");
        QVERIFY(parser.parseCommandLine(args + QStringList(oneProjectDir), settings.data()));
        QCOMPARE(projectFilePath, parser.projectFilePath());
        QVERIFY(!parser.parseCommandLine(args + QStringList(noProjectsDir), settings.data()));
        QVERIFY(!parser.parseCommandLine(args + QStringList(multiProjectsDir), settings.data()));
    }
};

QTEST_MAIN(TestCmdLineParser)

#include "tst_cmdlineparser.moc"
