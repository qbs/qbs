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
#include <tools/platform.h>
#include <tools/settings.h>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QSettings>
#include <QStringList>
#include <QTextStream>

#ifdef Q_OS_UNIX
#include <iostream>
#include <termios.h>
#endif

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shlobj.h>
#endif

#include <cstdio>

using namespace qbs;

static void showUsage()
{
    QTextStream s(stderr);
    s   << "platform [action]\n"
        << "actions:\n"
        << "  ls|list                            --  list available platforms\n"
        << "  mv|rename <from> <to>              --  rename a platform\n"
        << "  rm|remove <name>                   --  irrevocably remove the given target\n"
        << "  config <name> [<key>] [<value>]    --  show or change configuration\n"
        << "  probe                              --  probe the current environment\n"
        << "                                         and construct qbs platforms for each compiler found\n"
        << "  print-config-base-dir              --  prints the base dir of the platform configurations\n"
        ;
}

static QString toInternalSeparators(const QString &variable)
{
    QString transformedVar = variable;
    return transformedVar.replace(QLatin1Char('.'), QLatin1Char('/'));
}

static QString toExternalSeparators(const QString &variable)
{
    QString transformedVar = variable;
    return transformedVar.replace(QLatin1Char('/'), QLatin1Char('.'));
}

int probe(const QString &settingsPath, QHash<QString, Platform::Ptr> &platforms);

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTextStream qstdout(stdout);

    ConsoleLogger cl;

    Settings settings;
    QString defaultPlatform = settings.value("modules/qbs/platform").toString();

    QString localSettingsPath = Platform::configBaseDir();
    if (!localSettingsPath.endsWith(QLatin1Char('/')))
        localSettingsPath.append(QLatin1Char('/'));
    QDir().mkpath(localSettingsPath);

    enum Action {
        ListPlatform,
        ProbePlatform,
        RenamePlatform,
        RemovePlatform,
        ConfigPlatform
    };

    Action action = ListPlatform;

    QStringList arguments = app.arguments();
    arguments.takeFirst();
    if (arguments.count()) {
        QString cmd = arguments.takeFirst();
        if (cmd == "probe") {
            action = ProbePlatform;
        } else if (cmd == "rename" || cmd == "mv") {
            action = RenamePlatform;
        } else if (cmd == "rm" || cmd == "remove") {
            action = RemovePlatform;
        } else if (cmd == "config") {
            action = ConfigPlatform;
        } else if (cmd == "list" || cmd == "ls") {
            action = ListPlatform;
        } else if (cmd == "print-config-base-dir") {
            puts(qPrintable(QDir::toNativeSeparators(Platform::configBaseDir())));
            return 0;
        } else {
            showUsage();
            return 3;
        }
    }

    QHash<QString, Platform::Ptr> platforms;
    QDirIterator i(localSettingsPath, QDir::Dirs | QDir::NoDotAndDotDot);
    while (i.hasNext()) {
        i.next();
        Platform::Ptr platform(new Platform(i.fileName(), i.filePath()));
        platforms.insert(platform->name, platform);
    }

    if (action == ListPlatform) {
        qstdout << "Platforms:\n";
        foreach (Platform::Ptr platform, platforms.values()) {
            qstdout << "\t- " << platform->name;
            if (platform->name == defaultPlatform)
                qstdout << " (default)";
            qstdout << " "<< platform->settings.value("target-triplet").toString() << "\n";
        }
    } else if (action == RenamePlatform) {
        if (arguments.count() < 2) {
            showUsage();
            return 3;
        }
        QString from = arguments.takeFirst();
        if (!platforms.contains(from)) {
            qbsError("Cannot rename: No such target '%s'.", qPrintable(from));
            return 5;
        }
        QString to = arguments.takeFirst();
        if (platforms.contains(to)) {
            qbsError("Cannot rename: Target '%s' already exists.", qPrintable(to));
            return 5;
        }
        if (!QFile(localSettingsPath + from).rename(localSettingsPath + to)) {
            qbsError("File error moving '%s'' to '%s'.",
                    qPrintable(localSettingsPath + from),
                    qPrintable(localSettingsPath + to)
                    );
            return 5;
        }
        platforms.insert(to, platforms.take(from));
    } else if (action == RemovePlatform) {
        if (arguments.count() < 1) {
            showUsage();
            return 3;
        }
        QString targetName = arguments.takeFirst();
        if (!platforms.contains(targetName)) {
            qbsError("Cannot remove: No such target '%s'.", qPrintable(targetName));
            return 5;
        }
        QDirIterator i1(localSettingsPath + targetName,
                QDir::Files | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden,
                QDirIterator::Subdirectories);
        while (i1.hasNext()) {
            i1.next();
            QFile(i1.filePath()).remove();
        }
        QDirIterator i2(localSettingsPath + targetName,
                QDir::Dirs| QDir::NoDotAndDotDot | QDir::System | QDir::Hidden,
                QDirIterator::Subdirectories);
        while (i2.hasNext()) {
            i2.next();
            QDir().rmdir(i2.filePath());
        }
        QDir().rmdir(localSettingsPath + targetName);
        platforms.remove(targetName);
    } else if (action == ConfigPlatform) {
        if (arguments.count() < 1) {
            showUsage();
            return 3;
        }
        QString platformName = arguments.takeFirst();
        if (!platforms.contains(platformName)) {
            qbsError("Unknown platform '%s'.", qPrintable(platformName));
            return 5;
        }
        Platform::Ptr p = platforms.value(platformName);
        if (arguments.count()) {
            const QString key = toInternalSeparators(arguments.takeFirst());
            if (arguments.count()) {
                QString value = arguments.takeFirst();
                p->settings.setValue(key, value);
            }
            if (!p->settings.contains(key)) {
                qbsError("No such configuration key '%s'.",
                        qPrintable(toExternalSeparators(key)));
                return 7;
            }
            qstdout << p->settings.value(key).toString() << "\n";
        } else {
            foreach (const QString &key, p->settings.allKeys()) {
                qstdout << toExternalSeparators(key)
                        << "=" << p->settings.value(key).toString() << "\n";
            }
        }

    } else if (action == ProbePlatform) {
        bool firstRun = platforms.isEmpty();
        probe(localSettingsPath, platforms);
        if (firstRun && !platforms.isEmpty()) {
            settings.setValue(QLatin1String("modules/qbs/platform"),
                              platforms.values().at(0)->name);
        }
    }
    return 0;
}
