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

#include <app/qbs/parser/commandlineparser.h>
#include <app/shared/logging/consolelogger.h>
#include <tools/buildoptions.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>

#include <QDir>
#include <QTemporaryFile>
#include <QTest>

using namespace qbs;

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
        args << "--check-timestamps";
        args << "--check-outputs";
        CommandLineParser parser;

        QVERIFY(parser.parseCommandLine(args));
        QCOMPARE(ConsoleLogger::instance().logSink()->logLevel(), LoggerTrace);
        QCOMPARE(parser.command(), BuildCommandType);
        QCOMPARE(parser.products(), QStringList() << "blubb");
        QCOMPARE(parser.buildOptions(QString()).changedFiles().count(), 2);
        QVERIFY(parser.buildOptions(QString()).keepGoing());
        QVERIFY(parser.force());
        QVERIFY(parser.forceTimestampCheck());
        QVERIFY(parser.forceOutputCheck());
        QVERIFY(!parser.logTime());
        QCOMPARE(parser.buildConfigurations().count(), 1);

        QVERIFY(parser.parseCommandLine(QStringList() << "-vvvqqq" << fileArgs));
        QCOMPARE(ConsoleLogger::instance().logSink()->logLevel(), defaultLogLevel());
        QVERIFY(!parser.force());

        QVERIFY(parser.parseCommandLine(QStringList() << "-t" << fileArgs));
        QVERIFY(parser.logTime());

        if (!Internal::HostOsInfo::isWindowsHost()) { // Windows has no progress bar atm.
            // Note: We cannot just check for !parser.logTime() here, because if the test is not
            // run in a terminal, "--show-progress" is ignored, in which case "--log-time"
            // takes effect.
            QVERIFY(parser.parseCommandLine(QStringList() << "-t" << "--show-progress"
                                            << fileArgs));
            QVERIFY(parser.showProgress() != parser.logTime());
        }

        QVERIFY(parser.parseCommandLine(QStringList() << "-vvqqq" << fileArgs));
        QCOMPARE(ConsoleLogger::instance().logSink()->logLevel(), LoggerWarning);

        QVERIFY(parser.parseCommandLine(QStringList() << "-vvvqq" << fileArgs));
        QCOMPARE(ConsoleLogger::instance().logSink()->logLevel(), LoggerDebug);

        QVERIFY(parser.parseCommandLine(QStringList() << "--log-level" << "trace" << fileArgs));
        QCOMPARE(ConsoleLogger::instance().logSink()->logLevel(), LoggerTrace);

        // Second "global" profile overwrites first.
        QVERIFY(parser.parseCommandLine(QStringList(fileArgs) << "profile:a" << "profile:b"));
        QCOMPARE(parser.buildConfigurations().count(), 1);
        QCOMPARE(parser.buildConfigurations().first().value("qbs.profile").toString(), QLatin1String("b"));

        // Second build configuration-specific profile overwrites first.
        QVERIFY(parser.parseCommandLine(QStringList(fileArgs) << "debug" << "profile:a"
                                        << "profile:b"));
        QCOMPARE(parser.buildConfigurations().count(), 1);
        QCOMPARE(parser.buildConfigurations().first().value("qbs.profile").toString(), QLatin1String("b"));

        QVERIFY(parser.parseCommandLine(QStringList(fileArgs) << "a-debug" << "profile:a"
                                        << "b-debug" << "profile:b"));
        QCOMPARE(parser.buildConfigurations().count(), 2);
        QCOMPARE(parser.buildConfigurations().first().value("qbs.configurationName").toString(), QLatin1String("a-debug"));
        QCOMPARE(parser.buildConfigurations().first().value("qbs.profile").toString(), QLatin1String("a"));
        QCOMPARE(parser.buildConfigurations().at(1).value("qbs.configurationName").toString(), QLatin1String("b-debug"));
        QCOMPARE(parser.buildConfigurations().at(1).value("qbs.profile").toString(), QLatin1String("b"));

        // Redundant build request
        QVERIFY(parser.parseCommandLine(QStringList(fileArgs) << "debug" << "profile:a"
                                        << "debug" << "profile:a"));
        QCOMPARE(parser.buildConfigurations().count(), 1);

        QVERIFY(parser.parseCommandLine(QStringList(fileArgs) << "debug" << "profile:a"
                                        << "release" << "profile:b"));
        QCOMPARE(parser.buildConfigurations().count(), 2);
        QCOMPARE(parser.buildConfigurations().first().value("qbs.configurationName").toString(), QLatin1String("debug"));
        QCOMPARE(parser.buildConfigurations().first().value("qbs.profile").toString(), QLatin1String("a"));
        QCOMPARE(parser.buildConfigurations().at(1).value("qbs.configurationName").toString(), QLatin1String("release"));
        QCOMPARE(parser.buildConfigurations().at(1).value("qbs.profile").toString(), QLatin1String("b"));

        // Non-global property takes precedence.
        QVERIFY(parser.parseCommandLine(QStringList(fileArgs) << "profile:a" << "debug"
                                        << "profile:b"));
        QCOMPARE(parser.buildConfigurations().count(), 1);
        QCOMPARE(parser.buildConfigurations().first().value("qbs.configurationName").toString(), QLatin1String("debug"));
        QCOMPARE(parser.buildConfigurations().first().value("qbs.profile").toString(), QLatin1String("b"));
    }

    void testInvalidCommandLine()
    {
        QTemporaryFile projectFile;
        QVERIFY(projectFile.open());
        const QStringList fileArgs = QStringList() << "-f" << projectFile.fileName();
        CommandLineParser parser;
        QVERIFY(!parser.parseCommandLine(QStringList() << fileArgs << "-x")); // Unknown short option.
        QVERIFY(!parser.parseCommandLine(QStringList() << fileArgs << "--xyz")); // Unknown long option.
        QVERIFY(!parser.parseCommandLine(QStringList() << fileArgs << "-vjv")); // Invalid position.
        QVERIFY(!parser.parseCommandLine(QStringList() << fileArgs << "-j"));  // Missing argument.
        QVERIFY(!parser.parseCommandLine(QStringList() << "-j" << "0" << fileArgs)); // Wrong argument.
        QVERIFY(!parser.parseCommandLine(QStringList() << fileArgs << "--products"));  // Missing argument.
        QVERIFY(!parser.parseCommandLine(QStringList() << "--changed-files" << "," << fileArgs)); // Wrong argument.
        QVERIFY(!parser.parseCommandLine(QStringList() << "--log-level" << "blubb" << fileArgs)); // Wrong argument.
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
        QVERIFY(parser.parseCommandLine(args + QStringList(projectFilePath)));
        QCOMPARE(projectFilePath, parser.projectFilePath());
        projectFilePath = oneProjectDir + QLatin1String("/project.qbs");
        QVERIFY(parser.parseCommandLine(args + QStringList(oneProjectDir)));
        QCOMPARE(projectFilePath, parser.projectFilePath());
        QVERIFY(!parser.parseCommandLine(args + QStringList(noProjectsDir)));
        QVERIFY(!parser.parseCommandLine(args + QStringList(multiProjectsDir)));
    }
};

QTEST_MAIN(TestCmdLineParser)

#include "tst_cmdlineparser.moc"
