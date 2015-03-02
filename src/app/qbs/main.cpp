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

#include "application.h"
#include "commandlinefrontend.h"
#include "qbstool.h"
#include "parser/commandlineparser.h"
#include "../shared/logging/consolelogger.h"

#include <qbs.h>

#include <QTimer>
#include <cstdlib>

using namespace qbs;

static bool tryToRunTool(const QStringList &arguments, int &exitCode)
{
    if (arguments.isEmpty())
        return false;
    QStringList toolArgs = arguments;
    const QString toolName = toolArgs.takeFirst();
    if (toolName.startsWith(QLatin1Char('-')))
        return false;
    return QbsTool::tryToRunTool(toolName, toolArgs, &exitCode);
}

int main(int argc, char *argv[])
{
    ConsoleLogger::instance();

    try {
        Application app(argc, argv);
        QStringList arguments = app.arguments();
        arguments.removeFirst();

        int toolExitCode = 0;
        if (tryToRunTool(arguments, toolExitCode))
            return toolExitCode;

        CommandLineParser parser;
        if (!parser.parseCommandLine(arguments))
            return EXIT_FAILURE;

        if (parser.command() == HelpCommandType) {
            parser.printHelp();
            return 0;
        }

        Settings settings(parser.settingsDir());
        ConsoleLogger::instance().setSettings(&settings);
        CommandLineFrontend clFrontend(parser, &settings);
        app.setCommandLineFrontend(&clFrontend);
        QTimer::singleShot(0, &clFrontend, SLOT(start()));
        return app.exec();
    } catch (const ErrorInfo &error) {
        qbsError() << error.toString();
        return EXIT_FAILURE;
    }
}
