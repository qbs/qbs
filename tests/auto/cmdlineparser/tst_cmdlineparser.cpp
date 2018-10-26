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
#include <tools/hostosinfo.h>

#include <QtCore/qdir.h>
#include <QtCore/qregexp.h>
#include <QtCore/qtemporaryfile.h>

#include <QtTest/qtest.h>

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
    void initTestCase()
    {
        QVERIFY(m_projectFile.open());
        m_fileArgs = QStringList() << "-f" << m_projectFile.fileName();
    }

    void testValidCommandLine()
    {
        QStringList args;
        args.push_back("-vvk");
        args.push_back("-v");
        args << "--products" << "blubb";
        args << "--changed-files" << "foo,bar" << m_fileArgs;
        args << "--check-timestamps";
        args << "--check-outputs";
        CommandLineParser parser;

        QVERIFY(parser.parseCommandLine(args));
        QCOMPARE(ConsoleLogger::instance().logSink()->logLevel(), LoggerTrace);
        QCOMPARE(parser.command(), BuildCommandType);
        QCOMPARE(parser.products(), QStringList() << "blubb");
        QCOMPARE(parser.buildOptions(QString()).changedFiles().size(), 2);
        QVERIFY(parser.buildOptions(QString()).keepGoing());
        QVERIFY(parser.forceTimestampCheck());
        QVERIFY(parser.forceOutputCheck());
        QVERIFY(!parser.logTime());
        QCOMPARE(parser.buildConfigurations().size(), 1);

        QVERIFY(parser.parseCommandLine(QStringList() << "-vvvqqq" << m_fileArgs));
        QCOMPARE(ConsoleLogger::instance().logSink()->logLevel(), defaultLogLevel());

        QVERIFY(parser.parseCommandLine(QStringList() << "-t" << m_fileArgs));
        QVERIFY(parser.logTime());

        // Note: We cannot just check for !parser.logTime() here, because if the test is not
        // run in a terminal, "--show-progress" is ignored, in which case "--log-time"
        // takes effect.
        QVERIFY(parser.parseCommandLine(QStringList() << "-t" << "--show-progress" << m_fileArgs));
        QVERIFY(parser.showProgress() != parser.logTime());

        QVERIFY(parser.parseCommandLine(QStringList() << "-vvqqq" << m_fileArgs));
        QCOMPARE(ConsoleLogger::instance().logSink()->logLevel(), LoggerWarning);

        QVERIFY(parser.parseCommandLine(QStringList() << "-vvvqq" << m_fileArgs));
        QCOMPARE(ConsoleLogger::instance().logSink()->logLevel(), LoggerDebug);

        QVERIFY(parser.parseCommandLine(QStringList() << "--log-level" << "trace" << m_fileArgs));
        QCOMPARE(ConsoleLogger::instance().logSink()->logLevel(), LoggerTrace);

        // Second "global" profile overwrites first.
        QVERIFY(parser.parseCommandLine(QStringList() << "profile:a" << m_fileArgs << "profile:b"));
        QCOMPARE(parser.buildConfigurations().size(), 1);
        QCOMPARE(parser.buildConfigurations().front().value("qbs.profile").toString(), QLatin1String("b"));

        // Second build configuration-specific profile overwrites first.
        QVERIFY(parser.parseCommandLine(QStringList(m_fileArgs) << "config:debug" << "profile:a"
                                        << "profile:b"));
        QCOMPARE(parser.buildConfigurations().size(), 1);
        QCOMPARE(parser.buildConfigurations().front().value("qbs.profile").toString(), QLatin1String("b"));

        QVERIFY(parser.parseCommandLine(QStringList(m_fileArgs) << "config:a-debug" << "profile:a"
                                        << "config:b-debug" << "profile:b"));
        QCOMPARE(parser.buildConfigurations().size(), 2);
        QCOMPARE(parser.buildConfigurations().front().value("qbs.configurationName").toString(), QLatin1String("a-debug"));
        QCOMPARE(parser.buildConfigurations().front().value("qbs.profile").toString(), QLatin1String("a"));
        QCOMPARE(parser.buildConfigurations().at(1).value("qbs.configurationName").toString(), QLatin1String("b-debug"));
        QCOMPARE(parser.buildConfigurations().at(1).value("qbs.profile").toString(), QLatin1String("b"));

        // Redundant build request
        QVERIFY(parser.parseCommandLine(QStringList(m_fileArgs) << "config:debug" << "profile:a"
                                        << "config:debug" << "profile:a"));
        QCOMPARE(parser.buildConfigurations().size(), 1);

        QVERIFY(parser.parseCommandLine(QStringList() << "config:debug" << "profile:a"
                                        << "config:release" << "profile:b" << m_fileArgs));
        QCOMPARE(parser.buildConfigurations().size(), 2);
        QCOMPARE(parser.buildConfigurations().front().value("qbs.configurationName").toString(), QLatin1String("debug"));
        QCOMPARE(parser.buildConfigurations().front().value("qbs.profile").toString(), QLatin1String("a"));
        QCOMPARE(parser.buildConfigurations().at(1).value("qbs.configurationName").toString(), QLatin1String("release"));
        QCOMPARE(parser.buildConfigurations().at(1).value("qbs.profile").toString(), QLatin1String("b"));

        // Non-global property takes precedence.
        QVERIFY(parser.parseCommandLine(QStringList(m_fileArgs) << "profile:a" << "config:debug"
                                        << "profile:b"));
        QCOMPARE(parser.buildConfigurations().size(), 1);
        QCOMPARE(parser.buildConfigurations().front().value("qbs.configurationName").toString(), QLatin1String("debug"));
        QCOMPARE(parser.buildConfigurations().front().value("qbs.profile").toString(), QLatin1String("b"));

        // Digits are always handled as option parameters.
        QVERIFY(parser.parseCommandLine(QStringList(m_fileArgs) << "-j" << "123"));
        QCOMPARE(parser.buildOptions(QString()).maxJobCount(), 123);
        QVERIFY(parser.parseCommandLine(QStringList(m_fileArgs) << "-j123"));
        QCOMPARE(parser.buildOptions(QString()).maxJobCount(), 123);

        // Argument list separation for the "run" command.
        QVERIFY(parser.parseCommandLine(QStringList("run") << m_fileArgs << "config:custom"
                                        << "-j123"));
        QCOMPARE(parser.command(), RunCommandType);
        QCOMPARE(parser.buildOptions(QString()).maxJobCount(), 123);
        QCOMPARE(parser.buildConfigurations().front().value("qbs.configurationName").toString(),
                 QLatin1String("custom"));
        QVERIFY(parser.runArgs().empty());
        QVERIFY(parser.parseCommandLine(QStringList("run") << m_fileArgs << "-j" << "123" << "--"
                << "config:custom"));
        QCOMPARE(parser.command(), RunCommandType);
        QCOMPARE(parser.buildOptions(QString()).maxJobCount(), 123);
        QCOMPARE(parser.buildConfigurations().front().value("qbs.configurationName").toString(),
                 QLatin1String("default"));
        QCOMPARE(parser.runArgs(), QStringList({"config:custom"}));

        // show-version
        QVERIFY(parser.parseCommandLine(QStringList("show-version")));
        QVERIFY(parser.showVersion());
        QVERIFY(parser.parseCommandLine(QStringList("--version")));
        QVERIFY(parser.showVersion());
        QVERIFY(parser.parseCommandLine(QStringList("-V")));
        QVERIFY(parser.showVersion());

        QVERIFY(parser.parseCommandLine(QStringList{"run", "--setup-run-env-config", "x,y,z"}));
        QCOMPARE(parser.runEnvConfig(), QStringList({"x", "y", "z"}));
    }

    void testInvalidCommandLine()
    {
        QFETCH(QStringList, commandLine);
        CommandLineParser parser;
        QVERIFY(!parser.parseCommandLine(commandLine));
    }

    void testInvalidCommandLine_data()
    {
        QTest::addColumn<QStringList>("commandLine");
        QTest::newRow("Unknown short option") << (QStringList() << m_fileArgs << "-x");
        QTest::newRow("Unknown long option") << (QStringList() << m_fileArgs << "--xyz");
        QTest::newRow("Invalid position") << (QStringList() << m_fileArgs << "-vjv");
        QTest::newRow("Missing jobs argument") << (QStringList() << m_fileArgs << "-j");
        QTest::newRow("Missing products argument") << (QStringList() << m_fileArgs << "--products");
        QTest::newRow("Wrong argument") << (QStringList() << "-j" << "0" << m_fileArgs);
        QTest::newRow("Invalid list argument")
                << (QStringList() << "--changed-files" << "," << m_fileArgs);
        QTest::newRow("Invalid log level")
                << (QStringList() << "--log-level" << "blubb" << m_fileArgs);
        QTest::newRow("Unknown numeric argument") << (QStringList() << m_fileArgs << "-123");
        QTest::newRow("Unknown parameter") << (QStringList() << m_fileArgs << "debug");
        QTest::newRow("Too many arguments") << (QStringList("help") << "build" << "clean");
        QTest::newRow("Property assignment for clean") << (QStringList("clean") << "profile:x");
        QTest::newRow("Property assignment for dump-nodes-tree")
                << (QStringList("dump-nodes-tree") << "profile:x");
        QTest::newRow("Property assignment for status") << (QStringList("status") << "profile:x");
        QTest::newRow("Property assignment for update-timestamps")
                << (QStringList("update-timestamps") << "profile:x");
        QTest::newRow("Argument for show-version")
                << (QStringList("show-version") << "config:debug");
    }

private:
    QTemporaryFile m_projectFile;
    QStringList m_fileArgs;
};

QTEST_MAIN(TestCmdLineParser)

#include "tst_cmdlineparser.moc"
