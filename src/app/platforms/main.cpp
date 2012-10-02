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
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QSettings>
#include <QStringList>
#include <QTextStream>

#include <tools/platform.h>
#include <tools/settings.h>

#ifdef Q_OS_UNIX
#include <iostream>
#include <termios.h>
#endif

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shlobj.h>
#endif

#include <cstdio>

void showUsage()
{
    QTextStream s(stderr);
    s   << "platform [action]\n"
        << "actions:\n"
        << "  s|shell <name>                     --  open a shell, setting up the named platform\n"
        << "  ls|list                            --  list available platforms\n"
        << "  mv|rename <from> <to>              --  rename a platform\n"
        << "  rm|remove <name>                   --  irrevocably remove the given target\n"
        << "  config <name> [<key>] [<value>]    --  show or change configuration\n"
        << "  probe                              --  probe the current environment\n"
        << "                                         and construct qbs platforms for each compiler found\n"
        << "  print-config-base-dir              --  prints the base dir of the platform configurations\n"
        ;
}

int probe(const QString &settingsPath, QHash<QString, qbs::Platform::Ptr> &platforms);

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTextStream qstdout(stdout);

    qbs::Settings::Ptr settings = qbs::Settings::create();
    QString defaultPlatform = settings->value("modules/qbs/platform").toString();

    QString localSettingsPath = qbs::Platform::configBaseDir();
    if (!localSettingsPath.endsWith(QLatin1Char('/')))
        localSettingsPath.append(QLatin1Char('/'));
    QDir().mkpath(localSettingsPath);

    enum Action {
        ListPlatform,
        ProbePlatform,
        RenamePlatform,
        RemovePlatform,
        ShellPlatform,
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
        } else if (cmd == "shell" || cmd == "s") {
            action = ShellPlatform;
        } else if (cmd == "config") {
            action = ConfigPlatform;
        } else if (cmd == "list" || cmd == "ls") {
            action = ListPlatform;
        } else if (cmd == "print-config-base-dir") {
            puts(qPrintable(QDir::toNativeSeparators(qbs::Platform::configBaseDir())));
            return 0;
        } else {
            showUsage();
            return 3;
        }
    }

    QHash<QString, qbs::Platform::Ptr> platforms;
    QDirIterator i(localSettingsPath, QDir::Dirs | QDir::NoDotAndDotDot);
    while (i.hasNext()) {
        i.next();
        qbs::Platform::Ptr platform(new qbs::Platform(i.fileName(), i.filePath()));
        platforms.insert(platform->name, platform);
    }

    if (action == ListPlatform) {
        qstdout << "Platforms:\n";
        foreach (qbs::Platform::Ptr platform, platforms.values()) {
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
            qDebug("cannot rename: no such target: %s", qPrintable(from));
            return 5;
        }
        QString to = arguments.takeFirst();
        if (platforms.contains(to)) {
            qDebug("cannot rename: already exists: %s", qPrintable(to));
            return 5;
        }
        if (!QFile(localSettingsPath + from).rename(localSettingsPath + to)) {
            qDebug("file error moving %s to %s",
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
            qDebug("cannot remove: no such target: %s", qPrintable(targetName));
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
            fprintf(stderr, "Unknown platform '%s'.", qPrintable(platformName));
            return 5;
        }
        qbs::Platform::Ptr p = platforms.value(platformName);
        if (arguments.count()) {
            QString key = arguments.takeFirst();
            if (arguments.count()) {
                QString value = arguments.takeFirst();
                p->settings.setValue(key, value);
            }
            if (!p->settings.contains(key)) {
                qDebug("no such configuration key: %s", qPrintable(key));
                return 7;
            }
            qstdout << p->settings.value(key).toString() << "\n";
        } else {
            foreach (const QString &key, p->settings.allKeys()) {
                qstdout << key << "=" << p->settings.value(key).toString() << "\n";
            }
        }

    } else if (action == ProbePlatform) {
        bool firstRun = platforms.isEmpty();
        probe(localSettingsPath, platforms);
        if (firstRun && !platforms.isEmpty()) {
            settings->setValue(qbs::Settings::Global, "modules/qbs/platform", platforms.values().at(0)->name);
        }
    }
    return 0;
}
