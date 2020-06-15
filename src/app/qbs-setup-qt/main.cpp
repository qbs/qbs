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
#include "setupqt.h"

#include "commandlineparser.h"

#include <logging/translator.h>
#include <tools/qttools.h>
#include <tools/settings.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qstringlist.h>

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
        settings.setScopeForWriting(clParser.settingsScope());

        if (clParser.autoDetectionMode()) {
            // search all Qt's in path and dump their settings
            const std::vector<QtEnvironment> qtEnvironments = SetupQt::fetchEnvironments();
            if (qtEnvironments.empty()) {
                std::cout << qPrintable(Tr::tr("No Qt installations detected. "
                                               "No profiles created."))
                          << std::endl;
            }
            for (const QtEnvironment &qtEnvironment : qtEnvironments) {
                QString profileName = QLatin1String("qt-") + qtEnvironment.qtVersion.toString();
                if (SetupQt::checkIfMoreThanOneQtWithTheSameVersion(qtEnvironment.qtVersion, qtEnvironments)) {
                    QStringList prefixPathParts = QFileInfo(qtEnvironment.qmakeFilePath).path()
                            .split(QLatin1Char('/'), QBS_SKIP_EMPTY_PARTS);
                    if (!prefixPathParts.empty())
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

        const QtEnvironment qtEnvironment = SetupQt::fetchEnvironment(clParser.qmakePath());
        QString profileName = clParser.profileName();
        profileName.replace(QLatin1Char('.'), QLatin1Char('-'));
        SetupQt::saveToQbsSettings(profileName, qtEnvironment, &settings);
        return EXIT_SUCCESS;
    } catch (const ErrorInfo &e) {
        std::cerr << qPrintable(e.toString()) << std::endl;
        return EXIT_FAILURE;
    }
}
