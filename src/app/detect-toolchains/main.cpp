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

#include "probe.h"
#include "../shared/logging/consolelogger.h"
#include "../shared/qbssettings.h"

#include <logging/translator.h>
#include <tools/error.h>

#include <QCoreApplication>

#include <cstdlib>

using qbs::Internal::Tr;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    SettingsPtr settings = qbsSettings();
    ConsoleLogger::instance(settings.data());
    const QStringList args = app.arguments().mid(1);
    if (args.count() == 1 && (args.first() == QLatin1String("--help")
                              || args.first() == QLatin1String("-h"))) {
        qbsInfo() << Tr::tr("This tool tries to auto-detect known toolchains in your system.\n"
                            "It takes no command-line parameters.");
        return EXIT_SUCCESS;
    }

    if (!args.isEmpty()) {
        qbsWarning() << Tr::tr("You supplied command-line parameters, "
                               "but this tool does not use any.");
    }

    try {
        probe(settings.data());
        return EXIT_SUCCESS;
    } catch (const qbs::ErrorInfo &error) {
        qbsError() << Tr::tr("Probing for toolchains failed: %1").arg(error.toString());
        return EXIT_FAILURE;
    }
}
