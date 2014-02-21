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
#include "setupqt.h"

#include "../shared/qbssettings.h"
#include "commandlineparser.h"

#include <qtprofilesetup.h>
#include <logging/translator.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QStringList>

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

        SettingsPtr settings = qbsSettings(clParser.settingsDir());

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
                SetupQt::saveToQbsSettings(profileName, qtEnvironment, settings.data());
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
        SetupQt::saveToQbsSettings(profileName, qtEnvironment, settings.data());
        return EXIT_SUCCESS;
    } catch (const ErrorInfo &e) {
        std::cerr << qPrintable(e.toString()) << std::endl;
        return EXIT_FAILURE;
    }
}
