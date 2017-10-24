/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "application.h"
#include "commandlinefrontend.h"
#include "qbstool.h"
#include "parser/commandlineparser.h"
#include "../shared/logging/consolelogger.h"

#include <qbs.h>

#include <QtCore/qtimer.h>
#include <cstdlib>

using namespace qbs;

static bool tryToRunTool(const QStringList &arguments, int &exitCode)
{
    if (arguments.empty())
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
        QTimer::singleShot(0, &clFrontend, &CommandLineFrontend::start);
        return app.exec();
    } catch (const ErrorInfo &error) {
        qbsError() << error.toString();
        return EXIT_FAILURE;
    }
}
