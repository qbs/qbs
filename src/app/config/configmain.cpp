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

#include "configcommandlineparser.h"
#include "configcommandexecutor.h"

#include <logging/translator.h>
#include <tools/error.h>
#include <tools/settings.h>

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>

using qbs::Internal::Tr;
using qbs::Settings;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    ConfigCommandLineParser parser;
    try {
        parser.parse(app.arguments().mid(1));
        if (parser.helpRequested()) {
            std::cout << qPrintable(Tr::tr("This tool manages qbs settings.")) << std::endl;
            parser.printUsage();
            return EXIT_SUCCESS;
        }
        Settings settings(parser.settingsDir());
        ConfigCommandExecutor(&settings).execute(parser.command());
    } catch (const qbs::ErrorInfo &e) {
        std::cerr << qPrintable(e.toString()) << std::endl;
        parser.printUsage();
        return EXIT_FAILURE;
    }
}
