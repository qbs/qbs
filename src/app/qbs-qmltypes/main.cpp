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
#include "../shared/logging/consolelogger.h"

#include <api/languageinfo.h>
#include <logging/translator.h>

#include <QCoreApplication>
#include <QByteArray>

#include <cstdlib>
#include <iostream>

using qbs::Internal::Tr;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    ConsoleLogger::instance();

    const QStringList args = app.arguments().mid(1);
    if (args.count() == 1 && (args.first() == QLatin1String("--help")
                              || args.first() == QLatin1String("-h"))) {
        qbsInfo() << Tr::tr("This tool dumps information about the QML types supported by qbs.\n"
                            "It takes no command-line parameters.\n"
                            "The output is intended to be processed by other tools and has "
                            "little value for humans.");
        return EXIT_SUCCESS;
    }
    if (!args.isEmpty()) {
        qbsWarning() << Tr::tr("You supplied command-line parameters, "
                               "but this tool does not use any.");
    }

    qbs::LanguageInfo languageInfo;
    const QByteArray typeData = languageInfo.qmlTypeInfo();

    std::cout << typeData.constData();

    return 0;
}
