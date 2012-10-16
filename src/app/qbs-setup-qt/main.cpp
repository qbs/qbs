/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#include <logging/consolelogger.h>

#include <QCoreApplication>
#include <QtDebug>
#include <QStringList>

#include <iostream>

#include "setupqt.h"

static void printWrongQMakePath(const QString &qmakePath)
{
    qbs::qbsError() << QCoreApplication::translate("SetupQt", "Invalid path to qmake: %1").arg(qmakePath);
}

static void printHelp()
{
    std::cout << QCoreApplication::translate("SetupQt", "usage: qbs-setup-qt [qmake path [profile name]]").toStdString() << std::endl;
    std::cout << QCoreApplication::translate("SetupQt", "With --detect all qmakes in the PATH will be searched.").toStdString() << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    qbs::ConsoleLogger cl;

    try {
        if (argc == 1) {
            printHelp();
            return 0;
        } else if (argc == 2) {
            if (application.arguments()[1] == QLatin1String("--help") || application.arguments()[1] == QLatin1String("-h")) {
                printHelp();
            } else if (application.arguments()[1] == QLatin1String("--detect")) {
                // search all Qt's in path and dump their settings
                QList<qbs::QtEnviroment> qtEnvironments = qbs::SetupQt::fetchEnviroments();

                foreach (const qbs::QtEnviroment &qtEnvironment, qtEnvironments) {
                    QString profileName = QLatin1String("qt-") + qtEnvironment.qtVersion;
                    if (qbs::SetupQt::checkIfMoreThanOneQtWithTheSameVersion(qtEnvironment.qtVersion, qtEnvironments))
                        profileName += QLatin1String("-") + qtEnvironment.installPrefixPath.split("/").last();
                    qbs::SetupQt::saveToQbsSettings(profileName, qtEnvironment);
                }
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
    } catch (const qbs::Exception &e) {
        qbs::qbsError() << "qbs-setup-qt: " << qPrintable(e.message());
        return EXIT_FAILURE;
    }

    return 0;
}
