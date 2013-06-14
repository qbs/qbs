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
#include "setupqt.h"

#include "../shared/logging/consolelogger.h"
#include "../shared/qbssettings.h"

#include <logging/translator.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QtDebug>
#include <QStringList>

using namespace qbs;
using Internal::Tr;

static void printWrongQMakePath(const QString &qmakePath)
{
    qbsError() << QCoreApplication::translate("SetupQt", "Invalid path to qmake: %1").arg(qmakePath);
}

static void printUsage(const QString &appName)
{
    qbsInfo() << Tr::tr("Usage: %1 --detect | <qmake path> [<profile name>]").arg(appName);
    qbsInfo() << Tr::tr("Use --detect to use all qmake executables found "
                        "via the PATH environment variable.");
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    SettingsPtr settings = qbsSettings();
    ConsoleLogger::instance(settings.data());

    QStringList args = application.arguments();
    const QString appName = QFileInfo(args.takeFirst()).fileName();
    try {
        if (args.isEmpty()) {
            printUsage(appName);
            return EXIT_FAILURE;
        }
        if (args.count() == 1 && (args.first() == QLatin1String("--help")
                                  || args.first() == QLatin1String("-h"))) {
            qbsInfo() << Tr::tr("This tool creates qbs profiles from Qt versions.");
            printUsage(appName);
            return EXIT_SUCCESS;
        }

        if (args.count() == 1 && args.first() == QLatin1String("--detect")) {
            // search all Qt's in path and dump their settings
            QList<QtEnvironment> qtEnvironments = SetupQt::fetchEnvironments();
            foreach (const QtEnvironment &qtEnvironment, qtEnvironments) {
                QString profileName = QLatin1String("qt-") + qtEnvironment.qtVersion;
                if (SetupQt::checkIfMoreThanOneQtWithTheSameVersion(qtEnvironment.qtVersion, qtEnvironments)) {
                    QStringList prefixPathParts = qtEnvironment.installPrefixPath.split("/", QString::SkipEmptyParts);
                    if (!prefixPathParts.isEmpty())
                        profileName += QLatin1String("-") + prefixPathParts.last();
                }
                SetupQt::saveToQbsSettings(profileName, qtEnvironment, settings.data());
            }
            return EXIT_SUCCESS;
        }
        if (args.count() == 1) {
            QString qmakePath = args.first();
            if (!SetupQt::isQMakePathValid(qmakePath)) {
                printWrongQMakePath(qmakePath);
                return 1;
            }
            QtEnvironment qtEnvironment = SetupQt::fetchEnvironment(qmakePath);
            QString profileName = QLatin1String("qt-") + qtEnvironment.qtVersion;
            profileName.replace(".", "-");
            SetupQt::saveToQbsSettings(profileName , qtEnvironment, settings.data());
            return EXIT_SUCCESS;
        }
        if (args.count() == 2) {
            QString qmakePath = args.first();
            if (!SetupQt::isQMakePathValid(qmakePath)) {
                printWrongQMakePath(qmakePath);
                return 1;
            }
            QtEnvironment qtEnvironment = SetupQt::fetchEnvironment(qmakePath);
            QString profileName = args.at(1);
            profileName.replace(".", "-");
            SetupQt::saveToQbsSettings(profileName , qtEnvironment, settings.data());
            return EXIT_SUCCESS;
        }
        printUsage(appName);
        return EXIT_FAILURE;
    } catch (const ErrorInfo &e) {
        qbsError() << Tr::tr("%1: %2").arg(appName, e.toString());
        return EXIT_FAILURE;
    }

    return 0;
}
