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
#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QStringList>
#include <QtCore/QDebug>
#include <QtCore/QTextStream>
#include <QtCore/QSettings>

#include <tools/platform.h>
#include <tools/settings.h>

#ifdef Q_OS_UNIX
#include <iostream>
#include <termios.h>
#endif

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <Shlobj.h>
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

static int ask(const QString &msg, const QString &choices);
static QString prompt(const QString &msg);

int probe (const QString &settingsPath,
        QHash<QString, qbs::Platform*> &platforms,
        int (* ask)(const QString &msg, const QString &choices),
        QString ( *prompt)(const QString &msg)
        );

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTextStream qstdout(stdout);

    qbs::Settings::Ptr settings = qbs::Settings::create();
    QString defaultPlatform = settings->value("defaults/platform").toString();

    QString localSettingsPath = qbs::Platform::configBaseDir();
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

    QHash<QString, qbs::Platform*> targets;
    QDirIterator i(localSettingsPath, QDir::Dirs | QDir::NoDotAndDotDot);
    while (i.hasNext()) {
        i.next();
        qbs::Platform *t = new qbs::Platform(i.fileName(), i.filePath());
        targets.insert(t->name, t);
    }

    if (action == ListPlatform) {
        qstdout << "Platforms:\n";
        foreach (qbs::Platform *t, targets.values()) {
            qstdout << "\t- " << t->name;
            if (t->name == defaultPlatform)
                qstdout << " (default)";
            qstdout << " "<< t->settings.value("target-triplet").toString() << "\n";
        }
    } else if (action == RenamePlatform) {
        if (arguments.count() < 2) {
            showUsage();
            return 3;
        }
        QString from = arguments.takeFirst();
        if (!targets.contains(from)) {
            qDebug("cannot rename: no such target: %s", qPrintable(from));
            return 5;
        }
        QString to = arguments.takeFirst();
        if (targets.contains(to)) {
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
        targets.insert(to, targets.take(from));
    } else if (action == RemovePlatform) {
        if (arguments.count() < 1) {
            showUsage();
            return 3;
        }
        QString targetName = arguments.takeFirst();
        if (!targets.contains(targetName)) {
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
        delete targets.take(targetName);
    } else if (action == ConfigPlatform) {
        if (arguments.count() < 1) {
            showUsage();
            return 3;
        }
        QString targetName= arguments.takeFirst();
        if (!targets.contains(targetName)) {
            qDebug("no such target: %s", qPrintable(targetName));
            return 5;
        }
        qbs::Platform *p = targets.value(targetName);
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
        bool firstRun = targets.isEmpty();
        probe(localSettingsPath, targets, ask, prompt);
        if (firstRun && !targets.isEmpty()) {
            settings->setValue(qbs::Settings::Global, "defaults/platform", targets.values().at(0)->name);
        }
    }
    return 0;
}


static int ask(const QString &msg, const QString &choices)
{
#ifdef Q_OS_UNIX
    termios stored_settings;
    tcgetattr(0, &stored_settings);
    termios new_settings = stored_settings;
    new_settings.c_lflag &= (~ICANON);
    new_settings.c_lflag &= (~ECHO); // don't echo the character
    // apply the new settings
    tcsetattr(0, TCSANOW, &new_settings);
#endif

    setvbuf ( stdin , NULL , _IONBF , 0 );


    QTextStream qstdout(stdout);
    qstdout << msg << " (";

    QHash<QChar, int> cs;

    for (int i = 0; i < choices.count(); i++) {
        QChar c = choices.at(i);
        cs.insert(c.toLower(), i);
        if (i == 0)
            qstdout  << c.toUpper();
        else
            qstdout  << c.toLower();
        if (i != choices.count() -1)
            qstdout  << "/";
    }
    qstdout  << ") " << flush;

    while (true) {
        QChar i = QChar(getc(stdin)).toLower();
        if (i == '\n' || i == '\r') {
#ifdef Q_OS_UNIX
            tcsetattr(0, TCSANOW, &stored_settings);
#endif
            qstdout  << choices.at(0).toUpper() << "\n" << flush;
            return 0;
        }
        if (cs.contains(i)) {
#ifdef Q_OS_UNIX
            tcsetattr(0, TCSANOW, &stored_settings);
#endif
            qstdout  << i << "\n" << flush;
            return cs.value(i);
        }
    }
}

static QString prompt(const QString &msg)
{
    QTextStream qstdout(stdout);
    qstdout << msg << " "<< flush;
    QTextStream qstdin(stdin);
    QString s;
    qstdin >> s;
    return s;
}
