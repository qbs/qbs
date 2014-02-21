/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "commandlineparser.h"
#include "probe.h"
#include "../shared/qbssettings.h"

#include <logging/translator.h>
#include <tools/error.h>

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>

using qbs::Internal::Tr;

static void printUsage(const QString &usageString)
{
    std::cout << qPrintable(usageString) << std::endl;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    CommandLineParser clParser;
    try {
        clParser.parse(app.arguments());
        if (clParser.helpRequested()) {
            printUsage(clParser.usageString());
            return EXIT_SUCCESS;
        }
        SettingsPtr settings = qbsSettings(clParser.settingsDir());
        if (clParser.autoDetectionMode()) {
            probe(settings.data());
            return EXIT_SUCCESS;
        }
        createProfile(clParser.profileName(), clParser.toolchainType(), clParser.compilerPath(),
                      settings.data());
    } catch (const qbs::ErrorInfo &e) {
        std::cerr << qPrintable(e.toString()) << std::endl;
        return EXIT_FAILURE;
    }
}
