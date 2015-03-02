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
#include "setupqt.h"

#include "commandlineparser.h"

#include <qtprofilesetup.h>
#include <logging/translator.h>
#include <tools/settings.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QStringList>

#include <cstdlib>
#include <iostream>

using namespace qbs;
using Internal::Tr;

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);

    try {
        CommandLineParser clParser;
        clParser.parse(application.arguments());

        if (clParser.helpRequested()) {
            std::cout << qPrintable(clParser.usageString()) << std::endl;
            return EXIT_SUCCESS;
        }

        Settings settings(clParser.settingsDir());

        if (clParser.autoDetectionMode()) {
            // search all Qt's in path and dump their settings
            QList<QtEnvironment> qtEnvironments = SetupQt::fetchEnvironments();
            foreach (const QtEnvironment &qtEnvironment, qtEnvironments) {
                QString profileName = QLatin1String("qt-") + qtEnvironment.qtVersion;
                if (SetupQt::checkIfMoreThanOneQtWithTheSameVersion(qtEnvironment.qtVersion, qtEnvironments)) {
                    QStringList prefixPathParts = qtEnvironment.installPrefixPath
                            .split(QLatin1Char('/'), QString::SkipEmptyParts);
                    if (!prefixPathParts.isEmpty())
                        profileName += QLatin1String("-") + prefixPathParts.last();
                }
                SetupQt::saveToQbsSettings(profileName, qtEnvironment, &settings);
            }
            return EXIT_SUCCESS;
        }

        if (!SetupQt::isQMakePathValid(clParser.qmakePath())) {
            std::cerr << qPrintable(Tr::tr("'%1' does not seem to be a qmake executable.")
                                    .arg(clParser.qmakePath())) << std::endl;
            return EXIT_FAILURE;
        }

        QtEnvironment qtEnvironment = SetupQt::fetchEnvironment(clParser.qmakePath());
        QString profileName = clParser.profileName();
        profileName.replace(QLatin1Char('.'), QLatin1Char('-'));
        SetupQt::saveToQbsSettings(profileName, qtEnvironment, &settings);
        return EXIT_SUCCESS;
    } catch (const ErrorInfo &e) {
        std::cerr << qPrintable(e.toString()) << std::endl;
        return EXIT_FAILURE;
    }
}
