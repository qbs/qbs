/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#include <QCoreApplication>
#include <QtDebug>
#include <QStringList>

#include <iostream>

#include "setupqt.h"

static void printWrongQMakePath(const QString &qmakePath)
{
    std::cerr << QCoreApplication::translate("SetupQt", "Invalid path to qmake: %1").arg(qmakePath).toStdString() << std::endl;
}

static void printHelp()
{
    std::cout << QCoreApplication::translate("SetupQt", "usage: qbs-setup-qt [qmake path [profile name]]").toStdString() << std::endl;
    std::cout << QCoreApplication::translate("SetupQt", "If you omit all arguments all qmakes in the PATH will be searched.").toStdString() << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);

    if (argc == 1) { // search all Qt's in path and dump their settings
        QList<qbs::QtEnviroment> qtEnvironments = qbs::SetupQt::fetchEnviroments();

        foreach (const qbs::QtEnviroment &qtEnvironment, qtEnvironments) {
            QString profileName = QLatin1String("qt-") + qtEnvironment.qtVersion;
            if (qbs::SetupQt::checkIfMoreThanOneQtWithTheSameVersion(qtEnvironment.qtVersion, qtEnvironments))
                profileName += QLatin1String("-") + qtEnvironment.installPrefixPath.split("/").last();
            qbs::SetupQt::saveToQbsSettings(profileName , qtEnvironment);
        }
    } else if (argc == 2) {
        if (application.arguments()[1] == QLatin1String("--help") || application.arguments()[1] == QLatin1String("-h")) {
            printHelp();
        } else {
            QString qmakePath = application.arguments()[1];
            if (!qbs::SetupQt::isQMakePathValid(qmakePath)) {
                printWrongQMakePath(qmakePath);
                return 1;
            }
            qbs::QtEnviroment qtEnvironment = qbs::SetupQt::fetchEnviroment(qmakePath);
            QString profileName = QLatin1String("qt-") + qtEnvironment.qtVersion;
            profileName.replace(".", "-");
            qbs::SetupQt::saveToQbsSettings(profileName , qtEnvironment);
        }
    } else if (argc == 3) {
        QString qmakePath = application.arguments()[1];
        if (!qbs::SetupQt::isQMakePathValid(qmakePath)) {
            printWrongQMakePath(qmakePath);
            return 1;
        }
        qbs::QtEnviroment qtEnvironment = qbs::SetupQt::fetchEnviroment(qmakePath);
        QString profileName = application.arguments()[2];
        profileName.replace(".", "-");
        qbs::SetupQt::saveToQbsSettings(profileName , qtEnvironment);
    } else {
        printHelp();
    }

    return 0;
}
