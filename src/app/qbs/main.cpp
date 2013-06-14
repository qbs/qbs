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

#include "application.h"
#include "commandlinefrontend.h"
#include "qbstool.h"
#include "parser/commandlineparser.h"
#include "../shared/logging/consolelogger.h"
#include "../shared/qbssettings.h"

#include <qbs.h>

#include <QTimer>

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
    SettingsPtr settings = qbsSettings();
    ConsoleLogger::instance(settings.data());

    try {
        Application app(argc, argv);
        QStringList arguments = app.arguments();
        arguments.removeFirst();

        int toolExitCode = 0;
        if (tryToRunTool(arguments, toolExitCode))
            return toolExitCode;

        CommandLineParser parser;
        if (!parser.parseCommandLine(arguments, settings.data()))
            return EXIT_FAILURE;

        if (parser.command() == HelpCommandType) {
            parser.printHelp();
            return 0;
        }

        CommandLineFrontend clFrontend(parser, settings.data());
        app.setCommandLineFrontend(&clFrontend);
        QTimer::singleShot(0, &clFrontend, SLOT(start()));
        return app.exec();
    } catch (const ErrorInfo &error) {
        qbsError() << error.toString();
        return EXIT_FAILURE;
    }
}
