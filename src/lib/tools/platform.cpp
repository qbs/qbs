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

#include "platform.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QStringList>
#include <QtCore/QDebug>
#include <QtCore/QTextStream>
#include <QtCore/QSettings>

#ifdef Q_OS_WIN
#   include <qt_windows.h>
#   ifndef _WIN32_IE
#       define _WIN32_IE 0x0400    // for MinGW
#   endif
#   include <Shlobj.h>
#endif

namespace qbs {

Platform::Platform(const QString &_name, const QString& configpath)
    : name(_name)
    , settings(configpath + "/setup.ini", QSettings::IniFormat)
{
}

QHash<QString, Platform::Ptr> Platform::platforms()
{
    QString localSettingsPath = configBaseDir();
    QHash<QString, Platform::Ptr> targets;
    QDirIterator i(localSettingsPath, QDir::Dirs | QDir::NoDotAndDotDot);
    while (i.hasNext()) {
        i.next();
        Platform *t = new Platform(i.fileName(), i.filePath());
        targets.insert(t->name, Platform::Ptr(t));
    }
    return targets;
}

QString Platform::configBaseDir()
{
#if defined(Q_OS_LINUX) || defined(Q_OS_MAC)
    QString localSettingsPath = QDir::homePath() + "/.config/Nokia/qbs/platforms";
#elif defined(Q_OS_WIN)
    QString localSettingsPath;
    wchar_t wszPath[MAX_PATH];
    if (SHGetSpecialFolderPath(NULL, wszPath, CSIDL_APPDATA, TRUE))
        localSettingsPath = QString::fromUtf16(reinterpret_cast<ushort*>(wszPath)) + "/qbs/platforms";
#else
#error port me!
#endif
    return localSettingsPath;
}

} // namespace qbs
