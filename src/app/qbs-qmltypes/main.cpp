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
#include "../shared/logging/consolelogger.h"

#include <api/languageinfo.h>
#include <logging/translator.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qbytearray.h>

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
