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

#include "qtprofilesetup.h"

#include <logging/translator.h>
#include <tools/error.h>
#include <tools/profile.h>
#include <tools/settings.h>

#include <QDir>
#include <QFile>
#include <QRegExp>
#include <QTextStream>

namespace qbs {

static QString guessMinimumWindowsVersion(const QtEnvironment &qt)
{
    if (qt.mkspecName.startsWith("winrt-"))
        return QLatin1String("6.2");

    if (!qt.mkspecName.startsWith("win32-"))
        return QString();

    if (qt.architecture == QLatin1String("x86_64")
            || qt.architecture == QLatin1String("ia64")) {
        return QLatin1String("5.2");
    }

    QRegExp rex(QLatin1String("^win32-msvc(\\d+)$"));
    if (rex.exactMatch(qt.mkspecName)) {
        int msvcVersion = rex.cap(1).toInt();
        if (msvcVersion < 2012)
            return QLatin1String("5.0");
        else
            return QLatin1String("5.1");
    }

    return qt.qtMajorVersion < 5 ? QLatin1String("5.0") : QLatin1String("5.1");
}

ErrorInfo setupQtProfile(const QString &profileName, Settings *settings,
                         const QtEnvironment &qtEnvironment)
{
    Profile profile(profileName, settings);
    const QString settingsTemplate(QLatin1String("Qt.core.%1"));
    profile.setValue(settingsTemplate.arg("config"), qtEnvironment.configItems);
    profile.setValue(settingsTemplate.arg("qtConfig"), qtEnvironment.qtConfigItems);
    profile.setValue(settingsTemplate.arg("binPath"), qtEnvironment.binaryPath);
    profile.setValue(settingsTemplate.arg("libPath"), qtEnvironment.libraryPath);
    profile.setValue(settingsTemplate.arg("pluginPath"), qtEnvironment.pluginPath);
    profile.setValue(settingsTemplate.arg("incPath"), qtEnvironment.includePath);
    profile.setValue(settingsTemplate.arg("mkspecPath"), qtEnvironment.mkspecPath);
    profile.setValue(settingsTemplate.arg("docPath"), qtEnvironment.documentationPath);
    profile.setValue(settingsTemplate.arg("version"), qtEnvironment.qtVersion);
    profile.setValue(settingsTemplate.arg("namespace"), qtEnvironment.qtNameSpace);
    profile.setValue(settingsTemplate.arg("libInfix"), qtEnvironment.qtLibInfix);
    profile.setValue(settingsTemplate.arg("buildVariant"), qtEnvironment.buildVariant);
    if (qtEnvironment.staticBuild)
        profile.setValue(settingsTemplate.arg(QLatin1String("staticBuild")),
                         qtEnvironment.staticBuild);

    // Set the minimum operating system versions appropriate for this Qt version
    const QString windowsVersion = guessMinimumWindowsVersion(qtEnvironment);
    QString osxVersion, iosVersion, androidVersion;

    if (qtEnvironment.mkspecPath.contains("macx")) {
        profile.setValue(settingsTemplate.arg("frameworkBuild"), qtEnvironment.frameworkBuild);
        if (qtEnvironment.qtMajorVersion >= 5) {
            osxVersion = QLatin1String("10.6");
        } else if (qtEnvironment.qtMajorVersion == 4 && qtEnvironment.qtMinorVersion >= 6) {
            QDir qconfigDir;
            if (qtEnvironment.frameworkBuild) {
                qconfigDir.setPath(qtEnvironment.libraryPath);
                qconfigDir.cd("QtCore.framework/Headers");
            } else {
                qconfigDir.setPath(qtEnvironment.includePath);
                qconfigDir.cd("Qt");
            }
            QFile qconfig(qconfigDir.absoluteFilePath("qconfig.h"));
            if (qconfig.open(QIODevice::ReadOnly)) {
                bool qtCocoaBuild = false;
                QTextStream ts(&qconfig);
                QString line;
                do {
                    line = ts.readLine();
                    if (QRegExp(QLatin1String("\\s*#define\\s+QT_MAC_USE_COCOA\\s+1\\s*"),
                                Qt::CaseSensitive).exactMatch(line)) {
                        qtCocoaBuild = true;
                        break;
                    }
                } while (!line.isNull());

                if (ts.status() == QTextStream::Ok)
                    osxVersion = qtCocoaBuild ? QLatin1String("10.5") : QLatin1String("10.4");
            }

            if (osxVersion.isEmpty()) {
                return ErrorInfo(Internal::Tr::tr("Error reading qconfig.h; could not determine "
                                                  "whether Qt is using Cocoa or Carbon"));
            }
        }

        if (qtEnvironment.configItems.contains("c++11"))
            osxVersion = QLatin1String("10.7");
    }

    if (qtEnvironment.mkspecPath.contains("ios") && qtEnvironment.qtMajorVersion >= 5)
        iosVersion = QLatin1String("5.0");

    if (qtEnvironment.mkspecPath.contains("android")) {
        if (qtEnvironment.qtMajorVersion >= 5)
            androidVersion = QLatin1String("2.3");
        else if (qtEnvironment.qtMajorVersion == 4 && qtEnvironment.qtMinorVersion >= 8)
            androidVersion = QLatin1String("1.6"); // Necessitas
    }

    // ### TODO: wince, winphone, blackberry

    if (!windowsVersion.isEmpty())
        profile.setValue(QLatin1String("cpp.minimumWindowsVersion"), windowsVersion);

    if (!osxVersion.isEmpty())
        profile.setValue(QLatin1String("cpp.minimumOsxVersion"), osxVersion);

    if (!iosVersion.isEmpty())
        profile.setValue(QLatin1String("cpp.minimumIosVersion"), iosVersion);

    if (!androidVersion.isEmpty())
        profile.setValue(QLatin1String("cpp.minimumAndroidVersion"), androidVersion);

    return ErrorInfo();
}

} // namespace qbs
